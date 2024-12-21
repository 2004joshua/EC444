#  Skill Name

Author: Joshua Arrevillaga 

Date: 2024-10-06

### Summary

This code snippet sets up an ESP32 or ESP8266 microcontroller to communicate with an ADXL343 accelerometer sensor via the I2C protocol. It includes necessary header files and libraries for standard C operations and ESP-IDF specific functionalities. The I2C configuration initializes the master mode and sets GPIO pins for data (SDA) and clock (SCL) lines, enabling communication at a defined frequency. Constants are defined for I2C operations such as write and read bits, and mechanisms for acknowledging or ignoring acknowledgments (ACK/NACK). The getDeviceID() function exemplifies interaction with the ADXL343 by establishing an I2C command sequence to query the sensor's device ID. This approach serves as a foundational step for more advanced sensor interactions, such as retrieving acceleration data along different axes and implementing interrupt-driven functionalities based on sensor thresholds.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">

[![Watch the video](https://img.youtube.com/vi/dz-Ua3bSfV4/0.jpg)](https://youtu.be/G8wF9GtQ328)

</p>

Or

- [Link to video demo](). Not to exceed 10s

### AI and Open Source Code Assertions

- I have documented in my code readme.md and in my code any
software that we have adopted from elsewhere
- I used AI for coding and this is documented in my code as
indicated by comments "AI generated" 



