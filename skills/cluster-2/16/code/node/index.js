const { SerialPort } = require("serialport");
const { writeFile } = require("fs");

// Buffer to accumulate incoming data
let buffer = '';
const newLine = '\n'; // Assuming each message ends with a newline

// Initialize SerialPort with correct path and baud rate
const port = new SerialPort({
  path: "/dev/cu.usbserial-02650595", // Adjust as needed
  baudRate: 115200, // Ensure this matches your ESP32 setup
  autoOpen: true,
});

// Handle port open event
port.on("open", () => {
  console.log("Serial Port Opened");
});

// Handle incoming data
port.on("data", (data) => {
  buffer += data.toString(); // Accumulate data in the buffer

  let boundary;
  while ((boundary = buffer.indexOf(newLine)) !== -1) {
    const rawString = buffer.slice(0, boundary).trim(); // Extract one message
    buffer = buffer.slice(boundary + newLine.length); // Remove processed message

    try {
      const values = parseAccelerometerData(rawString); // Parse the raw data
      console.log("Parsed Data:", values);

      const { x, y, z } = values;

      // Calculate roll and pitch
      const roll = Math.atan2(y, z) * (180 / Math.PI);
      const pitch = Math.atan2(-x, Math.sqrt(y * y + z * z)) * (180 / Math.PI);

      console.log(`Roll: ${roll.toFixed(2)}\tPitch: ${pitch.toFixed(2)}`);

      // Append data to CSV
      const csvStr = `${x},${y},${z},${roll.toFixed(2)},${pitch.toFixed(2)}\n`;
      writeFile("accelerometer_data.csv", csvStr, { flag: "a" }, (err) => {
        if (err) console.error("File Write Error:", err);
      });

    } catch (err) {
      console.error("Error parsing data:", err.message); // Handle parsing errors
    }
  }
});

// Function to parse raw accelerometer data from plain text
function parseAccelerometerData(rawString) {
  // Regular expression to match "X: <value> Y: <value> Z: <value>"
  const regex = /X:\s*(-?\d+\.\d+)\s+Y:\s*(-?\d+\.\d+)\s+Z:\s*(-?\d+\.\d+)/;
  const match = rawString.match(regex);

  if (!match) throw new Error("Invalid accelerometer data format");

  // Parse and return the values as floats
  return {
    x: parseFloat(match[1]),
    y: parseFloat(match[2]),
    z: parseFloat(match[3]),
  };
}

// Handle port errors
port.on("error", (err) => {
  console.error("Serial Port Error:", err.message);
});

// Handle port close event
port.on("close", () => {
  console.log("Serial Port Closed");
});
