const fs = require("fs");
const { Server } = require("socket.io");
const { SerialPort } = require("serialport");

const PORT = process.env.PORT || 3000;

const serial = new SerialPort({
  path: "/dev/ttyUSB0",
  baudRate: 115200,
});

// Create a new server instance
const io = new Server(PORT, {
  cors: {
    origin: "*",
  },
});

// When a client connects, send the data
io.on("connection", (socket) => {
  console.log("Client connected...");
});

serial.on("data", (data) => {
  const dataStr = data.toString();
  try {
    const dataObj = JSON.parse(dataStr);
    io.emit("data", dataObj);
  } catch (e) {}
});

console.log(`Server running on http://localhost:${PORT}`);
