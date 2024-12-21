const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const fs = require('fs');
const http = require('http');
const socketIo = require('socket.io');

// Configure Serial Port
const port = new SerialPort({
  path: '/dev/tty.usbserial-01746A42', // Update to your actual serial port path
  baudRate: 115200,
});

// Create a line parser for serial data
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

// Set up HTTP server and Socket.io
const server = http.createServer((req, res) => {
  // Serve the client HTML
  if (req.url === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(`
      <!DOCTYPE html>
      <html>
      <head>
          <title>Live Sensor Data</title>
          <script src="/socket.io/socket.io.js"></script>
          <script src="https://canvasjs.com/assets/script/canvasjs.min.js"></script>
          <style>
              body {
                  font-family: Arial, sans-serif;
                  margin: 20px;
              }
          </style>
      </head>
      <body>
          <h1>Live Sensor Data</h1>
          <div id="chartContainer" style="height: 370px; width: 100%;"></div>
          <script>
              const socket = io();
              const dataPointsTemp = [];
              const dataPointsAccelMag = [];

              const chart = new CanvasJS.Chart("chartContainer", {
                  title: { text: "Temperature and Acceleration Magnitude" },
                  axisX: { title: "Time" },
                  axisY: { title: "Values" },
                  legend: {
                      cursor: "pointer",
                      itemclick: (e) => {
                          // Toggle data series visibility
                          e.dataSeries.visible = typeof e.dataSeries.visible === "undefined" || e.dataSeries.visible;
                          chart.render();
                      },
                  },
                  data: [
                      {
                          type: "line",
                          showInLegend: true,
                          name: "Temperature (°C)",
                          dataPoints: dataPointsTemp,
                      },
                      {
                          type: "line",
                          showInLegend: true,
                          name: "Acceleration Magnitude (m/s²)",
                          dataPoints: dataPointsAccelMag,
                      },
                  ],
              });

              socket.on('sensorData', (data) => {
                  const time = new Date(); // Use current time as X-axis
                  dataPointsTemp.push({ x: time, y: data.temp });
                  dataPointsAccelMag.push({ x: time, y: data.accelMagnitude });

                  // Limit points to 20 for better visualization
                  if (dataPointsTemp.length > 20) dataPointsTemp.shift();
                  if (dataPointsAccelMag.length > 20) dataPointsAccelMag.shift();

                  chart.render();
              });
          </script>
      </body>
      </html>
    `);
  } else {
    // Handle other requests with a 404
    if (!res.headersSent) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('404 Not Found');
    }
  }
});

const io = socketIo(server);

// File stream to save data
const fileStream = fs.createWriteStream('sensor_data.json', { flags: 'a' });

// Handle incoming serial data
parser.on('data', (line) => {
  try {
    const data = JSON.parse(line); // Parse the JSON string
    console.log(data); // Log the parsed data to the console

    // Calculate the magnitude of the acceleration
    const accelMagnitude = Math.sqrt(
      Math.pow(data.xAccel, 2) +
      Math.pow(data.yAccel, 2) +
      Math.pow(data.zAccel, 2)
    );

    // Add the calculated magnitude to the data object
    data.accelMagnitude = accelMagnitude;

    // Send the updated data to the browser client via Socket.io
    io.emit('sensorData', data);

    // Save the data to the file as a JSON string
    fileStream.write(`${JSON.stringify(data)}\n`);
  } catch (error) {
    console.error('Failed to parse JSON:', error, 'Line:', line);
  }
});

// Start the server
server.listen(3000, () => {
  console.log('Server is running at http://localhost:3000');
});
