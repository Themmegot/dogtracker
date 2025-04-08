
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
