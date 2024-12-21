# Code Readme
---
### Files

##### **`./Quest_3`**
- **`./public`**  Node.js server script for:

    - **`index.html`**  Provides the structure for the web dashboard, used to display activity data and leader status.

    - **`script.js`**  Front-end JavaScript for:
      - Dynamically updating the dashboard in real-time.
        - Managing UDP communication with ESP32 devices.
        - Processing incoming data.
        - Identifying leaders and serving a real-time web dashboard.
      - Visualizing activity data using **Socket.IO** and **Chart.js**.


- **`./Main`**

  - **`ADX343.h`**  Header file for the ADXL343 accelerometer. Includes register definitions, constants, and I2C configuration.
  
  - **`CMakeLists.txt`**  Build configuration script for compiling the ESP32 firmware.
  
  - **`char_binary.h`**  Contains utility functions and definitions for handling binary character representations in data parsing and transmission.
  
  - **`hello_world_main.c`**  Main source code for the ESP32 firmware. Implements Wi-Fi connectivity, accelerometer data processing, and UDP communication with the server.
  
  - **`package-lock.json`**  Automatically generated file to lock Node.js dependency versions for consistent builds.
  
  - **`package.json`**  Metadata and dependency file for the Node.js server, including dependencies like Express and Socket.IO.
  
  - **`pins.h`**  Defines GPIO pin assignments for ESP32, such as I2C pins, LED control, and button inputs.
---

## **Configuration Settings**

### **ESP32 Settings**
- **Wi-Fi**: 
  - SSID and password defined in the firmware.
- **UDP Communication**:
  - Server IP (`UDP_SERVER_IP`) and port (`UDP_SERVER_PORT`).
- **I2C Pins**:
  - `SCL`: GPIO22, `SDA`: GPIO23.
- **GPIO Pins**:
  - LED: GPIO25, Button: A2.
- **Accelerometer**:
  - I2C Address: `0x53`.
  - Range: ±16G.

### **Node.js Server**
- **UDP Port**: `33333` (default for ESP32 communication).
- **HTTP Port**: `3000` (dashboard access).
- **ESP32 Device IPs**:
  - Defined in the `ESP32_HOSTS` array in the server code.
    
---

* ChatGPT was used as referance in the development of this code


