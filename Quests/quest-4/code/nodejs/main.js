const express = require('express');
const path = require('path');
const http = require('http');
const { Server } = require('socket.io');
const app = express();
const PORT = 3000;

app.use(express.static(path.join(__dirname, 'src')));

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'src', 'index.html'));
});

const server = http.createServer(app);
const io = new Server(server);

let voteCounts = {
    Red: 12,
    Green: 8,
    Blue: 15
};

// Emit the vote counts to clients every time they connect
io.on('connection', (socket) => {
    console.log('A user connected');
    socket.emit('update-votes', voteCounts);
});

// Start the server
server.listen(PORT, () => {
    console.log(`Server is running on http://localhost:${PORT}`);
});
