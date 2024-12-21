# Quest Name

Authors: Alicja Mahr, Micheal Barany, Joshua Arrevillaga, Samuel Kraft

Date: 2024-09-20

### Summary

This quest was all about getting familiar with sensors, console input and output, and a button for hardware interrupts.
 After learning each of these skills we had to put everything together into one large circuit that would be able to do the full functionality of a smart pill (Read temperature, light levels, battery level, tilt, and status indication through LEDs). For sensors, we used a Thermistor sensor to measure temperature, a photocell to measure brightness value,
 and a tilt sensor to measure orientation. We then had to program 3 LEDs to communicate data and the console to communicate sensor data.
 After all was said and done, we had a working circuit that measured light, temperature, and battery level and a console and LEDs that communicated the data and interrupts.


### Solution Design

Our complete circuit included three Leds, two buttons, a thermistor, a photocell and a voltage divider which was comprised of 2 resistors. 
We use 8 resistors total, one with each component (6 total) and the voltage divider (2). 

[Link to picture of physical circuit](https://drive.google.com/file/d/1HUChAW9dqXs34ibSk9fsFzk43WnZ4zts/view?usp=sharing)  
[Link to picture of circuit diagram](https://drive.google.com/file/d/1Ry27ptrM6BA4X_h-__XO_w5o6JkACi1Q/view?usp=sharing)

### Quest Summary
Everything in our circuit works properly except the tilt sensor. When it was programmed, it never registered the tilt despite 
building and flashing properly the value would not change even after shaking the circuit. Another challenge we had was with converting the photocell sensor data to lux but that was able to be solved.


### Supporting Artifacts
- [Link to video technical presentation](https://drive.google.com/file/d/1jBouaftE9pEM7VwiZ81oSCZ3NBQ4OCbZ/view?usp=sharing)
- [Link to video demo](https://drive.google.com/file/d/1WmUmwAQOGqnJe2UNXVCsEsp_7uYFdxs0/view?usp=sharing)

### Self-Assessment 

| Objective Criterion | Rating | Max Value  | 
|---------------------------------------------|:-----------:|:---------:|
| Objective One |  |  1    1 | 
| Objective Two |  |  1    1 | 
| Objective Three |  |  1   1  | 
| Objective Four |  |  1    1 | 
| Objective Five |  |  1    1 | 
| Objective Six |  |  1    1 | 
| Objective Seven |  |  1   1  | 



### AI and Open Source Code Assertions

- We have documented in our code readme.md and in our code any software that we have adopted from elsewhere
- We used AI for coding and this is documented in our code as indicated by comments "AI generated" 


