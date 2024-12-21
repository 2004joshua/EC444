#  Skill Name

Author: Joshua Arrevillaga

Date: 2024-12-02

### Summary

This project focuses on controlling TT motors using an ESP32 microcontroller, allowing for precise movement through PWM (Pulse Width Modulation) signals. The motors are connected via a dual H-bridge motor driver, which enables bidirectional control for forward, reverse, turning, and spinning motions. By utilizing the ESP32's LEDC peripheral, the motors' speed is adjusted by varying the duty cycle of the PWM signals. This setup also integrates GPIO pins to control the direction of each motor independently.

To implement this, the motor_init function initializes the GPIO pins for motor direction and configures the LEDC module for PWM-based speed control. The PWM signal is set to a frequency of 50 Hz, with a 10-bit resolution for fine-grained control over motor speed. Two LEDC channels are assigned to the left and right motors, allowing independent control of each motor's speed. Directional control is achieved by setting the appropriate GPIO pins high or low, determining the polarity applied to the motor's terminals.

The test_motor_states function demonstrates various motor control states, including forward, backward, turning, and spinning. Each state is tested sequentially with a fixed duration, providing a clear demonstration of the ESP32's ability to control motor speed and direction. This comprehensive approach ensures that the motors respond correctly to all commands, making the setup robust for a variety of applications, such as robotics or automated vehicles.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<a href="https://drive.google.com/file/d/11ausswCABU8g2841hc2pd7mBtGwpmc-K/view?usp=drive_link"> Skill 30: Purple car </a>

<p align="center">
<img src="./images/ece444.png" width="50%">
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



