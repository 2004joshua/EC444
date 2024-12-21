# Code Overview 

## ESP32 Code
Directory: code/esp_code/
This directory contains the ESP32 firmware code, which manages the voting fobs, communication protocols, and leader election logic.

**`main/`**
  - **`fob.c:`** This is the core file containing all the ESP32 code. It implements:
    -Initialization of Wi-Fi, UART, and LED peripherals.
    -State machine logic for leader election (using the Bully Algorithm).
    -Tasks for handling vote casting, IR communication, and heartbeat monitoring.
    -UDP communication to send and receive votes and leader election messages.
    -LED indicators for device states.

## Node.js Server Code
Directory: **`code/nodejs/`**
This directory contains the server-side code responsible for handling incoming votes, managing the database, and serving the web interface.

**`main.js`**
  - A basic implementation for testing socket communication. Listens on a specified port and processes incoming data.
**`server.js`**
  - The primary server application that:
  - Handles UDP communication with ESP32 devices.
  - Logs vote data into the database.
  - Provides REST APIs for retrieving and resetting vote data.
  - Integrates with Socket.IO for real-time updates to the web interface.

**`src/`**

**`index.html:`** 
  - Contains the web interface code, built with HTML for user interaction. It displays vote counts and logs.

**`clientside.js:`** 
  - Miscellaneous JavaScript code used for front-end functionality.

**`db/`**
- **`votes.tingo:`** 
  - The TingoDB database file used for storing vote records persistently.
  
## Reminders
- Describe here any software you have adopted from elsewhere by citation or URL
- Your code should include a header with your team member names and date


