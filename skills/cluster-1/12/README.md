#  Skill Name

Author: Joshua Arrevillaga 

Date: 2024-09-20

### Summary

The skill I'm demonstrating involves using interrupts on the ESP32 microcontroller within a FreeRTOS environment. Interrupts are essential for handling asynchronous events like button presses efficiently without continuously polling, which is crucial for conserving power in embedded systems. By configuring a GPIO pin to trigger an interrupt on a button press (in this case, on a rising edge), I enable the microcontroller to respond immediately to external inputs while efficiently managing power during idle periods. This capability is particularly valuable in applications requiring real-time responsiveness, such as IoT devices, sensors, and control systems.

In implementing the interrupt-driven LED toggling program, I first set up GPIO pins for LEDs and a pushbutton, adhering to ESP32-specific configurations using the FreeRTOS framework. The LEDs were configured as outputs, and the pushbutton pin was set up to generate interrupts on a specific edge condition. Within the app_main function, I initialized the GPIO settings, ensuring all LEDs started in an off state. The interrupt service routine (button_isr_handler), marked with IRAM_ATTR for ESP32's IRAM (Internal RAM), was defined to toggle through LEDs sequentially each time the button was pressed. This involved turning off the currently lit LED, updating the index to the next LED, and turning it on. This setup effectively demonstrated the use of interrupts to handle user inputs promptly and efficiently, showcasing the power and versatility of interrupt-driven programming on microcontrollers like the ESP32.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
    <a href="https://youtu.be/HFg0wAicQbQ">
        <img src="./images/click.png" alt="Thumbnail of your video" />
    </a>
</p>

Or

- [Link to video demo](). Not to exceed 10s

### AI and Open Source Code Assertions

- I have documented in my code readme.md and in my code any
software that we have adopted from elsewhere
- I used AI for coding and this is documented in my code as
indicated by comments "AI generated" 



