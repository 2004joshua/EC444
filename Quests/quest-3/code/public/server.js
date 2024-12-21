const dgram = require('dgram');
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');

// Create UDP Server
const udpServer = dgram.createSocket('udp4');
const client = dgram.createSocket('udp4');

// Store ESP data
const espData = {};

// Create Express server for front-end
const app = express();
const server = http.createServer(app);
const io = socketIo(server);

const PORT_UDP = 33333; // UDP port
const PORT_HTTP = 3000; // HTTP port

const ESP32_HOSTS = [ // Replace with the target ESP32 IP addresses
    '192.168.1.139',
    '192.168.1.124',
    '192.168.1.119',
    '192.168.1.134',
    '192.168.1.106'
];
const ESP32_PORT = 33333; // Port ESP32 is listening on

// Serve static files (HTML, CSS, JS)
app.use(express.static('public'));

// Store previous leaders
let previousLeaders = {
    resting: null,
    active: null,
    highly_active: null,
};

// Function to send commands to all ESP32 devices
function sendBlinkCommand(blinkTime) {
    const command = `SET_BLINK_TIME:${blinkTime}`;
    const message = Buffer.from(command);

    ESP32_HOSTS.forEach((host) => {
        client.send(message, ESP32_PORT, host, (err) => {
            if (err) {
                console.error(`Error sending message to ${host}:`, err);
            } else {
                console.log(`Command sent to ${host}: ${command}`);
            }
        });
    });
}

// Function to handle leader change
function handleLeaderChange(metric, leader) {
    console.log(`Leader changed for ${metric}: ${leader}`);
    
    // Set blink time to 100 for 5 seconds
    sendBlinkCommand(100);

    // Reset blink time to 0 after 5 seconds
    setTimeout(() => {
        sendBlinkCommand(0);
    }, 5000);
}

// Handle incoming UDP messages
udpServer.on('message', (msg) => {
    try {
        const data = JSON.parse(msg.toString());
        const { device_name, resting_time, active_time, highly_active_time } = data;

        if (!device_name) return;

        // Update data store
        espData[device_name] = { resting_time, active_time, highly_active_time };

        // Calculate leaders for each metric
        const leaders = {
            resting: { device_name: null, maxTime: 0 },
            active: { device_name: null, maxTime: 0 },
            highly_active: { device_name: null, maxTime: 0 },
        };

        for (const [device, times] of Object.entries(espData)) {
            if (times.resting_time > leaders.resting.maxTime) {
                leaders.resting = { device_name: device, maxTime: times.resting_time };
            }
            if (times.active_time > leaders.active.maxTime) {
                leaders.active = { device_name: device, maxTime: times.active_time };
            }
            if (times.highly_active_time > leaders.highly_active.maxTime) {
                leaders.highly_active = { device_name: device, maxTime: times.highly_active_time };
            }
        }

        // Check for leader changes
        for (const metric of ['resting', 'active', 'highly_active']) {
            if (leaders[metric].device_name !== previousLeaders[metric]) {
                previousLeaders[metric] = leaders[metric].device_name;
                handleLeaderChange(metric, leaders[metric].device_name);
            }
        }

        // Emit updated data and leaders to front-end
        io.emit('update', { espData, leaders });
    } catch (err) {
        console.error('Error parsing UDP message:', err);
    }
});

// Handle UDP errors
udpServer.on('error', (err) => {
    console.error('UDP Server Error:', err);
    udpServer.close();
});

// Start UDP Server
udpServer.bind(PORT_UDP, () => {
    console.log(`UDP server listening on port ${PORT_UDP}`);
});

// Start Express HTTP Server
server.listen(PORT_HTTP, () => {
    console.log(`HTTP server running at http://localhost:${PORT_HTTP}`);
});
