#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <TinyGPS++.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>
#include <time.h>  // For configTime() and getLocalTime()

// ----------------------
// Global Variables
// ----------------------
Preferences preferences;  // Persistent storage for WiFi credentials and home location

// GPS module interface (using Serial1)
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// File system variables for GPX logging
String currentGpxFilename;
File gpxFile;

// Wi-Fi credentials (loaded from persistent storage)
String wifiSsid;
String wifiPassword;
bool wifiConnected = false;
bool inAPMode = false;

// Maximum attempts to connect as Station before falling back to AP mode
const uint8_t maxStationConnectAttempts = 3;
uint8_t stationConnectAttempts = 0;
bool stationConnectFailure = false;  // Flag set when max attempts are reached

// Minimum delay between WiFi mode switches (in milliseconds)
const unsigned long modeSwitchInterval = 15000;
unsigned long lastModeSwitchTime = 0;

// Home location (loaded from persistent storage) and trigger radius
double homeZoneLat = 0.0;
double homeZoneLon = 0.0;
const float triggerRadiusKm = 0.1;  // If distance < this, considered "home"

// Tracking state variables
bool isTracking = false;
bool hasLeftHome = false;
float totalDistance = 0.0;
float maxSpeed = 0.0;
float avgSpeed = 0.0;
float elevationGain = 0.0;
unsigned long pointCount = 0;
double lastLat = 0.0;
double lastLon = 0.0;
double lastAlt = 0.0;
unsigned long trackingStartTime = 0;
unsigned long lastLogTime = 0;
const unsigned long logInterval = 1000;  // Log every 1 second

// Auto Tracking flag: when true, tracker auto-starts when leaving home and auto-stops when returning
// When false, tracking is controlled solely by the manual "Track" button.
bool autoTracking = true;

// Web server instance
WebServer server(80);

// ----------------------
// Time Configuration (NTP)
// ----------------------
// Using built-in time functions via configTime() to set system time.
// (Adjust gmtOffset_sec and daylightOffset_sec as required.)
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 0;      
const int daylightOffset_sec = 0;  

// ----------------------
// LED Pin Definitions and Blink Patterns
// ----------------------
// GPS LED on GPIO12
const uint8_t gpsLedPin = 12;
const uint16_t gpsNoFixPattern[] = {100, 100, 100, 700};  // When no GPS fix
const uint8_t  gpsNoFixPatternLength = 4;
const uint16_t gpsFixPattern[]   = {100, 14900};           // When GPS fix is valid
const uint8_t  gpsFixPatternLength = 2;
uint8_t gpsLedPatternIndex = 0;
unsigned long gpsLedNextChange = 0;

// WiFi LED on GPIO13
const uint8_t wifiLedPin = 13;
const uint16_t wifiStationPattern[] = {100, 14900};        // When in Station mode
const uint8_t  wifiStationPatternLength = 2;
const uint16_t wifiAPPattern[] = {100, 100, 100, 14700};     // When in Access Point mode
const uint8_t  wifiAPPatternLength = 4;
uint8_t wifiLedPatternIndex = 0;
unsigned long wifiLedNextChange = 0;

// Tracking LED on GPIO15
const uint8_t trackingLedPin = 15;
const uint16_t trackingLedPattern[] = {200, 200, 200, 4400}; // Two blinks per cycle when tracking is active
const uint8_t trackingLedPatternLength = 4;
uint8_t trackingLedPatternIndex = 0;
unsigned long trackingLedNextChange = 0;

// ----------------------
// ADC Setup for Battery Voltage
// ----------------------
// ADC pin for battery voltage measurement via voltage divider.
const int batteryAdcPin = 33;
// Calibration constants based on resistor divider (R1=100k, R2=47k gives ~3.125 scaling factor)
const float voltageDividerFactor = 3.125;
const float ADCRefVoltage = 3.3;  // Reference voltage for ADC

// ----------------------
// Function Prototypes
// ----------------------
float getDistance(double lat1, double lon1, double lat2, double lon2);
void updateWiFiMode(float distanceFromHome);
void startClientWiFi();
void startAccessPoint();
void createNewGpx();
void writeGpxPoint(double lat, double lon, int y, int m, int d, int h, int min, int s, double alt);
void logEvent(String message);
void startWebServer();
String getTrackList();
void clearTracks();
void updateGPSLed();
void updateWiFiLed();
void updateTrackingLed();
String buildPage(String title, String bodyContent);
void loadConfiguration();
void saveWiFiConfig();
void saveHomeConfig();
void handleDownload();
float readBatteryVoltage();
String readLogFile();
void clearLog();

// ----------------------
// Setup Function
// ----------------------
void setup() {
  Serial.begin(115200);
  Serial.println("System starting...");
  
  // Initialize persistent configuration.
  preferences.begin("config", false);
  loadConfiguration();  // Loads WiFi credentials and home location
  
  // Initialize system time via NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  
  // Set ADC attenuation for battery measurement (ADC_11db allows ~3.9V input)
  analogSetAttenuation(ADC_11db);

  // Configure LED pins as outputs.
  pinMode(gpsLedPin, OUTPUT);
  digitalWrite(gpsLedPin, LOW);
  pinMode(wifiLedPin, OUTPUT);
  digitalWrite(wifiLedPin, LOW);
  pinMode(trackingLedPin, OUTPUT);
  digitalWrite(trackingLedPin, LOW);

  // Mount the file system
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  // Initialize GPS serial connection.
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("GPS module started");

  // Start WiFi: if credentials are missing, force AP mode.
  if (wifiSsid.length() == 0 || wifiPassword.length() == 0) {
    Serial.println("No WiFi credentials provided, forcing AP mode");
    startAccessPoint();
  } else {
    startClientWiFi();
  }

  // Start the web server and log startup event.
  startWebServer();
  logEvent("System started");
}

// ----------------------
// Main Loop
// ----------------------
void loop() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
  }
  
  // No need to call getLocalTime() explicitly here; it's used on demand in logEvent().
  
  if (gps.location.isUpdated()) {
    double currentLat = gps.location.lat();
    double currentLon = gps.location.lng();
    double currentAlt = gps.altitude.meters();
    float currentSpeed = gps.speed.kmph();
    
    float distanceFromHome = getDistance(currentLat, currentLon, homeZoneLat, homeZoneLon);
    updateWiFiMode(distanceFromHome);
    
    // Auto Tracking mode: automatically start or stop tracking based on distance.
    if (autoTracking) {
      if (distanceFromHome > triggerRadiusKm && !hasLeftHome) {
        logEvent("Left home zone; auto starting tracking");
        hasLeftHome = true;
        isTracking = true;
        createNewGpx();
        lastLat = currentLat;
        lastLon = currentLon;
        lastAlt = currentAlt;
        lastLogTime = millis();
        pointCount = 1;
      }
      if (distanceFromHome < triggerRadiusKm && isTracking) {
        logEvent("Returned home; auto stopping tracking");
        if (gpxFile) {
          gpxFile.println("</trkseg>");
          gpxFile.println("</trk>");
          gpxFile.println("</gpx>");
          gpxFile.close();
        }
        isTracking = false;
        hasLeftHome = false;
        totalDistance = 0;
        pointCount = 0;
      }
    }
    
    // Log new GPX points at defined intervals when tracking.
    if (isTracking && (millis() - lastLogTime > logInterval)) {
      float incDistance = getDistance(lastLat, lastLon, currentLat, currentLon);
      if (incDistance > 0.005) {  // Only log if movement exceeds a threshold
        totalDistance += incDistance;
        if (currentSpeed > maxSpeed) maxSpeed = currentSpeed;
        avgSpeed = totalDistance / ((millis() - trackingStartTime) / 3600000.0);
        if (currentAlt > lastAlt)
          elevationGain += (currentAlt - lastAlt);
        
        int year = gps.date.year();
        if (year < 100) year += 2000;
        int month = gps.date.month();
        int day = gps.date.day();
        int hour = gps.time.hour();
        int minute = gps.time.minute();
        int second = gps.time.second();
        
        writeGpxPoint(currentLat, currentLon, year, month, day, hour, minute, second, currentAlt);
        
        lastLat = currentLat;
        lastLon = currentLon;
        lastAlt = currentAlt;
        lastLogTime = millis();
        pointCount++;
      }
    }
  }
  
  if (wifiConnected)
    server.handleClient();
  
  // Update status LEDs
  updateGPSLed();
  updateWiFiLed();
  if (isTracking)
    updateTrackingLed();
  else
    digitalWrite(trackingLedPin, LOW);
  
  delay(1);
}

// ----------------------
// Utility Functions
// ----------------------

// Compute the great-circle distance between two points (in kilometers)
float getDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;  // Earth radius in km
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// Update WiFi mode based on distance from home
void updateWiFiMode(float distanceFromHome) {
  if (millis() - lastModeSwitchTime < modeSwitchInterval)
    return;
  
  // If WiFi credentials are missing, force AP mode.
  if (wifiSsid.length() == 0 || wifiPassword.length() == 0) {
    if (!inAPMode) {
      startAccessPoint();
      logEvent("No WiFi credentials provided. Forced AP mode.");
    }
    return;
  }
  
  // When within home zone, try to connect as Station.
  if (distanceFromHome < triggerRadiusKm) {
    if (inAPMode && !stationConnectFailure) {
      WiFi.disconnect(true);
      delay(1000);
      Serial.println("Switching to Station mode");
      logEvent("Switching to Station mode");
      startClientWiFi();
    } else if (!wifiConnected && !stationConnectFailure) {
      startClientWiFi();
    }
  }
  // When outside home zone, switch to AP mode.
  else {
    if (!inAPMode) {
      if (wifiConnected && !inAPMode) {
        WiFi.disconnect(true);
        delay(500);
      }
      startAccessPoint();
      logEvent("Switching to AP mode");
    }
  }
}

// Attempt WiFi connection in Station mode.
void startClientWiFi() {
  lastModeSwitchTime = millis();
  WiFi.mode(WIFI_STA);
  Serial.println("Attempting to connect with SSID: " + wifiSsid);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to WiFi");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected as Station");
    logEvent("WiFi connected as Station");
    wifiConnected = true;
    inAPMode = false;
    stationConnectAttempts = 0;
    stationConnectFailure = false;
  } else {
    Serial.println("\nFailed to connect as Station");
    stationConnectAttempts++;
    if (stationConnectAttempts >= maxStationConnectAttempts) {
      Serial.println("Max station connect attempts reached, switching to AP mode");
      logEvent("Max station connect attempts reached, switching to AP mode");
      stationConnectFailure = true;
      startAccessPoint();
      stationConnectAttempts = 0;
    }
  }
}

// Configure the ESP32 as a WiFi Access Point.
void startAccessPoint() {
  lastModeSwitchTime = millis();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AtlasTracker", "track1234");
  Serial.println("WiFi started in AP mode");
  logEvent("WiFi started in AP mode");
  wifiConnected = true;
  inAPMode = true;
}

// Create a new GPX file for logging a new tracking session.
void createNewGpx() {
  int year = gps.date.year();
  if (year < 100) year += 2000;
  int month = gps.date.month();
  int day = gps.date.day();
  int hour = gps.time.hour();
  int minute = gps.time.minute();
  char filename[30];
  sprintf(filename, "Track_%02d.%02d.%04d-%02d-%02d.gpx", day, month, year, hour, minute);
  currentGpxFilename = String(filename);
  
  gpxFile = LittleFS.open("/" + currentGpxFilename, "w");
  if (!gpxFile) {
    Serial.println("Failed to open GPX file for logging");
    return;
  }
  
  gpxFile.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  gpxFile.println("<gpx version=\"1.1\" creator=\"DogTracker\">");
  gpxFile.println("<trk>");
  gpxFile.println("<trkseg>");
  
  trackingStartTime = millis();
  Serial.print("Started new GPX file: ");
  Serial.println(currentGpxFilename);
}

// Write a GPX track point with the given parameters.
void writeGpxPoint(double lat, double lon, int y, int m, int d, int h, int min, int s, double alt) {
  char timeStr[25];
  sprintf(timeStr, "%04d-%02d-%02dT%02d:%02d:%02dZ", y, m, d, h, min, s);
  if (gpxFile) {
    gpxFile.print("<trkpt lat=\"");
    gpxFile.print(lat, 6);
    gpxFile.print("\" lon=\"");
    gpxFile.print(lon, 6);
    gpxFile.println("\">");
    gpxFile.print("<ele>");
    gpxFile.print(alt, 2);
    gpxFile.println("</ele>");
    gpxFile.print("<time>");
    gpxFile.print(timeStr);
    gpxFile.println("</time>");
    gpxFile.println("</trkpt>");
    gpxFile.flush();
  }
}

// Log an event with a timestamp using NTP/local time if available.
void logEvent(String message) {
  File logFile = LittleFS.open("/log.txt", "a");
  if (logFile) {
    String timestamp;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
      timestamp = String(timeStr);
    } else {
      timestamp = String(millis());
    }
    logFile.print(timestamp);
    logFile.print(": ");
    logFile.println(message);
    logFile.close();
  }
}

// Read the entire contents of the log file.
String readLogFile() {
  String logContent = "";
  File logFile = LittleFS.open("/log.txt", "r");
  if (logFile) {
    while (logFile.available()) {
      logContent += char(logFile.read());
    }
    logFile.close();
  } else {
    logContent = "Log file not found.";
  }
  return logContent;
}

// Delete the log file, then create a new empty log.
void clearLog() {
  if (LittleFS.exists("/log.txt")) {
    LittleFS.remove("/log.txt");
  }
  File logFile = LittleFS.open("/log.txt", "w");
  if (logFile) {
    logFile.println("Log cleared.");
    logFile.close();
  }
}

// Retrieve a list of GPX track files as HTML buttons for download and deletion.
String getTrackList() {
  String list = "";
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return "<p>Failed to open FS directory.</p>";
  }
  std::vector<String> fileNames;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    if (filename.endsWith(".gpx"))
      fileNames.push_back(filename);
    file = root.openNextFile();
  }
  for (size_t i = 0; i < fileNames.size(); i++) {
    list += "<p><a href='/download?file=" + fileNames[i] + "' class='button' style='background-color:#0077cc;'>Download " + fileNames[i] + "</a> ";
    list += "<a href='/deleteTrack?file=" + fileNames[i] + "' class='button' style='background-color:#0077cc;'>Delete</a></p>";
  }
  if (list == "")
    list = "<p>No tracks recorded.</p>";
  list += "<p><a href='/clearTracks' class='button' style='background-color:#0077cc;'>Delete All Tracks</a></p>";
  return list;
}

// Delete all GPX track files and log each deletion.
void clearTracks() {
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("Failed to open directory for clearing tracks");
    return;
  }
  std::vector<String> fileNames;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    if (filename.endsWith(".gpx"))
      fileNames.push_back(filename);
    file = root.openNextFile();
  }
  for (size_t i = 0; i < fileNames.size(); i++) {
    if (LittleFS.exists(fileNames[i])) {
      LittleFS.remove(fileNames[i]);
      logEvent("Deleted track file: " + fileNames[i]);
    }
  }
}

// Handle file download requests for GPX tracks.
void handleDownload() {
  if (server.hasArg("file")) {
    String fileName = server.arg("file");
    if (!fileName.startsWith("/"))
      fileName = "/" + fileName;
    if (LittleFS.exists(fileName)) {
      File f = LittleFS.open(fileName, "r");
      server.sendHeader("Content-Disposition", "attachment; filename=" + fileName);
      server.streamFile(f, "application/gpx+xml");
      f.close();
      return;
    } else {
      server.send(404, "text/html", buildPage("Download", "<p>File not found.</p>"));
      return;
    }
  }
  server.send(400, "text/html", buildPage("Download", "<p>No file specified.</p>"));
}

// ----------------------
// LED Update Functions
// ----------------------
void updateGPSLed() {
  const uint16_t* pattern;
  uint8_t patternLen;
  if (gps.location.isValid()) {
    pattern = gpsFixPattern;
    patternLen = gpsFixPatternLength;
  } else {
    pattern = gpsNoFixPattern;
    patternLen = gpsNoFixPatternLength;
  }
  unsigned long now = millis();
  if (now >= gpsLedNextChange) {
    digitalWrite(gpsLedPin, (gpsLedPatternIndex % 2 == 0) ? HIGH : LOW);
    gpsLedNextChange = now + pattern[gpsLedPatternIndex];
    gpsLedPatternIndex++;
    if (gpsLedPatternIndex >= patternLen)
      gpsLedPatternIndex = 0;
  }
}

void updateWiFiLed() {
  const uint16_t* pattern;
  uint8_t patternLen;
  if (inAPMode) {
    pattern = wifiAPPattern;
    patternLen = wifiAPPatternLength;
  } else {
    pattern = wifiStationPattern;
    patternLen = wifiStationPatternLength;
  }
  unsigned long now = millis();
  if (now >= wifiLedNextChange) {
    digitalWrite(wifiLedPin, (wifiLedPatternIndex % 2 == 0) ? HIGH : LOW);
    wifiLedNextChange = now + pattern[wifiLedPatternIndex];
    wifiLedPatternIndex++;
    if (wifiLedPatternIndex >= patternLen)
      wifiLedPatternIndex = 0;
  }
}

void updateTrackingLed() {
  unsigned long now = millis();
  if (now >= trackingLedNextChange) {
    digitalWrite(trackingLedPin, (trackingLedPatternIndex % 2 == 0) ? HIGH : LOW);
    trackingLedNextChange = now + trackingLedPattern[trackingLedPatternIndex];
    trackingLedPatternIndex++;
    if (trackingLedPatternIndex >= trackingLedPatternLength)
      trackingLedPatternIndex = 0;
  }
}

// ----------------------
// ADC and Log File Functions
// ----------------------
float readBatteryVoltage() {
  const int numReadings = 10;
  long total = 0;
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(batteryAdcPin);
    delay(10);
  }
  float averageReading = (float)total / numReadings;
  float adcVoltage = (averageReading / 4095.0) * ADCRefVoltage;
  return adcVoltage * voltageDividerFactor;
}

String buildPage(String title, String bodyContent) {
  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>)HTML";
  html += title;
  html += R"HTML(</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #f0f4f8; margin: 0; padding: 20px; color: #222; }
    h1 { font-size: 24px; margin-bottom: 10px; color: #333; }
    a.button { display: inline-block; padding: 10px 15px; margin: 5px; background-color: #0077cc; color: white; text-decoration: none; border-radius: 5px; }
    a.button:hover { background-color: #005fa3; }
  </style>
</head>
<body>
<h1>)HTML";
  html += title;
  html += R"HTML(</h1>
)HTML";
  html += bodyContent;
  html += R"HTML(
<p><a href="/" class="button">Return to Dashboard</a></p>
</body>
</html>
)HTML";
  return html;
}

void loadConfiguration() {
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("pass", "");
  homeZoneLat = preferences.getDouble("homeLat", 0.0);
  homeZoneLon = preferences.getDouble("homeLon", 0.0);
  Serial.println("Loaded WiFi SSID: " + wifiSsid);
  Serial.println("Loaded Home Location: " + String(homeZoneLat, 6) + ", " + String(homeZoneLon, 6));
}

void saveWiFiConfig() {
  preferences.putString("ssid", wifiSsid);
  preferences.putString("pass", wifiPassword);
  Serial.println("Saved WiFi configuration.");
}

void saveHomeConfig() {
  preferences.putDouble("homeLat", homeZoneLat);
  preferences.putDouble("homeLon", homeZoneLon);
  Serial.println("Saved Home configuration: " + String(homeZoneLat, 6) + ", " + String(homeZoneLon, 6));
}

// ----------------------
// Web Server and Endpoints
// ----------------------
void startWebServer() {
  // Endpoint to serve GPX files for download.
  server.on("/download", HTTP_GET, handleDownload);
  
  // Endpoint to view the log file.
  server.on("/log", HTTP_GET, [](){
    String logContent = readLogFile();
    String html = buildPage("Log File", "<pre>" + logContent + "</pre><p><a href='/deleteLog' class='button' style='background-color:#0077cc;'>Delete Log</a></p>");
    server.send(200, "text/html", html);
  });
  
  // Endpoint to delete (clear) the log file.
  server.on("/deleteLog", HTTP_GET, [](){
    clearLog();
    server.sendHeader("Location", "/log", true);
    server.send(302, "text/plain", "Redirecting...");
  });
  
  // Dashboard endpoint.
  server.on("/", HTTP_GET, [](){
#ifdef ESP32
    uint32_t totalBytes = LittleFS.totalBytes();
    uint32_t usedBytes = LittleFS.usedBytes();
#else
    uint32_t totalBytes = 0;
    uint32_t usedBytes = 0;
#endif
    unsigned long durationSec = isTracking ? ((millis() - trackingStartTime) / 1000) : 0;
    
    // Build manual tracking toggle button.
    String trackButton;
    if (isTracking)
      trackButton = "<a href='/toggleTracking' class='button' style='background-color:red;'>Tracking: On</a>";
    else
      trackButton = "<a href='/toggleTracking' class='button' style='background-color:green;'>Tracking: Off</a>";
    
    // Build auto tracking toggle button.
    String autoTrackButton;
    if (autoTracking)
      autoTrackButton = "<a href='/toggleAutoTrack' class='button' style='background-color:red;'>Auto Tracking: On</a>";
    else
      autoTrackButton = "<a href='/toggleAutoTrack' class='button' style='background-color:green;'>Auto Tracking: Off</a>";
    
    String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Atlas Tracker</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #f0f4f8; margin: 0; padding: 20px; color: #222; }
    h1 { font-size: 24px; margin-bottom: 10px; color: #333; }
    .stat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 15px; margin-bottom: 25px; }
    .stat-card { background: white; border-radius: 8px; padding: 15px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    .stat-card strong { display: block; font-size: 14px; color: #555; }
    .stat-card span { font-size: 18px; font-weight: bold; color: #111; }
    a.button { display: inline-block; padding: 10px 15px; margin: 5px; text-decoration: none; border-radius: 5px; color: white; }
    a.button:hover { opacity: 0.8; }
  </style>
</head>
<body>
  <h1>Atlas Tracker Dashboard</h1>
  <div class='stat-grid'>
    <div class='stat-card'><strong>Distance</strong><span>%DIST% km</span></div>
    <div class='stat-card'><strong>Max Speed</strong><span>%MAXSPEED% m/s</span></div>
    <div class='stat-card'><strong>Avg Speed</strong><span>%AVGSPEED% km/h</span></div>
    <div class='stat-card'><strong>Elevation Gain</strong><span>%ELEVATION% m</span></div>
    <div class='stat-card'><strong>Latitude</strong><span>%LAT%</span></div>
    <div class='stat-card'><strong>Longitude</strong><span>%LON%</span></div>
    <div class='stat-card'><strong>Altitude</strong><span>%ALT% m</span></div>
    <div class='stat-card'><strong>Track Duration</strong><span>%DURATION% s</span></div>
    <div class='stat-card'><strong>Points</strong><span>%POINTS%</span></div>
    <div class='stat-card'><strong>Used</strong><span>%USED% KB</span></div>
    <div class='stat-card'><strong>Free</strong><span>%FREE% KB</span></div>
    <div class='stat-card'><strong>WiFi Mode</strong><span>%WIFIMODE%</span></div>
    <div class='stat-card'><strong>Battery</strong><span>%BATTERY%</span></div>
  </div>
  <div>
    <a href='/setHome' class='button' style='background-color:#0077cc;'>Set Home</a>
    )HTML";
    html += trackButton;
    html += autoTrackButton;
    html += R"HTML(
    <a href='/wifiConfig' class='button' style='background-color:#0077cc;'>WiFi Config</a>
    <a href='/log' class='button' style='background-color:#0077cc;'>View Log</a>
  </div>
  <div>
    <h2>Recorded Tracks</h2>
    %TRACKS%
  </div>
</body>
</html>
)HTML";
    
    html.replace("%DIST%", String(totalDistance, 2));
    html.replace("%MAXSPEED%", String(maxSpeed, 2));
    html.replace("%AVGSPEED%", String(avgSpeed, 2));
    html.replace("%ELEVATION%", String(elevationGain, 1));
    html.replace("%LAT%", String(gps.location.lat(), 6));
    html.replace("%LON%", String(gps.location.lng(), 6));
    html.replace("%ALT%", String(gps.altitude.meters(), 1));
    html.replace("%DURATION%", String(durationSec));
    html.replace("%POINTS%", String(pointCount));
#ifdef ESP32
    html.replace("%USED%", String((float)LittleFS.usedBytes() / 1024.0, 1));
    html.replace("%FREE%", String(((float)LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024.0, 1));
#else
    html.replace("%USED%", "0");
    html.replace("%FREE%", "0");
#endif
    html.replace("%WIFIMODE%", wifiConnected ? (inAPMode ? "Access Point" : "Station (Connected)") : "Disconnected");
    html.replace("%BATTERY%", String(readBatteryVoltage(), 2) + " V");
    html.replace("%TRACKS%", getTrackList());
    
    server.send(200, "text/html", html);
  });
  
  // Endpoint to set home point using current GPS fix.
  server.on("/setHome", HTTP_GET, [](){
    String body;
    if (gps.location.isValid()) {
      homeZoneLat = gps.location.lat();
      homeZoneLon = gps.location.lng();
      saveHomeConfig();
      logEvent("Home set to: " + String(homeZoneLat, 6) + ", " + String(homeZoneLon, 6));
      body = "<p>Home point set to current location (" + String(homeZoneLat, 6) + ", " + String(homeZoneLon, 6) + ").</p>";
    } else {
      body = "<p>No valid GPS fix to set home point.</p>";
    }
    server.send(200, "text/html", buildPage("Set Home", body));
  });
  
  // Endpoint for manual tracking toggle.
  server.on("/toggleTracking", HTTP_GET, [](){
    if (isTracking) {
      logEvent("Manual stop tracking triggered");
      if (gpxFile) {
        gpxFile.println("</trkseg>");
        gpxFile.println("</trk>");
        gpxFile.println("</gpx>");
        gpxFile.close();
      }
      isTracking = false;
      hasLeftHome = false;
      totalDistance = 0;
      pointCount = 0;
    } else {
      logEvent("Manual start tracking triggered");
      isTracking = true;
      hasLeftHome = true;
      createNewGpx();
      pointCount = 1;
      trackingStartTime = millis();
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });
  
  // Endpoint to toggle auto tracking.
  server.on("/toggleAutoTrack", HTTP_GET, [](){
    autoTracking = !autoTracking;
    logEvent(String("Auto Tracking toggled to: ") + (autoTracking ? "On" : "Off"));
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });
  
  // Endpoint to display the WiFi configuration form.
  server.on("/wifiConfig", HTTP_GET, [](){
    String body;
    body = "<form action='/saveWifi' method='POST'>";
    body += "SSID: <input type='text' name='ssid' value='" + wifiSsid + "'><br>";
    body += "Password: <input type='text' name='pass' value='" + wifiPassword + "'><br>";
    body += "<input type='submit' value='Save'>";
    body += "</form>";
    server.send(200, "text/html", buildPage("WiFi Configuration", body));
  });
  
  // Endpoint to save WiFi credentials.
  server.on("/saveWifi", HTTP_POST, [](){
    String body;
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      wifiSsid = server.arg("ssid");
      wifiPassword = server.arg("pass");
      logEvent("WiFi credentials updated: SSID: " + wifiSsid);
      Serial.println("New credentials: " + wifiSsid + " / " + wifiPassword);
      saveWiFiConfig();
      delay(3000);
      if (wifiSsid.length() > 0 && wifiPassword.length() > 0)
        startClientWiFi();
      else
        startAccessPoint();
      body = "<p>WiFi credentials updated.</p>";
    } else {
      body = "<p>Missing parameters.</p>";
    }
    server.send(200, "text/html", buildPage("Save WiFi", body));
  });
  
  // Endpoint to delete a single track file.
  server.on("/deleteTrack", HTTP_GET, [](){
    String fileName;
    if (server.hasArg("file")) {
      fileName = server.arg("file");
      if (!fileName.startsWith("/"))
        fileName = "/" + fileName;
      if (LittleFS.exists(fileName)) {
        LittleFS.remove(fileName);
        logEvent("Deleted track file: " + fileName);
      }
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });
  
  // Endpoint to delete all track files.
  server.on("/clearTracks", HTTP_GET, [](){
    clearTracks();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });
  
  // Basic 404 handler.
  server.onNotFound([](){
    server.send(404, "text/html", buildPage("404 Not Found", "<p>The requested page was not found.</p>"));
  });
  
  server.begin();
  Serial.println("Web server started");
}
