# Dog Tracker Project

A GPS-based dog tracker built on an ESP32 that logs location data to GPX files, monitors battery status, and provides an interactive web dashboard for controlling features such as tracking start/stop, auto-stop toggling, and Wi-Fi configuration.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Circuit Diagram & Wiring](#circuit-diagram--wiring)
- [Software and Libraries](#software-and-libraries)
- [Installation and Setup](#installation-and-setup)
- [Web Interface Endpoints](#web-interface-endpoints)
- [Calibration and Configuration](#calibration-and-configuration)
- [Future Improvements](#future-improvements)
- [License](#license)

## Overview

The Dog Tracker is designed to monitor the location of your dog in real time using a GPS module, log the track in GPX format, and provide control over tracking features via a built-in web server on the ESP32. The device automatically switches between Station mode (connecting to a home Wi-Fi network) and Access Point (AP) mode (when away from home or if Wi-Fi credentials are not set), ensuring you can always connect and manage the tracker.

## Features

- **GPS Tracking:**  
  - Continuously reads GPS data using TinyGPS++.
  - Logs GPX track points with latitude, longitude, elevation, and timestamps.
  
- **Wi-Fi Modes:**  
  - Automatically switches between Station and AP modes based on GPS location (home zone defined by a trigger radius) and the availability of Wi-Fi credentials.
  - Includes a dwell interval (15 seconds) between mode switches and limits repeated connection attempts with a failure flag.

- **Web Dashboard:**  
  - Displays tracking statistics (distance, speed, elevation, GPS coordinates, battery voltage, Wi-Fi status, etc.).
  - Interactive controls:
    - **Set Home:** Set the home location based on the current GPS fix.
    - **Track/Tracking Toggle:** Button displays "Track" (green) when not tracking and "Tracking" (red) when active.
    - **AutoStop Toggle:** Button toggles auto-stop tracking (stopping tracking when near home) and displays "AutoStop: On" (orange) or "AutoStop: Off" (light blue).
    - **Wi-Fi Config:** Configure and save Wi-Fi credentials.
  - Track files appear as download links and are accompanied by "Delete" buttons. A "Delete All Tracks" button is also provided.

- **Battery Monitoring:**  
  - Measures battery voltage via an external voltage divider connected to an ADC pin (e.g., GPIO33).
  - Displays the battery voltage on the dashboard.

- **LED Indicators:**  
  - **GPS LED (GPIO12):** Blinks based on GPS fix status.
  - **Wi-Fi LED (GPIO13):** Indicates Wi-Fi mode (AP vs. Station) with different blink patterns.
  - **Tracking LED (GPIO15 or 27):** Blinks when tracking is active (two blinks every 5 seconds) and stays off when not tracking.

- **Persistent Configuration:**  
  - Uses the ESP32 Preferences library to store Wi-Fi credentials and the home location across reboots.

## Hardware Requirements

- **Microcontroller:** ESP32 (e.g., Adafruit ESP32 Feather Huzzah)
- **GPS Module:** (Compatible with ESP32 and TinyGPS++; e.g., NEO-6M)
- **Power Supply:**  
  - A LiPo battery (e.g., 3.87 V, 930 mAh) with onboard charger circuitry.
- **Voltage Divider:**  
  - Two resistors (e.g., R1 = 100 kΩ and R2 = 47 kΩ, or as measured/calibrated) for stepping down the battery voltage.
- **LEDs:**  
  - Three status LEDs with current-limiting resistors for GPS, Wi-Fi, and Tracking indicators.
- **Additional Components:**  
  - Connecting wires, breadboard or perfboard for prototyping.

## Circuit Diagram & Wiring

### Battery Voltage Divider

<pre>
   + (Battery Positive)
         │
        R1 (e.g., 100kΩ)
         │
         ├──────> To ESP32 ADC Pin (e.g., GPIO33)
         │
        R2 (e.g., 47kΩ)
         │
   GND (Battery Negative)
</pre>

*Ensure that the ADC pin is connected to the junction between R1 and R2, and that the battery ground is common with the ESP32 ground.*

### LED Connections

- **GPS LED (GPIO12):** Connect the LED (with resistor) between GPIO12 and ground.
- **Wi-Fi LED (GPIO13):** Connect the LED (with resistor) between GPIO13 and ground.
- **Tracking LED (e.g., GPIO15 or GPIO27):** Connect the LED (with resistor) between the chosen pin and ground.

## Software and Libraries

The project is based on the Arduino framework for ESP32. The following libraries are used:
- **WiFi.h** — For Wi-Fi connectivity.
- **LittleFS.h** — For file system operations (storing GPX logs and configuration).
- **TinyGPS++.h** — For parsing GPS data.
- **WebServer.h** — To host the web dashboard.
- **Preferences.h** — For persistent configuration storage.
- **vector (STL)** — Used for file management within LittleFS.

## Installation and Setup

1. **Clone or Download the Code:**  
   Clone the repository or download the source files from GitHub.

2. **Install the Arduino IDE and ESP32 Support:**  
   Ensure you have the Arduino IDE with ESP32 board support installed.  
   Install required libraries via the Library Manager.

3. **Assemble the Hardware:**  
   Wire the GPS module, battery, voltage divider, and LEDs according to the schematic.

4. **Upload the Code:**  
   Open the sketch in the Arduino IDE, select your board (e.g., Adafruit ESP32 Feather Huzzah), and upload the code.

5. **Test the Tracker:**  
   Access the web dashboard through the assigned IP address to view tracking data, adjust settings, and download GPX files.

## Web Interface Endpoints

- **Dashboard (`/`):**  
  Displays current stats, including distance, speed, elevation, coordinates, battery voltage, Wi-Fi mode, and more.  
  Contains buttons for:
  - **Set Home:** Set the home location using the current GPS fix.
  - **Track/Tracking Toggle:** Toggles tracking manually. Displays "Track" (green) when off and "Tracking" (red) when on.
  - **AutoStop Toggle:** Toggles auto-stop tracking (stops tracking automatically when in the home zone).
  - **Wi-Fi Config:** Update Wi-Fi credentials.

- **Download Track (`/download?file=<filename>`):**  
  Serves a GPX file for download with the filename specified in the URL.

- **Delete Single Track (`/deleteTrack?file=<filename>`):**  
  Deletes the specified GPX file and redirects back to the dashboard.

- **Delete All Tracks (`/clearTracks`):**  
  Deletes all GPX files from the file system and redirects back to the dashboard.

- **Set Home (`/setHome`):**  
  Reads the current GPS fix and saves the coordinates as the home location.

- **Toggle Tracking (`/toggleTracking`):**  
  Starts or stops tracking. The button label and colour change based on the current state.

- **Toggle AutoStop (`/toggleAutoStop`):**  
  Toggles the autoStopTracking flag. When enabled, tracking stops automatically upon returning home.

- **Wi-Fi Config (`/wifiConfig`):**  
  Displays a form for updating Wi-Fi credentials.

- **Save Wi-Fi (`/saveWifi`):**  
  Saves the updated Wi-Fi credentials to persistent storage.

Actions (toggle, delete, etc.) redirect back to the dashboard automatically.

## Calibration and Configuration

- **ADC Calibration:**  
  Use the resistor values in your voltage divider to set the scaling factor. If your divider doesn’t match the nominal values, adjust the `scalingFactor` accordingly.

- **Persistent Settings:**  
  Wi-Fi credentials and the home location are stored using the Preferences library. They are loaded on startup and can be updated via the web interface.

- **Wi-Fi Mode Switching:**  
  Based on the current GPS location and the value of `triggerRadiusKm`, the device switches between Station mode (for home) and AP mode (for remote tracking). A 15-second dwell time prevents rapid changes, and a failure flag stops repeated unsuccessful connection attempts.

## Future Improvements

- **Enhanced ADC Calibration:**  
  Incorporate averaging and a two-point calibration process for improved battery voltage accuracy.
- **Advanced File Management:**  
  Improve GPX log handling with features such as file renaming, archiving, or cloud backups.
- **Dashboard Enhancements:**  
  Add graphs or historical data on the web dashboard for battery life, GPS location, or speed.
- **Mobile Integration:**  
  Consider creating a companion mobile app for more convenient monitoring and control.
- **Low-Power Modes:**  
  Implement sleep modes or other power management features to extend battery life.

## License

[MIT License](LICENSE)

---

Feel free to adjust this documentation to suit your project. Happy coding!
