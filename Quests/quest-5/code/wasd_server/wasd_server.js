/*

  - Joshua Arrevillaga
  - Michael Barany
  - Samuel Kraft
  
*/


const dgram = require("dgram");
const keypress = require("keypress");

// ESP32 Configuration
const ESP32_IP = "192.168.0.225";
const ESP32_PORT = 3333;

// Create UDP Socket
const server = dgram.createSocket("udp4");

// Function to send a command
function sendCommand(command) {
  if (!command) return;
  const message = Buffer.from(command);
  server.send(message, ESP32_PORT, ESP32_IP, (err) => {
    if (err) {
      console.error("Error sending message:", err.message);
    } else {
      console.log(`Sent '${command}' to ESP32 (${ESP32_IP}:${ESP32_PORT})`);
    }
  });
}

// Handle keypress events
function handleKeypress(ch, key) {
  if (!key) return;
  if (['w', 'a', 's', 'd', 'e', '0'].includes(key.name)) {
    sendCommand(key.name);
  } else if (key.name === 'space') {
    sendCommand('0'); // Stop command
  }
}

// Enable keypress events
keypress(process.stdin);
process.stdin.setRawMode(true);
process.stdin.resume();

// Listen for keypress events
process.stdin.on("keypress", (ch, key) => {
  if (key && key.ctrl && key.name === "c") {
    process.exit(); // Exit on Ctrl+C
  }
  handleKeypress(ch, key);
});

// Handle server errors
server.on("error", (err) => {
  console.error("UDP Error:", err.message);
});

// Bind the server
server.bind(() => {
  console.log("UDP client ready!");
  console.log("Press 'w', 'a', 's', 'd', or 'e' to control the car. Press SPACE or '0' to stop.");
});

// Clean up on exit
process.on("exit", () => {
  console.log("Closing UDP client...");
  server.close();
});