#  Skill Name

Author: FirstName LastName

Date: YYYY-MM-DD

### Summary

In this exercise, I'm delving into the realm of UART communication between the ESP32 microcontroller and a PC via a USB interface. UART (Universal Asynchronous Receiver-Transmitter) serves as a foundational method for serial communication, crucial for transmitting data and commands between embedded systems and external devices like terminal emulators on PCs. The primary goal is to develop a program that showcases bidirectional data exchange capabilities. This involves configuring UART settings, handling multiple characters sent to and from the ESP32, and implementing distinct operational modes based on user input. These modes include toggling an onboard LED, echoing multiple characters back to the console, and converting decimal numbers into hexadecimal format for display.

Code Explanation:
The provided code initializes necessary libraries and configurations for FreeRTOS, UART, GPIO, and other ESP-IDF components essential for ESP32 development. UART 0 is configured with specific TX and RX pin assignments using menuconfig settings to ensure proper communication setup. Hardware flow control is disabled to simplify the data exchange process. Within the main program loop, UART input is continuously polled (uart_read_bytes) to detect and respond to commands sent from the terminal emulator on the PC. Depending on the received command ('s' key), the program switches between different modes:

  Mode 1: Controls LED toggling based on 't' key input.
  Mode 2: Echoes multiple characters entered on the keyboard back to the console.
  Mode 3: Converts decimal numbers provided as input into hexadecimal and echoes the result.

This exercise not only demonstrates practical UART configuration and usage but also reinforces fundamental programming concepts such as data type handling, character array manipulation, and the implementation of state-based behavior using switch statements in an embedded systems context.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
    <a href="https://youtu.be/6lFgKpskqQU">
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



