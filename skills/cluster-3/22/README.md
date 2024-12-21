#  Skill Name

Author: Joshua Arrevillaga

Date: 2024-10-24

### Summary

In this skill, I implemented bidirectional UDP communication between a laptop and an ESP32 over a WiFi network. Initially, I set up a UDP server on the laptop using a Node.js application and a UDP client on the ESP32, enabling the ESP32 to send a request for LED blink time to the server. The server responded with a new blink time, which the ESP32 used to adjust its onboard LED blinking interval. Next, I reversed the roles, configuring the laptop as a UDP client and the ESP32 as a UDP server. The laptop sent a command to turn on a GPIO LED on the ESP32, and the ESP32 acknowledged the command by lighting up the LED and sending a confirmation response back to the laptop. This exercise demonstrated successful bidirectional communication and control, showcasing the ESP32's ability to interact seamlessly with other devices over a wireless network.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
<img src="./images/ece444.png" width="50%">

<a href="https://youtu.be/AgAybL4OT8s"> Skill 22 video </a>
</p>

<p align="center">
Template for Including Graphics
</p>

Or

- [Link to video demo](). Not to exceed 10s

### AI and Open Source Code Assertions

- I have documented in my code readme.md and in my code any
software that we have adopted from elsewhere
- I used AI for coding and this is documented in my code as
indicated by comments "AI generated" 



