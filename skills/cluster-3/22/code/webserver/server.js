const dgram = require('dgram');
const server = dgram.createSocket('udp4');

const HOST = '192.168.1.109'; // Laptop's IP
const PORT = 3333;            // Server's listening port

server.on('listening', () => {
    const address = server.address();
    console.log(`UDP Server is listening on ${address.address}:${address.port}`);
});

server.on('message', (message, remote) => {
    console.log(`Received message from ${remote.address}:${remote.port} - ${message}`);

    if (message.toString().includes("Requesting blink time")) {
        const response = Buffer.from('Blink time: 500ms');
        server.send(response, remote.port, remote.address, (error) => {
            if (error) {
                console.log('Failed to send response:', error);
            } else {
                console.log('Sent blink time response to ESP32');
            }
        });
    }
});

server.bind(PORT, HOST);
