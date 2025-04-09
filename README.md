# Dog Tracker Project

The Dog Tracker Project is an ESP32-based GPS tracking system that logs location data in GPX format and provides a comprehensive web dashboard for real-time monitoring and control. The tracker can automatically start when leaving home and stop when returning (Auto Tracking mode) or be controlled manually. It also monitors battery voltage and logs events with human-readable date/time stamps obtained via NTP (or GPS if needed).

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Circuit Diagram and Wiring](#circuit-diagram-and-wiring)
- [Software and Libraries](#software-and-libraries)
- [Installation and Setup](#installation-and-setup)
- [Usage](#usage)
- [Web Interface Endpoints](#web-interface-endpoints)
- [Flash Endurance Considerations](#flash-endurance-considerations)
- [Future Improvements](#future-improvements)
- [License](#license)

## Overview

The Dog Tracker Project is designed to monitor your dog's location using a GPS module, log the route as GPX files, and provide a user-friendly web dashboard for managing tracking, configuring Wi-Fi, and viewing event logs. It supports two tracking modes:

- **Auto Tracking:** Automatically starts tracking when the device leaves the home zone and stops when it returns.
- **Manual Tracking:** Allows you to start or stop tracking with a dedicated button when Auto Tracking is disabled.

## Features

- **GPS Tracking and GPX Logging:**  
  - Reads GPS data using TinyGPS++.
  - Logs data (latitude, longitude, elevation, timestamp) in GPX format.

- **Dual Tracking Modes:**  
  - **Auto Tracking:** Automatically controls tracking based on distance from home.
  - **Manual Tracking:** Toggle tracking on/off manually.
  - Dashboard buttons display clear labels and colors:
    - **Tracking:** "Tracking: On" (red) when active, and "Tracking: Off" (green) when off.
    - **Auto Tracking:** "Auto Tracking: On" (red) when enabled, and "Auto Tracking: Off" (green) when disabled.

- **Web Dashboard:**  
  - Displays real-time stats such as distance, speed, elevation gain, GPS coordinates, battery voltage, and Wi-Fi mode.
  - Provides buttons for setting home location, configuring Wi-Fi, viewing/deleting log files, and managing GPX tracks.
  
- **Log File Management:**  
  - Logs events with human-readable date/time stamps (sourced via NTP or GPS).
  - Web endpoints allow you to view and clear the log file.

- **Battery Monitoring:**  
  - Uses an ADC input and a voltage divider to measure battery voltage.
  
- **Wi-Fi Connectivity:**  
  - Automatically switches between Station (connecting to home Wi-Fi) and Access Point modes based on configuration and location.

## Hardware Requirements

- **Microcontroller:** ESP32 board (e.g., Adafruit ESP32 Feather Huzzah)
- **GPS Module:** Compatible with ESP32 and TinyGPS++ (e.g., NEO-6M)
- **Power Supply:**  
  - LiPo battery (e.g., 3.87 V, 930 mAh) with appropriate charging circuitry
- **Voltage Divider:**  
  - Two resistors (e.g., R1 = 100 kΩ, R2 = 47 kΩ) for scaling battery voltage to an ADC-safe level
- **LEDs:**  
  - Status LEDs for GPS (GPIO12), Wi-Fi (GPIO13), and Tracking (GPIO15) with current-limiting resistors
- **Additional:**  
  - Necessary wiring and a breadboard or perfboard for prototyping

## Circuit Diagram and Wiring

### Battery Voltage Divider

To measure the battery voltage, the battery is connected through a voltage divider. For example:

```
   + (Battery Positive)
         │
        R1 (e.g., 100kΩ)
         │
         ├──────> To ESP32 ADC Pin (e.g., GPIO33)
         │
        R2 (e.g., 47kΩ)
         │
   GND (Battery Negative)
```

> **Tip:** Place the diagram in a code block as shown above for proper formatting.

### LED Connections

- **GPS LED:** Connect to GPIO12 (with resistor) to ground.
- **Wi-Fi LED:** Connect to GPIO13 (with resistor) to ground.
- **Tracking LED:** Connect to GPIO15 (with resistor) to ground.

Ensure a common ground between all components.

## Software and Libraries

The project is developed using the Arduino framework for ESP32. The following libraries are used:

- **WiFi.h** — For Wi-Fi connectivity.
- **LittleFS.h** — For file system operations.
- **TinyGPS++.h** — For parsing GPS data.
- **WebServer.h** — For hosting the dashboard.
- **Preferences.h** — For persistent configuration.
- **time.h** — For time functions (NTP configuration via `configTime()`).

## Installation and Setup

1. **Install the Arduino IDE and ESP32 Board Support:**  
   Follow the [ESP32 installation guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp32/getting_started.html).

2. **Clone or Download the Repository:**  
   Save the project source code to your local machine.

3. **Install Required Libraries:**  
   Use the Arduino Library Manager to install the libraries listed above.

4. **Hardware Assembly:**  
   Wire the ESP32, GPS module, voltage divider, LEDs, and battery according to the provided schematic.

5. **Upload the Code:**  
   Open the sketch in the Arduino IDE, select your board, and upload the code.

## Usage

- **Setting Home:**  
  Use the "Set Home" button on the dashboard to capture your current GPS location as the home area.
  
- **Tracking Modes:**  
  - **Auto Tracking:**  
    When enabled, the tracker automatically starts recording once it leaves the home area and stops when returning.
  - **Manual Tracking:**  
    When auto tracking is disabled, you can toggle tracking on/off using the "Tracking: Off/On" button.

- **Viewing and Managing Logs:**  
  - Click "View Log" on the dashboard to see log entries with human-readable timestamps.
  - Use "Delete Log" on the log page to clear all log entries.

- **File Management:**  
  - Download or delete individual GPX track files from the dashboard.
  - Use "Delete All Tracks" to clear all logged tracks.

## Web Interface Endpoints

- **/** – Dashboard: Displays tracking data, battery voltage, and control buttons.
- **/setHome** – Sets the current GPS position as home.
- **/toggleTracking** – Toggles manual tracking (if auto tracking is off).
- **/toggleAutoTrack** – Toggles the Auto Tracking feature.
- **/wifiConfig** & **/saveWifi** – Configure Wi-Fi credentials.
- **/log** – Displays the log file with timestamps.
- **/deleteLog** – Clears the log file.
- **/download?file=<filename>** – Downloads a GPX track file.
- **/deleteTrack?file=<filename>** – Deletes a specific GPX track file.
- **/clearTracks** – Deletes all GPX track files.

## Flash Endurance Considerations

The ESP32’s flash typically endures 10,000–100,000 write cycles per sector. Using LittleFS with wear leveling helps distribute writes. Under normal usage conditions—like periodic logging—the system should last months or years before reaching the flash endurance limit.

## Future Improvements

- **Enhanced ADC Calibration:**  
  Improve battery voltage accuracy with advanced calibration techniques or buffer samples.
- **File Management Enhancements:**  
  Add features such as file archiving, renaming, or integration with cloud storage.
- **Dashboard Enhancements:**  
  Incorporate mapping, graphing, and historical data visualization.
- **Mobile App Integration:**  
  Develop a companion app for easier remote monitoring.
- **Power Optimization:**  
  Implement low-power modes to extend battery life.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
