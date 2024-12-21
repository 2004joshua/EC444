const fs = require("fs");
const http = require("http");
const { Server } = require("socket.io");
const path = require("path");

const PORT = process.env.PORT || 3000;

// Create an HTTP server
const server = http.createServer((req, res) => {
  if (req.url === "/") {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end("<h1>Socket.io Server Running</h1>");
  } else {
    res.writeHead(404);
    res.end("Not Found");
  }
});

// Attach Socket.io to the server
const io = new Server(server, {
  cors: {
    origin: "*",
  },
});

// Function to read CSV
const readCSVFile = (filePath) => {
  const lines = fs.readFileSync(filePath, "utf8").split("\n");
  const headers = lines[0].split(",");
  const data = [];
  for (let i = 1; i < lines.length; i++) {
    const line = lines[i].split(",");
    const obj = {};
    for (let j = 0; j < headers.length; j++) {
      obj[headers[j]] = line[j];
    }
    data.push(obj);
  }
  return data;
};

// When a client connects, send data
io.on("connection", (socket) => {
  console.log("Client connected - sending data...");

  // Adjust the file path to match your directory structure
  const filePath = path.join(__dirname, "..", "stocks.csv"); // Move up one level
  if (fs.existsSync(filePath)) {
    const data = readCSVFile(filePath);
    socket.emit("data", data);
    console.log("Data sent to the client");
  } else {
    console.error("stocks.csv not found at:", filePath);
    socket.emit("error", "CSV file not found");
  }
});

// Start the server
server.listen(PORT, () => {
  console.log(`Data server running on http://localhost:${PORT}`);
});
