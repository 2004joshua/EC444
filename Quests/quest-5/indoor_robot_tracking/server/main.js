const Engine = require('tingodb')();
const dgram = require('dgram');
const express = require('express');
const path = require('path');

// Create a TingoDB database
const db = new Engine.Db(path.join(__dirname, '../database'), {});
const positions = db.collection("positions");

// UDP Client setup
const udpClient = dgram.createSocket('udp4');
const UDP_PORT = 41234; // Replace with your UDP server port
const UDP_HOST = '192.168.0.167'; 
// HTTP API setup
const app = express();
const HTTP_PORT = 3000;

// Log position data to TingoDB
function logPosition(id, x, z, theta, status) {
    positions.insert({
        time: new Date().toISOString(),
        id,
        x,
        z,
        theta,
        status
    });
}

// UDP listener to query position data for multiple robots
const robotIds = [13, 14]; // Add all robot IDs here
setInterval(() => {
    robotIds.forEach((robotId) => {
        const message = Buffer.from(`ROBOTID ${robotId}`);
        udpClient.send(message, UDP_PORT, UDP_HOST, (err) => {
            if (err) console.error(`Error sending request for Robot ${robotId}:`, err);
        });
    });
}, 1000);

udpClient.on('message', (msg, rinfo) => {
    const data = msg.toString();
    const [id, x, z, theta, status] = data.split(',');
    logPosition(Number(id), parseFloat(x), parseFloat(z), parseFloat(theta), status);
    console.log(`Logged position: ${data}`);
});

// HTTP API endpoint to fetch data
app.get('/positions', (req, res) => {
    positions.find().toArray((err, items) => {
        if (err) return res.status(500).send(err);
        res.json(items);
    });
});

// Start the HTTP server
app.listen(HTTP_PORT, () => {
    console.log(`HTTP server running at http://localhost:${HTTP_PORT}`);
});
