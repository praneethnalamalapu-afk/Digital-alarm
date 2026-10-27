# Digital Alarm Clock using PIC18F4580

## Project Overview

The **Digital Alarm Clock** is an embedded systems project developed using the PIC18F4580 microcontroller. The project demonstrates how timers, an RTC, an LCD, switches, and a buzzer can be integrated to build a real-time alarm clock.

The system allows users to configure and manage alarms based on different scheduling options, such as daily, weekly, and one-time alarms.

The project was developed to gain practical experience in Embedded C programming, microcontroller interfacing, and real-time embedded system design.

## Features

- Displays the current time and date.
- Supports configuration of up to 8 alarm events.
- Allows daily, weekly, and one-time alarm scheduling.
- Provides an LCD-based user interface.
- Allows users to configure alarm time and duration.
- Uses a buzzer and LED for alarm notification.
- Supports alarm event management through switches.
- Uses an RTC module for timekeeping.
- Uses EEPROM to store alarm configurations.

## Hardware Components

- PIC18F4580 Microcontroller
- Character LCD (CLCD)
- RTC module (DS1307)
- EEPROM (24C04)
- Switches / Keypad
- Buzzer
- LED
- Required supporting components and connections

## Software and Tools

- MPLAB X IDE
- XC8 C Compiler
- Embedded C
- I2C Communication

## Working Principle

The system uses the PIC18F4580 microcontroller to manage the clock and alarm operations.

The RTC module provides the current time and date. The microcontroller reads this information and displays it on the LCD.

Users can navigate through the configuration menu using switches to set the time, date, and alarm events.

When the configured alarm time matches the current time and date, the system activates the buzzer and LED to notify the user.

The alarm can be configured for different scheduling types, including daily, weekly, and one-time events.

## Project Objectives

- To understand the working of timers and real-time clock modules.
- To develop an embedded application using Embedded C.
- To interface LCD, RTC, EEPROM, switches, buzzer, and LED with a microcontroller.
- To understand I2C communication.
- To implement alarm scheduling and event management.
- To gain practical experience in embedded system development.

## Learning Outcomes

Through this project, I gained practical knowledge of:

- PIC18F4580 microcontroller programming.
- Embedded C programming.
- RTC and EEPROM interfacing.
- I2C communication.
- LCD interfacing.
- Timer and interrupt concepts.
- Switch-based menu navigation.
- Real-time clock and alarm management.

## How to Build the Project

1. Install MPLAB X IDE.
2. Install the required XC8 compiler.
3. Open the Digital Alarm Clock project in MPLAB X IDE.
4. Select the PIC18F4580 microcontroller.
5. Configure the required compiler and project settings.
6. Build the project to generate the HEX file.
7. Program the HEX file into the PIC18F4580 microcontroller.
8. Connect the required hardware components and test the system.

## Project Applications

- Personal digital alarm clock.
- Embedded real-time clock applications.
- Alarm scheduling systems.
- Microcontroller-based event notification systems.

## Conclusion

The Digital Alarm Clock project demonstrates the integration of multiple embedded system peripherals to develop a practical real-time application.

It helped strengthen my understanding of microcontroller programming, peripheral interfacing, I2C communication, and alarm event management.

This project was a valuable step in my Embedded Systems learning journey.

## Author

**Praneeth Reddy N**

B.Tech — Electrical and Electronics Engineering

## Technologies

`Embedded C` `PIC18F4580` `MPLAB X IDE` `XC8` `I2C` `RTC` `EEPROM` `LCD`
