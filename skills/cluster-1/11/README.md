#  Skill Name

Author: Joshua Arrevillaga

Date: 2024-09-20

### Summary

In this skill, I'm leveraging FreeRTOS on the ESP32 microcontroller to implement multitasking capabilities using real-time operating system principles. FreeRTOS allows tasks to run concurrently, each with its own priority and execution context, facilitating efficient handling of tasks with different timing requirements. This skill focuses on designing three separate tasks: one to count up in binary using LEDs, another to toggle counting direction based on a button press, and a third to display the current counting direction on an LED. Tasks communicate via global variables, showcasing inter-task communication without mutual exclusion mechanisms initially, which will be addressed in future iterations.

The code initializes GPIO pins for LEDs (LED_1_GPIO, LED_2_GPIO, LED_3_GPIO) and a pushbutton (BUTTON_GPIO) using FreeRTOS and ESP32-specific configurations. Task A, implemented with a higher priority, controls the LEDs to count up in binary every second. Task B monitors the pushbutton state and toggles a global variable (direction) to switch counting directions. Task C reads the direction variable and displays "up" or "down" on a blue LED (LED_DIR). Tasks are created using xTaskCreate with appropriate stack sizes and priorities to ensure real-time responsiveness. This setup demonstrates effective multitasking in embedded systems, highlighting the flexibility and efficiency of using an RTOS like FreeRTOS on microcontrollers.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
    <a href="https://youtu.be/_x1szhR15j4">
        <img src="./images/click.png" alt="Thumbnail of your video" />
    </a>
</p>


### AI and Open Source Code Assertions

- I have documented in my code readme.md and in my code any
software that we have adopted from elsewhere
- I used AI for coding and this is documented in my code as
indicated by comments "AI generated" 



