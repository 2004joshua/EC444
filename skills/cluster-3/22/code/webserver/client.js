const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const ESP32_HOST = '192.168.1.139'; // IP address of the ESP32
const ESP32_PORT = 4444;            // Port ESP32 is listening on

const message = Buffer.from('Requesting blink time'); // Message to send

// Send the message to the ESP32
client.send(message, ESP32_PORT, ESP32_HOST, (error) => {
    if (error) {
        console.error('Error sending message:', error);
        client.close();
    } else {
        console.log('Message sent to ESP32');
    }
});

// Listen for any response from the ESP32
client.on('message', (msg, rinfo) => {
    console.log(`Received response from ESP32: ${msg}`);
    client.close(); // Close the client after receiving a response
});
