# Code Readme

This readme should explain the contents of the code folder and subfolders
Make it easy for us to navigate this space.

This code is designed for an ESP32 microcontroller and involves controlling four LEDs and a button to create a simple counting system. The app_main function initializes the GPIO pins and starts three tasks: check_button, update_dir_led, and count. The check_button task monitors a button's state, toggling the counting direction between incrementing (up) and decrementing (down) when pressed. The update_dir_led task updates an LED to indicate the current counting direction, either on for down or off for up. The count task manages the counting process, adjusting a 4-bit value between 0 and 15 based on the direction, and updates the LEDs accordingly. Each bit of the current count value controls one LED, which is updated by calling the set_count function to set the appropriate LED states. The code also provides visual feedback via console print statements to show the current count and LED states.

## Reminders
- Describe here any software you have adopted from elsewhere by citation or URL
- Your code should include a header with your team member names and date
