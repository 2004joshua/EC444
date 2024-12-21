#  Skill Name

Author: Joshua Arrevillaga

Date: YYYY-MM-DD

### Summary


For this assignment, I connected an ESP32 to send live sensor data over a UART connection to a Node.js application. The ESP32 was configured to gather data from two sensors, formatted as space-separated values for easier parsing, and sent to the serial port at one-second intervals. The Node.js app utilized the serialport package to read incoming data, parse it into JSON format, and calculate any derived values, such as the magnitude of acceleration. These processed values were then streamed to a web client using socket.io.

On the front end, I used CanvasJS to create a live visualization of the data. A dynamically updating line chart displayed the two sensor streams against time, ensuring the user could monitor changes in real-time. The chart was configured to handle a limited number of data points to maintain smooth rendering while keeping the display focused on the most recent activity. The Node.js server served this visualization alongside the live data via a simple HTTP setup, making the system user-friendly and accessible.

In addition to visualization, I implemented data logging to store the sensor readings persistently. Using Node.js's fs module, the application appended each parsed data entry to a local file in JSON format. This dual functionality of real-time streaming and persistent storage ensures that the data is both immediately actionable and available for offline analysis, setting a solid foundation for integration with databases or advanced data processing pipelines in the future.

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
<img src="./images/ece444.png" width="50%">
</p>

<a href="https://youtube.com/shorts/yQHk4VD6RqM"> Skill 21 </a>
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



