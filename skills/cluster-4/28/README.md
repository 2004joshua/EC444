#  Skill Name

Author: Joshua Arrevillaga

Date: 2024-11-04

### Summary

The Leader (Coordinator) Election skill involves implementing the Bully Algorithm to elect a leader in a distributed system. The goal is to ensure that only one leader is elected at any given time, all devices recognize the leader, and a new leader is elected if the current leader fails. The algorithm involves devices exchanging messages to decide the leader, ensuring that the election process completes in a finite time.

In this skill, devices communicate via UDP to exchange election messages. Each device runs the same code, transitioning through states such as leader, follower, and timeout. The system uses LEDs to indicate the device’s current state, with blue representing the leader, green for followers, and red for timeout situations. The Bully Algorithm ensures that if the leader fails, a new election is triggered, and a new leader is chosen.

The goal is to demonstrate a functioning system where multiple devices boot up, elect a leader, handle leader failure and recovery, and display the new leader. This skill showcases fault tolerance and distributed decision-making, with a focus on managing leader elections in real-time.

in my code file the only one for the esp is in the main folder

### Evidence of Completion
- Attach a photo or upload a video that captures a demonstration of
  your solution. Include in the photo/video your BU ID.

<p align="center">
<img src="./images/ece444.png" width="50%">
</p>

<a href="https://youtu.be/4UxdZjaKW_M"> Skill 28 </a>

<p align="center">
<img src="./images/id.png" width="50%">
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



