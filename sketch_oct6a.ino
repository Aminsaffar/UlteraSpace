/*********
  ESP32 NEO-6M GPS Datalogger with Async Web Server + SIM800L
  Reads GPS data, saves to LittleFS CSV, serves async webpage with last location and CSV download.
  Sends GPS data to remote webserver via SIM800L GPRS connection.
  Tested with ESP32 core 3.0.5 + lacamera ESP Async WebServer v1.2.3.
*********/

#include <WiFi.h>  // Explicit for AP mode
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#ifndef ARDUINO_EVENT_WIFI_STA_GOT_IP
#define ARDUINO_EVENT_WIFI_STA_GOT_IP IP_EVENT_STA_GOT_IP
#define ARDUINO_EVENT_WIFI_STA_DISCONNECTED WIFI_EVENT_STA_DISCONNECTED
#endif

// GPS Configuration
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// SIM800L Configuration
#define SIM800L_RX 25  // ESP32 GPIO25 -> SIM800L TX
#define SIM800L_TX 26  // ESP32 GPIO26 -> SIM800L RX
#define SIM800L_BAUD 9600
HardwareSerial sim800lSerial(1);

// SIM800L Settings
const char* apn = "mcinet";  // Change to your carrier's APN (e.g., "hologram", "mtn", "airtel")
const char* gprsUser = "";     // GPRS username (often empty)
const char* gprsPass = "";     // GPRS password (often empty)

// Webserver endpoint for GPS data
const char* serverUrl = "http://amins.ir/api/gps";  // Change to your server URL
const int serverPort = 80;

// SIM800L state management
bool sim800lReady = false;
bool gprsConnected = false;
unsigned long lastGprsCheckMillis = 0;
const unsigned long GPRS_CHECK_INTERVAL_MS = 25 * 60000; // Check GPRS every 25 minutes
unsigned long lastDataSendMillis = 0;
const unsigned long DATA_SEND_INTERVAL_MS = 120000;  // Send data every 2 minutes
String sim800lResponse = "";

// LED Configuration (for serial activity indicator)
#define LED_PIN 2
// WiFi AP Configuration
const char* ssid = "UlTeRa_SpAcE";
const char* password = "12345678";

// Async Web Server
AsyncWebServer server(80);

// Data Storage
const char* dataPath = "/gpslog.csv";
const char* CSV_HEADER = "UTC Time,Latitude,Longitude,Altitude(m),Speed(km/h),Satellites";
String lastLat = "0.000000";
String lastLng = "0.000000";
String lastTime = "No fix yet";

String lastSpeedKmph = "0.0";
String lastAltitudeMeters = "0.0";
String lastCourseDeg = "N/A";
String lastSatellites = "0";
String lastHdop = "N/A";
String lastFixAgeMs = "N/A";

// SIM800L status for display
String sim800lStatus = "Initializing";
String gprsStatus = "Disconnected";
String lastSendTime = "Never";
String lastSendStatus = "N/A";

// System information variables
String freeHeapStr = "0";
String totalHeapStr = "0";
String usedHeapStr = "0";
String flashSizeStr = "0";
String flashSpeedStr = "0";
String cpuFreqStr = "0";
String littleFSTotalStr = "0";
String littleFSUsedStr = "0";
String littleFSFreeStr = "0";
String chipModelStr = "ESP32";
String uptimeStr = "0s";

// --- Logging policy/config ---
const unsigned long LOG_MAX_INTERVAL_MS = 30UL * 1000UL; // maximum time between logged points (30s heartbeat)
const double SIGNIFICANT_DISTANCE_METERS = 10.0; // minimum distance change to consider (meters)

// Storage management
const size_t MAX_LOG_FILE_SIZE = 640 * 1024; // trim when >720KB
const size_t LOG_TRIM_KEEP_BYTES = 320 * 1024; // keep the newest ~460KB of log entries when trimming
const float STORAGE_HIGH_WATERMARK = 0.90; // fraction used of LittleFS to trigger cleanup

struct KnownNetwork {
  const char* ssid;
  const char* password;
};

const KnownNetwork knownNetworks[] = {
  // TODO: update with your trusted hotspots
  {"mixura", "@@@sadra@@@sadra@@@min"},
  {"YourOfficeWiFi", "office-password"}
};
const size_t KNOWN_NETWORK_COUNT = sizeof(knownNetworks) / sizeof(knownNetworks[0]);

bool stationConnected = false;
String stationIpStr = "Not connected";
String currentStationSsid = "";
String apIpStr = "";

const unsigned long WIFI_SCAN_INTERVAL_MS = 20 * 60UL * 1000UL; // rescan for known networks every 20 minutes
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000UL;
unsigned long lastWifiScanMillis = 0;

// Runtime state for logging decisions
bool hasLastLogged = false;
double lastLoggedLat = 0.0;
double lastLoggedLng = 0.0;
unsigned long lastLoggedMillis = 0;
unsigned long lastStorageCheckMillis = 0;
const unsigned long STORAGE_CHECK_INTERVAL_MS = 5UL * 60UL * 1000UL; // check storage every 5 minutes

// Small helper: Haversine distance (meters)
double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0; // Earth radius in meters
  double dLat = (lat2 - lat1) * DEG_TO_RAD;
  double dLon = (lon2 - lon1) * DEG_TO_RAD;
  double a = sin(dLat/2) * sin(dLat/2) + cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) * sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      stationConnected = true;
      stationIpStr = WiFi.localIP().toString();
      currentStationSsid = WiFi.SSID();
      Serial.printf("Station connected, IP: %s\n", stationIpStr.c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (stationConnected) {
        Serial.println("Station disconnected. Attempting reconnect...");
      }
      stationConnected = false;
      stationIpStr = "Not connected";
      currentStationSsid = "";
      WiFi.reconnect();
      break;
    default:
      break;
  }
}

bool attemptConnectToKnownNetworks() {
  if (KNOWN_NETWORK_COUNT == 0) {
    Serial.println("No known networks configured.");
    return false;
  }

  Serial.println("Scanning for known WiFi hotspots...");
  int16_t networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    Serial.println("No WiFi networks detected.");
    return false;
  }

  int bestKnownIndex = -1;
  int bestKnownRssi = -1000;
  for (int i = 0; i < networkCount; ++i) {
    String foundSsid = WiFi.SSID(i);
    for (size_t k = 0; k < KNOWN_NETWORK_COUNT; ++k) {
      if (foundSsid == knownNetworks[k].ssid) {
        int rssi = WiFi.RSSI(i);
        if (rssi > bestKnownRssi) {
          bestKnownRssi = rssi;
          bestKnownIndex = static_cast<int>(k);
        }
      }
    }
  }

  if (bestKnownIndex == -1) {
    Serial.println("No known hotspots in range.");
    return false;
  }

  const KnownNetwork& chosen = knownNetworks[bestKnownIndex];
  Serial.printf("Attempting connection to %s (RSSI %d dBm)\n", chosen.ssid, bestKnownRssi);
  WiFi.begin(chosen.ssid, chosen.password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    stationConnected = true;
    currentStationSsid = String(chosen.ssid);
    stationIpStr = WiFi.localIP().toString();
    Serial.printf("Connected to %s, IP %s\n", currentStationSsid.c_str(), stationIpStr.c_str());
    WiFi.setAutoReconnect(true);
    return true;
  }

  Serial.println("Connection attempt failed.");
  WiFi.disconnect(true);
  return false;
}

void maintainWiFi() {
  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED) {
    if (stationConnected) {
      stationConnected = false;
      stationIpStr = "Not connected";
      currentStationSsid = "";
    }
    if (KNOWN_NETWORK_COUNT == 0) {
      return;
    }
    unsigned long now = millis();
    if (now - lastWifiScanMillis >= WIFI_SCAN_INTERVAL_MS) {
      lastWifiScanMillis = now;
      attemptConnectToKnownNetworks();
    }
  } else if (!stationConnected) {
    stationConnected = true;
    stationIpStr = WiFi.localIP().toString();
    currentStationSsid = WiFi.SSID();
  }
}

// Trim the current log file to keep only the newest entries (single-file policy)
// void trimLogFile() {
//   if (!LittleFS.exists(dataPath)) return;
//   File file = LittleFS.open(dataPath, FILE_READ);
//   if (!file) return;

//   size_t fileSize = file.size();
//   if (fileSize <= MAX_LOG_FILE_SIZE) {
//     file.close();
//     return;
//   }

//   String header = file.readStringUntil('\n');
//   String remaining = file.readString();
//   file.close();

//   size_t keepBytes = LOG_TRIM_KEEP_BYTES;
//   if (keepBytes == 0 || keepBytes > remaining.length()) {
//     keepBytes = remaining.length();
//   }

//   size_t startIndex = remaining.length() > keepBytes ? remaining.length() - keepBytes : 0;
//   // adjust to next newline boundary to ensure we keep whole rows
//   int newlineAfter = remaining.indexOf('\n', startIndex);
//   if (newlineAfter != -1 && (size_t)newlineAfter < remaining.length() - 1) {
//     startIndex = newlineAfter + 1; // skip partial row
//   } else if (startIndex > 0) {
//     int newlineBefore = remaining.lastIndexOf('\n', startIndex);
//     if (newlineBefore != -1 && (size_t)newlineBefore + 1 < remaining.length()) {
//       startIndex = newlineBefore + 1;
//     }
//   }

//   String trimmed = remaining.substring(startIndex);

//   File out = LittleFS.open(dataPath, FILE_WRITE);
//   if (!out) return;
//   out.println(header);
//   out.print(trimmed);
//   out.close();
//   Serial.println("Log file trimmed to maintain size limits.");
// }
void trimLogFile() {
  if (!LittleFS.exists(dataPath)) return;
  File in = LittleFS.open(dataPath, FILE_READ);
  if (!in) return;

  String header = in.readStringUntil('\n');
  size_t fileSize = in.size();
  if (fileSize <= MAX_LOG_FILE_SIZE) {
    in.close();
    return;
  }

  size_t keepBytes = LOG_TRIM_KEEP_BYTES;
  if (keepBytes == 0 || keepBytes > fileSize) keepBytes = fileSize;

  size_t startPos = fileSize - keepBytes;
  in.seek(startPos);

  // align to next full line
  in.readStringUntil('\n'); // discard partial line

  File out = LittleFS.open("/tmp_log.csv", FILE_WRITE);
  if (!out) { in.close(); return; }

  out.print(header);
  out.print('\n');

  while (in.available()) {
    out.write(in.read());
  }

  in.close();
  out.close();

  LittleFS.remove(dataPath);
  LittleFS.rename("/tmp_log.csv", dataPath);

  Serial.println("Log file trimmed to maintain size limits.");
}

  String formatBytes(size_t bytes) {
    if (bytes < 1024) {
      return String(bytes) + " B";
    }
    double kb = bytes / 1024.0;
    if (kb < 1024.0) {
      return String(kb, 1) + " KB";
    }
    double mb = kb / 1024.0;
    return String(mb, 2) + " MB";
  }

  // Update system information
  void updateSystemInfo() {
    // Heap information
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedHeap = totalHeap - freeHeap;
    freeHeapStr = formatBytes(freeHeap);
    totalHeapStr = formatBytes(totalHeap);
    usedHeapStr = formatBytes(usedHeap);
    
    // Flash information
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashSpeed = ESP.getFlashChipSpeed() / 1000000; // Convert to MHz
    flashSizeStr = formatBytes(flashSize);
    flashSpeedStr = String(flashSpeed) + " MHz";
    
    // CPU frequency
    uint32_t cpuFreq = ESP.getCpuFreqMHz();
    cpuFreqStr = String(cpuFreq) + " MHz";
    
    // LittleFS storage
    size_t totalFS = LittleFS.totalBytes();
    size_t usedFS = LittleFS.usedBytes();
    size_t freeFS = totalFS > usedFS ? (totalFS - usedFS) : 0;
    
    littleFSTotalStr = totalFS > 0 ? formatBytes(totalFS) : "0 B";
    littleFSUsedStr = totalFS > 0 ? formatBytes(usedFS) : "0 B";
    littleFSFreeStr = totalFS > 0 ? formatBytes(freeFS) : "0 B";
    
    // Chip model - simplified approach
    chipModelStr = ESP.getChipModel();
    chipModelStr += " Rev" + String(ESP.getChipRevision());
    
    // Uptime
    unsigned long upMillis = millis();
    unsigned long upSeconds = upMillis / 1000;
    unsigned long upMinutes = upSeconds / 60;
    unsigned long upHours = upMinutes / 60;
    unsigned long upDays = upHours / 24;
    
    if (upDays > 0) {
      uptimeStr = String(upDays) + "d " + String(upHours % 24) + "h";
    } else if (upHours > 0) {
      uptimeStr = String(upHours) + "h " + String(upMinutes % 60) + "m";
    } else if (upMinutes > 0) {
      uptimeStr = String(upMinutes) + "m " + String(upSeconds % 60) + "s";
    } else {
      uptimeStr = String(upSeconds) + "s";
    }
  }

  String generateMinimalPage() {
    updateSystemInfo(); // Update system info before generating page
    String page = F(R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>UlTeRa Space GPS · Direct Mode</title>
  <style>
  :root{color-scheme:dark light;}
  body{margin:0;font-family:'Segoe UI',Arial,sans-serif;background:#0b0d17;color:#f5f7ff;display:flex;flex-direction:column;min-height:100vh;}
  main{flex:1;display:flex;flex-direction:column;gap:18px;padding:32px;margin:0 auto;max-width:480px;}
  h1{margin:0;font-size:1.9rem;font-weight:600;}
  p{margin:0;color:#b0b9d9;line-height:1.6;}
  .card{background:rgba(15,23,42,0.55);border-radius:18px;padding:20px;box-shadow:0 24px 48px rgba(15,23,42,0.55);border:1px solid rgba(148,163,184,0.22);}
  label{display:block;text-transform:uppercase;font-size:0.75rem;letter-spacing:.15em;color:#9ea6c9;margin-top:12px;}
  .value{font-size:1.4rem;font-weight:600;margin-top:4px;color:#f8fbff;}
  .actions{display:flex;flex-wrap:wrap;gap:12px;margin-top:8px;}
  a.btn{display:inline-flex;align-items:center;justify-content:center;padding:12px 24px;border-radius:999px;text-decoration:none;font-weight:600;background:#5c6cff;color:white;box-shadow:0 16px 32px rgba(92,108,255,0.35);}
  footer{padding:20px 32px;text-align:center;font-size:0.85rem;color:#7c86a7;}
  </style>
  </head>
  <body>
  <main>
    <h1>UlTeRa Space GPS</h1>
    <p>Device is operating in access point mode. It will automatically join trusted hotspots when they are in range.</p>
    <div class="card">
      <label>Latitude</label>
      <div class="value">{{LAT}}</div>
      <label>Longitude</label>
      <div class="value">{{LNG}}</div>
      <label>Last Fix</label>
      <div class="value">{{TIME}}</div>
    </div>
    <div class="card">
      <label>Speed (km/h)</label>
      <div class="value">{{SPEED}}</div>
      <label>Visible Satellites</label>
      <div class="value">{{SAT}}</div>
    </div>
    <div class="actions">
      <a class="btn" href="/download">Download Latest CSV</a>
      <a class="btn" href="/trace">GPS Trace</a>
    </div>
    <div class="card">
      <label>Current Hotspot</label>
      <div class="value">SSID: {{AP_SSID}}</div>
      <label>AP IP</label>
      <div class="value">{{AP_IP}}</div>
    </div>
    <div class="card">
      <label>Chip Model</label>
      <div class="value">{{CHIP_MODEL}}</div>
      <label>CPU Frequency</label>
      <div class="value">{{CPU_FREQ}}</div>
    </div>
    <div class="card">
      <label>Free Heap</label>
      <div class="value">{{FREE_HEAP}} / {{TOTAL_HEAP}}</div>
      <label>Flash Size</label>
      <div class="value">{{FLASH_SIZE}} @ {{FLASH_SPEED}}</div>
    </div>
    <div class="card">
      <label>LittleFS Storage</label>
      <div class="value">{{FS_FREE}} free of {{FS_TOTAL}}</div>
      <label>Uptime</label>
      <div class="value">{{UPTIME}}</div>
    </div>
  </main>
  <footer>Searching for trusted WiFi every {{SCAN_INTERVAL}} seconds.</footer>
  </body>
  </html>
  )rawliteral");
    page.replace("{{LAT}}", lastLat);
    page.replace("{{LNG}}", lastLng);
    page.replace("{{TIME}}", lastTime);
    page.replace("{{SPEED}}", lastSpeedKmph);
    page.replace("{{SAT}}", lastSatellites);
    page.replace("{{AP_SSID}}", String(ssid));
    page.replace("{{AP_IP}}", apIpStr);
    page.replace("{{SCAN_INTERVAL}}", String(WIFI_SCAN_INTERVAL_MS / 1000));
    page.replace("{{CHIP_MODEL}}", chipModelStr);
    page.replace("{{CPU_FREQ}}", cpuFreqStr);
    page.replace("{{FREE_HEAP}}", freeHeapStr);
    page.replace("{{TOTAL_HEAP}}", totalHeapStr);
    page.replace("{{FLASH_SIZE}}", flashSizeStr);
    page.replace("{{FLASH_SPEED}}", flashSpeedStr);
    page.replace("{{FS_FREE}}", littleFSFreeStr);
    page.replace("{{FS_TOTAL}}", littleFSTotalStr);
    page.replace("{{UPTIME}}", uptimeStr);
    return page;
  }

  String generateAdvancedPage() {
    updateSystemInfo(); // Update system info before generating page
    size_t fileSize = 0;
    if (LittleFS.exists(dataPath)) {
      File f = LittleFS.open(dataPath, FILE_READ);
      if (f) {
        fileSize = f.size();
        f.close();
      }
    }
    String page = F(R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>UlTeRa Space GPS · Connected Dashboard</title>
  <style>
  :root{color-scheme:dark light;}
  body{margin:0;font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#0f172a 0%,#1e293b 60%,#312e81 100%);color:#e2e8f0;min-height:100vh;}
  main{max-width:1100px;margin:0 auto;padding:32px 24px 64px;}
  header{display:flex;flex-direction:column;gap:8px;margin-bottom:28px;}
  h1{margin:0;font-size:2.2rem;font-weight:700;letter-spacing:0.02em;}
  .status-strip{display:flex;flex-wrap:wrap;gap:12px;}
  .pill{display:inline-flex;align-items:center;gap:8px;padding:10px 18px;border-radius:999px;background:rgba(148,163,184,0.18);backdrop-filter:blur(10px);font-size:0.85rem;}
  .pill strong{color:#fff;}
  .grid{display:grid;gap:18px;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));}
  .card{background:rgba(15,23,42,0.68);border-radius:22px;padding:22px;border:1px solid rgba(148,163,184,0.22);box-shadow:0 30px 60px rgba(15,23,42,0.55);backdrop-filter:blur(18px);}
  .card h2{margin:0 0 12px 0;font-size:0.95rem;text-transform:uppercase;letter-spacing:.18em;color:#94a3b8;}
  .metric{font-size:1.75rem;font-weight:600;color:#f8fafc;}
  .sub{margin-top:6px;color:#cbd5f5;font-size:0.85rem;}
  .actions{margin:28px 0;display:flex;flex-wrap:wrap;gap:12px;}
  .btn{display:inline-flex;align-items:center;gap:10px;padding:12px 22px;border-radius:999px;text-decoration:none;font-weight:600;transition:transform .2s,box-shadow .2s;background:#6366f1;color:white;box-shadow:0 18px 36px rgba(99,102,241,0.35);} 
  .btn:hover{transform:translateY(-1px);box-shadow:0 16px 30px rgba(99,102,241,0.45);} 
  .btn.secondary{background:rgba(148,163,184,0.22);color:#e2e8f0;box-shadow:none;}
  table{width:100%;border-collapse:collapse;background:rgba(15,23,42,0.6);border-radius:18px;overflow:hidden;}
  th,td{padding:12px 16px;text-align:left;border-bottom:1px solid rgba(148,163,184,0.18);font-size:0.95rem;}
  tr:last-child td{border-bottom:none;}
  .badge{display:inline-flex;align-items:center;gap:6px;padding:6px 12px;border-radius:999px;font-size:0.8rem;font-weight:600;background:rgba(34,197,94,0.18);color:#4ade80;}
  .badge.offline{background:rgba(248,113,113,0.2);color:#fca5a5;}
  </style>
  </head>
  <body>
  <main>
    <header>
      <h1>UlTeRa Space GPS · Mission Dashboard</h1>
      <div class="status-strip">
        <span class="pill">Hotspot: <strong>{{WIFI_SSID}}</strong> · IP <span id="wifiIp">{{WIFI_IP}}</span></span>
        <span class="pill">Local AP <strong>{{AP_SSID}}</strong> · {{AP_IP}}</span>
        <span class="pill">Logs trimmed above {{MAX_SIZE}} KB · Threshold {{TRIM_THRESHOLD}}%</span>
        <span class="pill">Heartbeat ≤ {{HEARTBEAT}} s</span>
      </div>
    </header>
    <section class="grid">
      <div class="card">
        <h2>Position</h2>
        <div class="metric"><span id="lat">{{LAT}}</span>°</div>
        <div class="metric"><span id="lng">{{LNG}}</span>°</div>
        <p class="sub">Updated <span id="lastTime">{{TIME}}</span></p>
      </div>
      <div class="card">
        <h2>Motion</h2>
        <div class="metric"><span id="speed">{{SPEED}}</span> km/h</div>
        <p class="sub">Course <span id="course">{{COURSE}}</span>°</p>
      </div>
      <div class="card">
        <h2>Altitude</h2>
        <div class="metric"><span id="altitude">{{ALT}}</span> m</div>
        <p class="sub">HDOP <span id="hdop">{{HDOP}}</span></p>
      </div>
      <div class="card">
        <h2>Satellites</h2>
        <div class="metric"><span id="satellites">{{SAT}}</span></div>
        <p class="sub">Fix age <span id="fixAge">{{FIXAGE}}</span> ms</p>
      </div>
    </section>
    <section class="actions">
      <a class="btn" href="/gpslog.csv">Download CSV</a>
      <a class="btn" href="/trace">GPS Trace</a>
      <a class="btn secondary" href="/data.json" target="_blank">Open Real-time JSON</a>
    </section>
    <table>
      <tbody>
        <tr><th>WiFi Link</th><td><span id="connectionBadge" class="badge">Online</span></td><td id="linkNote">Connected to {{WIFI_SSID}}</td></tr>
        <tr><th>SIM800L Status</th><td>{{SIM_STATUS}}</td><td>GPRS: {{GPRS_STATUS}}</td></tr>
        <tr><th>Last Data Sent</th><td>{{LAST_SEND_TIME}}</td><td>{{LAST_SEND_STATUS}}</td></tr>
        <tr><th>Log File</th><td>{{FILE_SIZE}}</td><td>Auto trims beyond {{MAX_SIZE}} KB</td></tr>
        <tr><th>AP Access</th><td>{{AP_SSID}} · {{AP_IP}}</td><td>Use direct AP when hotspot unavailable</td></tr>
      </tbody>
    </table>
    <h2 style="margin:32px 0 12px;font-size:1.2rem;letter-spacing:0.1em;text-transform:uppercase;color:#94a3b8;">System Information</h2>
    <table>
      <tbody>
        <tr><th>Chip Model</th><td>{{CHIP_MODEL}}</td><td>CPU @ {{CPU_FREQ}}</td></tr>
        <tr><th>Memory (Heap)</th><td>{{FREE_HEAP}} free / {{TOTAL_HEAP}} total</td><td>{{USED_HEAP}} in use</td></tr>
        <tr><th>Flash Memory</th><td>{{FLASH_SIZE}}</td><td>Running @ {{FLASH_SPEED}}</td></tr>
        <tr><th>LittleFS Storage</th><td>{{FS_USED}} used / {{FS_TOTAL}} total</td><td>{{FS_FREE}} available</td></tr>
        <tr><th>System Uptime</th><td>{{UPTIME}}</td><td>Since last reboot</td></tr>
      </tbody>
    </table>
  </main>
  <script>
  const connectionBadge=document.getElementById('connectionBadge');
  const linkNote=document.getElementById('linkNote');
  function setBadge(online,ssid){
    if(online){connectionBadge.classList.remove('offline');connectionBadge.textContent='Online';linkNote.textContent='Connected to '+(ssid||'—');}
    else{connectionBadge.classList.add('offline');connectionBadge.textContent='Offline';linkNote.textContent='Awaiting known hotspot';}
  }
  async function refresh(){
    try{
      const res=await fetch('/data.json');
      if(!res.ok) return;
      const data=await res.json();
      document.getElementById('lat').textContent=data.lat;
      document.getElementById('lng').textContent=data.lng;
      document.getElementById('lastTime').textContent=data.time;
      document.getElementById('speed').textContent=data.speedKmph;
      document.getElementById('course').textContent=data.courseDeg;
      document.getElementById('altitude').textContent=data.altMeters;
      document.getElementById('hdop').textContent=data.hdop;
      document.getElementById('satellites').textContent=data.satellites;
      document.getElementById('fixAge').textContent=data.fixAgeMs;
      document.getElementById('wifiIp').textContent=data.wifiIp;
      setBadge(data.wifiConnected, data.wifiSsid);
    }catch(err){console.log(err);}
  }
  setBadge(true,'{{WIFI_SSID}}');
  setInterval(refresh,4000);
  </script>
  </body>
  </html>
  )rawliteral");
    page.replace("{{WIFI_SSID}}", stationConnected ? currentStationSsid : String("Not connected"));
    page.replace("{{WIFI_IP}}", stationIpStr);
    page.replace("{{AP_SSID}}", String(ssid));
    page.replace("{{AP_IP}}", apIpStr);
    page.replace("{{TRIM_THRESHOLD}}", String(STORAGE_HIGH_WATERMARK * 100.0f, 0));
    page.replace("{{HEARTBEAT}}", String(LOG_MAX_INTERVAL_MS / 1000));
    page.replace("{{LAT}}", lastLat);
    page.replace("{{LNG}}", lastLng);
    page.replace("{{TIME}}", lastTime);
    page.replace("{{SPEED}}", lastSpeedKmph);
    page.replace("{{COURSE}}", lastCourseDeg);
    page.replace("{{ALT}}", lastAltitudeMeters);
    page.replace("{{HDOP}}", lastHdop);
    page.replace("{{SAT}}", lastSatellites);
    page.replace("{{FIXAGE}}", lastFixAgeMs);
    page.replace("{{FILE_SIZE}}", formatBytes(fileSize));
    page.replace("{{MAX_SIZE}}", String(static_cast<int>(MAX_LOG_FILE_SIZE / 1024)));
    page.replace("{{SIM_STATUS}}", sim800lStatus);
    page.replace("{{GPRS_STATUS}}", gprsStatus);
    page.replace("{{LAST_SEND_TIME}}", lastSendTime);
    page.replace("{{LAST_SEND_STATUS}}", lastSendStatus);
    page.replace("{{CHIP_MODEL}}", chipModelStr);
    page.replace("{{CPU_FREQ}}", cpuFreqStr);
    page.replace("{{FREE_HEAP}}", freeHeapStr);
    page.replace("{{TOTAL_HEAP}}", totalHeapStr);
    page.replace("{{USED_HEAP}}", usedHeapStr);
    page.replace("{{FLASH_SIZE}}", flashSizeStr);
    page.replace("{{FLASH_SPEED}}", flashSpeedStr);
    page.replace("{{FS_USED}}", littleFSUsedStr);
    page.replace("{{FS_TOTAL}}", littleFSTotalStr);
    page.replace("{{FS_FREE}}", littleFSFreeStr);
    page.replace("{{UPTIME}}", uptimeStr);
    return page;
  }

  void handleDataJson(AsyncWebServerRequest *request) {
    updateSystemInfo(); // Update system info before sending JSON
    size_t fileSize = 0;
    if (LittleFS.exists(dataPath)) {
      File f = LittleFS.open(dataPath, FILE_READ);
      if (f) {
        fileSize = f.size();
        f.close();
      }
    }
    String json = "{";
    json += "\"lat\":\"" + lastLat + "\",";
    json += "\"lng\":\"" + lastLng + "\",";
    json += "\"time\":\"" + lastTime + "\",";
    json += "\"speedKmph\":\"" + lastSpeedKmph + "\",";
    json += "\"altMeters\":\"" + lastAltitudeMeters + "\",";
    json += "\"courseDeg\":\"" + lastCourseDeg + "\",";
    json += "\"satellites\":\"" + lastSatellites + "\",";
    json += "\"hdop\":\"" + lastHdop + "\",";
    json += "\"fixAgeMs\":\"" + lastFixAgeMs + "\",";
    json += "\"wifiConnected\":" + String(stationConnected ? "true" : "false") + ",";
    json += "\"wifiSsid\":\"" + (stationConnected ? currentStationSsid : String("")) + "\",";
    json += "\"wifiIp\":\"" + stationIpStr + "\",";
    json += "\"apIp\":\"" + apIpStr + "\",";
    json += "\"sim800lStatus\":\"" + sim800lStatus + "\",";
    json += "\"gprsStatus\":\"" + gprsStatus + "\",";
    json += "\"lastSendTime\":\"" + lastSendTime + "\",";
    json += "\"lastSendStatus\":\"" + lastSendStatus + "\",";
    json += "\"logSizeBytes\":" + String((unsigned long)fileSize) + ",";
    json += "\"maxLogBytes\":" + String((unsigned long)MAX_LOG_FILE_SIZE) + ",";
    json += "\"chipModel\":\"" + chipModelStr + "\",";
    json += "\"cpuFreq\":\"" + cpuFreqStr + "\",";
    json += "\"freeHeap\":\"" + freeHeapStr + "\",";
    json += "\"totalHeap\":\"" + totalHeapStr + "\",";
    json += "\"usedHeap\":\"" + usedHeapStr + "\",";
    json += "\"flashSize\":\"" + flashSizeStr + "\",";
    json += "\"flashSpeed\":\"" + flashSpeedStr + "\",";
    json += "\"fsUsed\":\"" + littleFSUsedStr + "\",";
    json += "\"fsTotal\":\"" + littleFSTotalStr + "\",";
    json += "\"fsFree\":\"" + littleFSFreeStr + "\",";
    json += "\"uptime\":\"" + uptimeStr + "\"";
    json += "}";
    request->send(200, "application/json", json);
  }

// Check storage usage and rotate/cleanup if needed
void manageStorageIfNeeded() {
  unsigned long now = millis();
  if (now - lastStorageCheckMillis < STORAGE_CHECK_INTERVAL_MS) return;
  lastStorageCheckMillis = now;

  size_t total = 0;
  size_t used = 0;
  
  // Get LittleFS stats directly
  total = LittleFS.totalBytes();
  used = LittleFS.usedBytes();

  float usedFrac = 0.0;
  if (total > 0) {
    usedFrac = (float)used / (float)total;
    Serial.printf("Storage used: %u / %u (%.1f%%)\n", (unsigned)used, (unsigned)total, usedFrac * 100.0);
  } else {
    Serial.println("Warning: LittleFS reports 0 total bytes");
  }

  bool trimmed = false;
  if (LittleFS.exists(dataPath)) {
    File f = LittleFS.open(dataPath, FILE_READ);
    if (f) {
      size_t sz = f.size();
      Serial.printf("Log file size: %u bytes\n", (unsigned)sz);
      f.close();
      if (sz >= MAX_LOG_FILE_SIZE) {
        trimLogFile();
        trimmed = true;
      }
    }
  }

  if (!trimmed && total > 0 && usedFrac >= STORAGE_HIGH_WATERMARK) {
    trimLogFile();
    trimmed = true;
    // Recalculate usage after trim
    total = LittleFS.totalBytes();
    used = LittleFS.usedBytes();
    usedFrac = total > 0 ? (float)used / (float)total : 0.0f;
  }

  if (total > 0 && usedFrac >= STORAGE_HIGH_WATERMARK) {
    Serial.println("Storage critically full, clearing current log file.");
    if (LittleFS.exists(dataPath)) {
      LittleFS.remove(dataPath);
    }
    File file = LittleFS.open(dataPath, FILE_WRITE);
    if (file) {
      file.println(CSV_HEADER);
      file.close();
    }
  }
}

// Function to append data to CSV file
void appendFile(fs::FS &fs, const char * path, const char * message) {
  // Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    // Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

// ====== GPS CONFIGURATION FUNCTIONS ======

// Calculate UBX checksum
void calcChecksum(unsigned char* CK, unsigned char* payload, uint8_t length) {
  CK[0] = 0;
  CK[1] = 0;
  for (uint8_t i = 0; i < length; i++) {
    CK[0] += payload[i];
    CK[1] += CK[0];
  }
}

// Set GPS Dynamic Platform Model
// Models: 0=Portable, 2=Stationary, 3=Pedestrian, 4=Automotive, 5=Sea, 6=Airborne<1g, 7=Airborne<2g, 8=Airborne<4g
bool setGPSDynamicModel(uint8_t model) {
  Serial.print("Setting GPS Dynamic Model to: ");
  Serial.println(model);

  // Flush GPS serial buffer completely
  Serial.println("Flushing GPS serial buffer...");
  unsigned long flushStart = millis();
  int flushed = 0;
  while (millis() - flushStart < 500) {
    if (gpsSerial.available()) {
      gpsSerial.read();
      flushed++;
    }
    delay(10);
  }
  Serial.printf("Flushed %d bytes from GPS buffer\n", flushed);
  delay(100); // Let GPS finish any pending output

  // UBX-CFG-NAV5 message to set dynamic platform model
  uint8_t payload[36] = {
    0x03, 0x00,  // Apply dynamic model + fix mode
    model,       // dynModel: 4 = Automotive
    0x03,        // fixMode: 3 = Auto 2D/3D
    0x00, 0x00, 0x00, 0x00,  // fixedAlt (not used)
    0x10, 0x27, 0x00, 0x00,  // fixedAltVar: 10000 (0.01m)
    0x05,        // minElev: 5 degrees
    0x00,        // drLimit: 0 (reserved)
    0xFA, 0x00,  // pDop: 25.0
    0xFA, 0x00,  // tDop: 25.0
    0x64, 0x00,  // pAcc: 100m
    0x2C, 0x01,  // tAcc: 300m
    0x00,        // staticHoldThresh: 0 cm/s
    0x3C,        // dgnssTimeout: 60s
    0x00,        // cnoThreshNumSVs: 0
    0x00,        // cnoThresh: 0 dBHz
    0x00, 0x00,  // reserved
    0xC8, 0x00,  // staticHoldMaxDist: 200m
    0x00,        // utcStandard: 0 (auto)
    0x00, 0x00, 0x00, 0x00, 0x00  // reserved
  };

  // Build complete UBX message
  uint8_t message[44];
  message[0] = 0xB5;  // UBX sync char 1
  message[1] = 0x62;  // UBX sync char 2
  message[2] = 0x06;  // Class: CFG
  message[3] = 0x24;  // ID: NAV5
  message[4] = 0x24;  // Length LSB (36 bytes)
  message[5] = 0x00;  // Length MSB

  for (int i = 0; i < 36; i++) {
    message[6 + i] = payload[i];
  }

  uint8_t CK[2];
  uint8_t checksumPayload[40];
  checksumPayload[0] = 0x06;
  checksumPayload[1] = 0x24;
  checksumPayload[2] = 0x24;
  checksumPayload[3] = 0x00;
  for (int i = 0; i < 36; i++) {
    checksumPayload[4 + i] = payload[i];
  }
  calcChecksum(CK, checksumPayload, 40);
  message[42] = CK[0];
  message[43] = CK[1];

  // Send UBX command (single attempt with longer wait)
  Serial.println("Sending UBX-CFG-NAV5 command to GPS...");
  for (int i = 0; i < 44; i++) {
    gpsSerial.write(message[i]);
  }
  gpsSerial.flush(); // Ensure all bytes are transmitted

  // Wait for ACK response with improved detection
  Serial.println("Waiting for ACK from GPS...");
  delay(200); // Short delay before checking
  unsigned long start = millis();
  bool ackReceived = false;
  uint8_t ackBuffer[100]; // Larger buffer to capture response
  int ackIndex = 0;
  int bytesRead = 0;
  
  while (millis() - start < 3000) { // 3 second timeout
    if (gpsSerial.available()) {
      uint8_t c = gpsSerial.read();
      bytesRead++;
      
      // Store in circular buffer
      ackBuffer[ackIndex] = c;
      ackIndex = (ackIndex + 1) % 100;
      
      // Look for ACK-ACK pattern: 0xB5 0x62 0x05 0x01
      for (int i = 0; i < 97; i++) {
        int idx = (ackIndex + i) % 100;
        if (ackBuffer[idx] == 0xB5 && 
            ackBuffer[(idx+1)%100] == 0x62 && 
            ackBuffer[(idx+2)%100] == 0x05 && 
            ackBuffer[(idx+3)%100] == 0x01) {
          ackReceived = true;
          Serial.printf("ACK found after reading %d bytes!\n", bytesRead);
          break;
        }
      }
      if (ackReceived) break;
    }
    delay(1);
  }

  if (!ackReceived) {
    Serial.printf("Timeout after reading %d bytes (no ACK pattern found)\n", bytesRead);
  }

  if (ackReceived) {
    Serial.println("✓ GPS Dynamic Model set successfully (ACK received)");
    
    // Flush buffer again before saving
    while (gpsSerial.available()) gpsSerial.read();
    delay(100);
    
    // Save configuration to GPS flash memory
    uint8_t saveCfg[] = {
      0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00,
      0x00, 0x00, 0x00, 0x00,  // clearMask
      0x1F, 0x1F, 0x00, 0x00,  // saveMask: save all
      0x00, 0x00, 0x00, 0x00,  // loadMask
      0x1F,  // deviceMask: save to flash/EEPROM
      0x00, 0x00   // Checksum (will calculate)
    };
    uint8_t saveCK[2];
    uint8_t savePayload[17];
    savePayload[0] = 0x06;
    savePayload[1] = 0x09;
    savePayload[2] = 0x0D;
    savePayload[3] = 0x00;
    for (int i = 0; i < 13; i++) savePayload[4 + i] = saveCfg[6 + i];
    calcChecksum(saveCK, savePayload, 17);
    saveCfg[19] = saveCK[0];
    saveCfg[20] = saveCK[1];
    
    Serial.println("Saving configuration to GPS flash memory...");
    for (int i = 0; i < 21; i++) gpsSerial.write(saveCfg[i]);
    gpsSerial.flush();
    delay(1000);
    
    Serial.println("✓ Configuration saved. GPS will retain Automotive mode after power cycle.");
    return true;
  } else {
    Serial.println("⚠ Warning: No ACK received from GPS (timeout)");
    Serial.println("  GPS may still be working in default mode.");
    Serial.println("  Possible causes:");
    Serial.println("  - RX/TX wires swapped (check GPIO16→GPS_TX, GPIO17→GPS_RX)");
    Serial.println("  - GPS not fully booted yet (increase delay in setup)");
    Serial.println("  - Non u-blox GPS module (this only works with u-blox NEO-6M)");
    Serial.println("  - Defective GPS module or incompatible clone");
    Serial.println("  The tracker will continue working with default GPS settings.");
    return false;
  }
}

// Query current GPS Dynamic Model (optional - for verification)
void queryGPSDynamicModel() {
  Serial.println("Querying GPS Dynamic Model...");
  
  // UBX-CFG-NAV5 poll request
  uint8_t pollMsg[] = {
    0xB5, 0x62,  // Header
    0x06, 0x24,  // CFG-NAV5
    0x00, 0x00,  // Length: 0 (poll)
    0x00, 0x00   // Checksum (will calculate)
  };
  
  // Calculate checksum
  uint8_t CK[2];
  uint8_t payload[4] = {0x06, 0x24, 0x00, 0x00};
  calcChecksum(CK, payload, 4);
  pollMsg[6] = CK[0];
  pollMsg[7] = CK[1];
  
  // Send poll request
  for (int i = 0; i < 8; i++) {
    gpsSerial.write(pollMsg[i]);
  }
  
  delay(500);
  
  // Read response (for debugging)
  Serial.println("GPS Response:");
  unsigned long start = millis();
  while (millis() - start < 1000 && gpsSerial.available()) {
    uint8_t c = gpsSerial.read();
    Serial.print(c, HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ====== SIM800L FUNCTIONS ======

// Send AT command to SIM800L and wait for response
String sendATCommand(const char* cmd, unsigned long timeout = 1000) {
  sim800lSerial.println(cmd);
  Serial.print("AT Command: ");
  Serial.println(cmd);
  
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (sim800lSerial.available()) {
      char c = sim800lSerial.read();
      response += c;
    }
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
      break;
    }
    delay(10);
  }
  
  Serial.print("Response: ");
  Serial.println(response);
  return response;
}

// Initialize SIM800L module
bool initSIM800L() {
  Serial.println("Initializing SIM800L...");
  sim800lStatus = "Initializing";
  
  // Test AT communication
  for (int i = 0; i < 3; i++) {
    String resp = sendATCommand("AT", 2000);
    if (resp.indexOf("OK") != -1) {
      Serial.println("SIM800L responded to AT");
      break;
    }
    delay(1000);
  }
  
  // Check SIM card
  String simResp = sendATCommand("AT+CPIN?", 3000);
  if (simResp.indexOf("READY") == -1) {
    Serial.println("SIM card not ready!");
    sim800lStatus = "SIM Error";
    return false;
  }
  
  // Check network registration
  delay(2000);
  for (int i = 0; i < 10; i++) {
    String regResp = sendATCommand("AT+CREG?", 2000);
    if (regResp.indexOf("+CREG: 0,1") != -1 || regResp.indexOf("+CREG: 0,5") != -1) {
      Serial.println("Registered on network");
      sim800lStatus = "Network OK";
      sim800lReady = true;
      return true;
    }
    Serial.println("Waiting for network registration...");
    delay(2000);
  }
  
  sim800lStatus = "No Network";
  Serial.println("Failed to register on network");
  return false;
}

// Connect to GPRS
bool connectGPRS() {
  Serial.println("Connecting to GPRS...");
  gprsStatus = "Connecting";
  
  // Attach to GPRS
  sendATCommand("AT+CGATT=1", 5000);
  delay(1000);
  
  // Set connection type to GPRS
  sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 2000);
  
  // Set APN
  String apnCmd = "AT+SAPBR=3,1,\"APN\",\"" + String(apn) + "\"";
  sendATCommand(apnCmd.c_str(), 2000);
  
  // Set username if provided
  if (strlen(gprsUser) > 0) {
    String userCmd = "AT+SAPBR=3,1,\"USER\",\"" + String(gprsUser) + "\"";
    sendATCommand(userCmd.c_str(), 2000);
  }
  
  // Set password if provided
  if (strlen(gprsPass) > 0) {
    String passCmd = "AT+SAPBR=3,1,\"PWD\",\"" + String(gprsPass) + "\"";
    sendATCommand(passCmd.c_str(), 2000);
  }
  
  // Open GPRS context
  String openResp = sendATCommand("AT+SAPBR=1,1", 10000);
  delay(2000);
  
  // Query GPRS context
  String queryResp = sendATCommand("AT+SAPBR=2,1", 3000);
  if (queryResp.indexOf("1,1") != -1) {
    Serial.println("GPRS connected successfully");
    gprsConnected = true;
    gprsStatus = "Connected";
    return true;
  }
  
  Serial.println("GPRS connection failed");
  gprsStatus = "Failed";
  gprsConnected = false;
  return false;
}

// Send GPS data to webserver via HTTP POST
bool sendGPSDataToServer() {
  if (!gprsConnected || !gps.location.isValid()) {
    Serial.println("Cannot send data: GPRS not connected or GPS invalid");
    lastSendStatus = "Failed - No connection";
    return false;
  }
  
  Serial.println("Sending GPS data to server...");
  lastSendStatus = "Sending...";
  
  // Prepare JSON payload
  String jsonData = "{";
  jsonData += "\"latitude\":" + String(gps.location.lat(), 6) + ",";
  jsonData += "\"longitude\":" + String(gps.location.lng(), 6) + ",";
  jsonData += "\"altitude\":" + String(gps.altitude.meters(), 2) + ",";
  jsonData += "\"speed\":" + String(gps.speed.kmph(), 2) + ",";
  jsonData += "\"course\":" + String(gps.course.deg(), 2) + ",";
  jsonData += "\"satellites\":" + String(gps.satellites.value()) + ",";
  jsonData += "\"hdop\":" + String(gps.hdop.hdop(), 2) + ",";
  jsonData += "\"timestamp\":\"" + lastTime + "\"";
  jsonData += "}";
  
  Serial.print("Payload: ");
  Serial.println(jsonData);
  
  // Initialize HTTP
  sendATCommand("AT+HTTPTERM", 2000);
  delay(500);
  sendATCommand("AT+HTTPINIT", 3000);
  delay(500);
  
  // Set HTTP parameters
  sendATCommand("AT+HTTPPARA=\"CID\",1", 2000);
  
  String urlCmd = "AT+HTTPPARA=\"URL\",\"" + String(serverUrl) + "\"";
  sendATCommand(urlCmd.c_str(), 2000);
  
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 2000);
  
  // Set data to send
  String dataCmd = "AT+HTTPDATA=" + String(jsonData.length()) + ",10000";
  sim800lSerial.println(dataCmd);
  Serial.println(dataCmd);
  delay(500);
  
  // Wait for DOWNLOAD prompt
  unsigned long start = millis();
  bool downloadReady = false;
  while (millis() - start < 3000) {
    if (sim800lSerial.available()) {
      String resp = sim800lSerial.readString();
      Serial.print(resp);
      if (resp.indexOf("DOWNLOAD") != -1) {
        downloadReady = true;
        break;
      }
    }
    delay(10);
  }
  
  if (!downloadReady) {
    Serial.println("Failed to get DOWNLOAD prompt");
    sendATCommand("AT+HTTPTERM", 2000);
    lastSendStatus = "Failed - Download";
    return false;
  }
  
  // Send the JSON data
  sim800lSerial.print(jsonData);
  delay(500);
  
  // Execute HTTP POST
  String postResp = sendATCommand("AT+HTTPACTION=1", 15000);
  delay(3000);
  
  // Read HTTP response
  String readResp = sendATCommand("AT+HTTPREAD", 5000);
  
  // Terminate HTTP
  sendATCommand("AT+HTTPTERM", 2000);
  
  // Check if successful
  if (postResp.indexOf("+HTTPACTION: 1,200") != -1 || postResp.indexOf("+HTTPACTION: 1,201") != -1) {
    Serial.println("Data sent successfully!");
    lastSendStatus = "Success";
    lastSendTime = lastTime;
    return true;
  } else {
    Serial.println("HTTP POST failed");
    lastSendStatus = "Failed - HTTP";
    return false;
  }
}

// Maintain GPRS connection
void maintainGPRS() {
  unsigned long now = millis();
  
  // Periodically check GPRS connection
  if (now - lastGprsCheckMillis >= GPRS_CHECK_INTERVAL_MS) {
    lastGprsCheckMillis = now;
    
    if (!gprsConnected && sim800lReady) {
      connectGPRS();
    } else if (gprsConnected) {
      // Check if still connected
      String queryResp = sendATCommand("AT+SAPBR=2,1", 3000);
      if (queryResp.indexOf("1,1") == -1) {
        Serial.println("GPRS connection lost, reconnecting...");
        gprsConnected = false;
        gprsStatus = "Reconnecting";
        connectGPRS();
      }
    }
  }
  
  // Send data periodically if GPS has valid fix
  if (gprsConnected && gps.location.isValid() && 
      now - lastDataSendMillis >= DATA_SEND_INTERVAL_MS) {
    lastDataSendMillis = now;
    sendGPSDataToServer();
  }
}

// ====== END SIM800L FUNCTIONS ======

// Handle root page (show last location)
void handleRoot(AsyncWebServerRequest *request) {
  if (stationConnected) {
    request->send(200, "text/html", generateAdvancedPage());
  } else {
    request->send(200, "text/html", generateMinimalPage());
  }
}

// Handle CSV download
void handleDownload(AsyncWebServerRequest *request) {
  if (LittleFS.exists(dataPath)) {
    request->send(LittleFS, dataPath, "text/csv", true);
  } else {
    request->send(404, "text/plain", "File not found");
  }
}

// Handle GPS Trace page with interactive map
void handleTrace(AsyncWebServerRequest *request) {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>GPS Trace</title>";
  page += "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>";
  page += "<style>";
  page += "body{margin:0;font-family:Arial,sans-serif;background:#0b0d17;color:#f5f7ff;display:flex;flex-direction:column;height:100vh;overflow:hidden;}";
  page += "header{padding:16px 24px;background:rgba(15,23,42,0.95);border-bottom:1px solid rgba(148,163,184,0.22);display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px;}";
  page += "h1{margin:0;font-size:1.5rem;color:#f8fbff;}";
  page += ".controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center;}";
  page += ".btn{padding:8px 16px;border-radius:999px;text-decoration:none;background:#5c6cff;color:white;border:none;cursor:pointer;font-size:0.9rem;}";
  page += ".btn.secondary{background:rgba(148,163,184,0.22);}";
  page += ".btn.upload{background:#10b981;position:relative;overflow:hidden;}";
  page += ".btn.upload input{position:absolute;top:0;left:0;width:100%;height:100%;opacity:0;cursor:pointer;}";
  page += ".btn.theme{background:#8b5cf6;}";
  page += ".btn.live-active{background:#10b981;animation:pulse 2s infinite;}";
  page += "@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.7;}}";
  page += "#map{flex:1;width:100%;position:relative;}";
  page += "#info{position:absolute;top:80px;right:16px;background:rgba(15,23,42,0.95);padding:16px;border-radius:12px;max-width:250px;z-index:1000;border:1px solid rgba(148,163,184,0.22);max-height:calc(100vh - 100px);overflow-y:auto;}";
  page += "#infoToggle{display:none;position:fixed;bottom:16px;right:16px;width:56px;height:56px;border-radius:50%;background:#5c6cff;color:white;border:none;font-size:1.5rem;box-shadow:0 4px 12px rgba(0,0,0,0.3);z-index:999;cursor:pointer;}";
  page += "@media (max-width:768px){";
  page += "#info{display:none;top:auto;bottom:16px;right:16px;left:16px;max-width:none;max-height:60vh;font-size:0.85rem;}";
  page += "#info.show{display:block;}";
  page += "#infoToggle{display:block;}";
  page += "#info h3{font-size:0.9rem;}";
  page += "#info p{font-size:0.75rem;margin:2px 0;}";
  page += ".legend-item{font-size:0.7rem;}";
  page += "header{padding:12px 16px;}";
  page += "h1{font-size:1.2rem;}";
  page += ".btn{padding:6px 12px;font-size:0.8rem;}";
  page += "}";
  page += "#info h3{margin:0 0 8px;font-size:1rem;}";
  page += "#info p{margin:4px 0;font-size:0.85rem;}";
  page += ".legend{margin-top:12px;padding-top:12px;border-top:1px solid rgba(148,163,184,0.22);}";
  page += ".legend-item{display:flex;align-items:center;gap:8px;margin:4px 0;font-size:0.8rem;}";
  page += ".legend-color{width:20px;height:4px;border-radius:2px;}";
  page += "#loading{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(15,23,42,0.95);padding:24px;border-radius:12px;z-index:2000;text-align:center;}";
  page += "#source{margin-top:8px;font-size:0.75rem;color:#94a3b8;padding-top:8px;border-top:1px solid rgba(148,163,184,0.22);}";
  page += "</style></head><body>";
  page += "<header><h1>GPS Trace Map</h1><div class='controls'>";
  page += "<button class='btn' onclick='loadDefault()'>Device Log</button> ";
  page += "<button class='btn' id='liveBtn' onclick='toggleLive()'>🚗 Live Track</button> ";
  page += "<label class='btn upload'>Upload CSV<input type='file' accept='.csv' onchange='loadCustomFile(this)'></label> ";
  page += "<button class='btn theme' id='themeBtn' onclick='toggleTheme()'>🌙 Night</button> ";
  page += "<button class='btn secondary' onclick='location.reload()'>Refresh</button> ";
  page += "<a class='btn secondary' href='/'>Back</a>";
  page += "</div></header>";
  page += "<div id='map'></div>";
  page += "<button id='infoToggle' onclick='toggleInfo()'>📊</button>";
  page += "<div id='info'><h3>Route Info</h3>";
  page += "<p id='liveStatus' style='display:none;color:#10b981;font-weight:bold;margin-bottom:8px;'>🔴 Live Tracking</p>";
  page += "<p id='pointCount'>Points: 0</p>";
  page += "<p id='distance'>Distance: 0 km</p>";
  page += "<p id='maxSpeed'>Max Speed: 0 km/h</p>";
  page += "<p id='currentSpeed' style='display:none;'>Current: 0 km/h</p>";
  page += "<div class='legend'>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#22c55e'></div>&lt; 20 km/h</div>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#84cc16'></div>20-40 km/h</div>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#eab308'></div>40-60 km/h</div>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#f97316'></div>60-80 km/h</div>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#ef4444'></div>80-100 km/h</div>";
  page += "<div class='legend-item'><div class='legend-color' style='background:#dc2626'></div>&gt; 100 km/h</div>";
  page += "</div>";
  page += "<p id='source'>Source: Device</p>";
  page += "</div>";
  page += "<div id='loading'>Loading GPS data...</div>";
  page += "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>";
  page += "<script>";
  page += "var map=L.map('map').setView([0,0],2),mapLayers=[],isDarkMode=false,tileLayer,liveMarker=null,liveInterval=null,isLiveMode=false,userInteracted=false;";
  page += "tileLayer=L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19}).addTo(map);";
  page += "var carIcon=L.divIcon({className:'car-icon',html:'<div style=\"font-size:32px;text-shadow:0 0 3px #000;\">🚗</div>',iconSize:[32,32],iconAnchor:[16,16]});";
  page += "map.on('zoomstart',function(){userInteracted=true;});";
  page += "map.on('movestart',function(){userInteracted=true;});";
  page += "function toggleInfo(){";
  page += "var info=document.getElementById('info');";
  page += "info.classList.toggle('show');}";
  page += "document.getElementById('map').addEventListener('click',function(e){";
  page += "if(window.innerWidth<=768&&e.target.id==='map'){";
  page += "document.getElementById('info').classList.remove('show');}});";
  page += "function toggleLive(){";
  page += "isLiveMode=!isLiveMode;";
  page += "var btn=document.getElementById('liveBtn'),status=document.getElementById('liveStatus'),currSpd=document.getElementById('currentSpeed');";
  page += "if(isLiveMode){";
  page += "userInteracted=false;";
  page += "btn.classList.add('live-active');";
  page += "btn.innerHTML='⏹️ Stop Live';";
  page += "status.style.display='block';";
  page += "currSpd.style.display='block';";
  page += "clearMap();";
  page += "document.getElementById('source').textContent='Source: Live GPS';";
  page += "updateLivePosition();";
  page += "liveInterval=setInterval(updateLivePosition,3000);";
  page += "}else{";
  page += "btn.classList.remove('live-active');";
  page += "btn.innerHTML='🚗 Live Track';";
  page += "status.style.display='none';";
  page += "currSpd.style.display='none';";
  page += "if(liveInterval)clearInterval(liveInterval);";
  page += "if(liveMarker)map.removeLayer(liveMarker);";
  page += "liveMarker=null;";
  page += "}}";
  page += "function updateLivePosition(){";
  page += "fetch('/data.json').then(r=>r.json()).then(data=>{";
  page += "var lat=parseFloat(data.lat),lng=parseFloat(data.lng),spd=parseFloat(data.speedKmph)||0;";
  page += "if(isNaN(lat)||isNaN(lng)||lat==0||lng==0)return;";
  page += "if(liveMarker)map.removeLayer(liveMarker);";
  page += "liveMarker=L.marker([lat,lng],{icon:carIcon}).addTo(map);";
  page += "liveMarker.bindPopup('<b>Current Position</b><br>Speed: '+spd+' km/h<br>Time: '+data.time+'<br>Satellites: '+data.satellites);";
  page += "if(!userInteracted){map.setView([lat,lng],16);}";
  page += "document.getElementById('currentSpeed').innerHTML='<b>Current: '+spd+' km/h</b>';";
  page += "document.getElementById('liveStatus').innerHTML='🔴 Live - Updated: '+new Date().toLocaleTimeString();";
  page += "}).catch(e=>console.log('Live update error:',e));}";
  page += "function toggleTheme(){";
  page += "isDarkMode=!isDarkMode;";
  page += "var btn=document.getElementById('themeBtn');";
  page += "map.removeLayer(tileLayer);";
  page += "if(isDarkMode){";
  page += "tileLayer=L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',{maxZoom:19,attribution:'&copy; CARTO'}).addTo(map);";
  page += "btn.innerHTML='☀️ Day';";
  page += "}else{";
  page += "tileLayer=L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19}).addTo(map);";
  page += "btn.innerHTML='🌙 Night';";
  page += "}}";
  page += "function getSpeedColor(s){";
  page += "if(s<20)return'#22c55e';";
  page += "if(s<40)return'#84cc16';";
  page += "if(s<60)return'#eab308';";
  page += "if(s<80)return'#f97316';";
  page += "if(s<100)return'#ef4444';";
  page += "return'#dc2626';}";
  page += "function calcDist(lat1,lon1,lat2,lon2){";
  page += "var R=6371,dLat=(lat2-lat1)*Math.PI/180,dLon=(lon2-lon1)*Math.PI/180;";
  page += "var a=Math.sin(dLat/2)*Math.sin(dLat/2)+Math.cos(lat1*Math.PI/180)*Math.cos(lat2*Math.PI/180)*Math.sin(dLon/2)*Math.sin(dLon/2);";
  page += "return R*2*Math.atan2(Math.sqrt(a),Math.sqrt(1-a));}";
  page += "function clearMap(){mapLayers.forEach(l=>map.removeLayer(l));mapLayers=[];}";
  page += "function renderRoute(csv,source){";
  page += "clearMap();";
  page += "document.getElementById('loading').style.display='block';";
  page += "document.getElementById('loading').innerHTML='Processing route...';";
  page += "setTimeout(function(){";
  page += "var lines=csv.trim().split('\\n'),pts=[],dist=0,maxSpd=0;";
  page += "if(lines.length<2){document.getElementById('loading').innerHTML='<p>No GPS data</p><p>Expected CSV format:<br>Time,Lat,Lng,Alt,Speed,Satellites</p>';return;}";
  page += "for(var i=1;i<lines.length;i++){";
  page += "var c=lines[i].split(',');if(c.length<5)continue;";
  page += "var lat=parseFloat(c[1]),lng=parseFloat(c[2]),spd=parseFloat(c[4])||0;";
  page += "if(!isNaN(lat)&&!isNaN(lng)&&lat!=0&&lng!=0){";
  page += "pts.push({time:c[0],lat:lat,lng:lng,alt:parseFloat(c[3])||0,speed:spd,sat:c[5]||'N/A'});";
  page += "if(spd>maxSpd)maxSpd=spd;}}";
  page += "if(pts.length==0){document.getElementById('loading').innerHTML='<p>No valid coordinates found</p><p>Check CSV format</p>';return;}";
  page += "for(var i=1;i<pts.length;i++){dist+=calcDist(pts[i-1].lat,pts[i-1].lng,pts[i].lat,pts[i].lng);";
  page += "var avgSpd=(pts[i-1].speed+pts[i].speed)/2,col=getSpeedColor(avgSpd);";
  page += "var ln=L.polyline([[pts[i-1].lat,pts[i-1].lng],[pts[i].lat,pts[i].lng]],{color:col,weight:4,opacity:0.8}).addTo(map);";
  page += "ln.bindPopup('<b>Seg '+i+'</b><br>Speed: '+avgSpd.toFixed(1)+' km/h<br>Time: '+pts[i].time+'<br>Alt: '+pts[i].alt.toFixed(1)+'m<br>Sat: '+pts[i].sat);";
  page += "mapLayers.push(ln);}";
  page += "var startMrk=L.marker([pts[0].lat,pts[0].lng]).addTo(map).bindPopup('<b>Start</b><br>'+pts[0].time);";
  page += "var endMrk=L.marker([pts[pts.length-1].lat,pts[pts.length-1].lng]).addTo(map).bindPopup('<b>End</b><br>'+pts[pts.length-1].time);";
  page += "mapLayers.push(startMrk);mapLayers.push(endMrk);";
  page += "if(!userInteracted){map.fitBounds(L.latLngBounds(pts.map(p=>[p.lat,p.lng])),{padding:[50,50]});}";
  page += "document.getElementById('pointCount').textContent='Points: '+pts.length;";
  page += "document.getElementById('distance').textContent='Distance: '+dist.toFixed(2)+' km';";
  page += "document.getElementById('maxSpeed').textContent='Max Speed: '+maxSpd.toFixed(1)+' km/h';";
  page += "document.getElementById('source').textContent='Source: '+source;";
  page += "document.getElementById('loading').style.display='none';";
  page += "},100);}";
  page += "function loadDefault(){";
  page += "userInteracted=false;";
  page += "fetch('/gpslog.csv').then(r=>r.text()).then(csv=>renderRoute(csv,'Device Log'))";
  page += ".catch(e=>{document.getElementById('loading').innerHTML='<p>Error loading device log</p><p>'+e.message+'</p>';});}";
  page += "function loadCustomFile(input){";
  page += "if(!input.files||!input.files[0])return;";
  page += "userInteracted=false;";
  page += "var file=input.files[0];";
  page += "if(!file.name.toLowerCase().endsWith('.csv')){alert('Please select a CSV file');return;}";
  page += "var reader=new FileReader();";
  page += "reader.onload=function(e){renderRoute(e.target.result,'Custom: '+file.name);};";
  page += "reader.onerror=function(){document.getElementById('loading').innerHTML='<p>Error reading file</p>';};";
  page += "reader.readAsText(file);}";
  page += "loadDefault();";
  page += "</script></body></html>";
  request->send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200); //comment for production
  Serial.println("Starting ESP32 GPS Datalogger (Async)...");

  // Initialize GPS Serial
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS Serial started");

  // Wait longer for GPS to boot (critical for UBX commands)
  Serial.println("Waiting for GPS to boot...");
  delay(3000);  // Increased to 3 seconds

  // Set dynamic model (single call with better error handling)
  Serial.println("Configuring GPS for automotive use...");
  bool gpsConfigured = setGPSDynamicModel(4);  // 4 = Automotive mode
  if (!gpsConfigured) {
    Serial.println("GPS will use default mode. Tracker functionality is NOT affected.");
  }
  delay(500);

  // Optional: Query to verify setting (uncomment to debug)
  // queryGPSDynamicModel();

  // Initialize SIM800L Serial
  sim800lSerial.begin(SIM800L_BAUD, SERIAL_8N1, SIM800L_RX, SIM800L_TX);
  Serial.println("SIM800L Serial started");
  delay(3000); // Give SIM800L time to boot up
  
  // Initialize SIM800L
  if (initSIM800L()) {
    Serial.println("SIM800L initialized successfully");
    // Connect to GPRS
    connectGPRS();
  } else {
    Serial.println("SIM800L initialization failed");
  }

  // Initialize LittleFS
  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS mount failed, formatting...");
    if (!LittleFS.begin(true)) {
      Serial.println("LittleFS format failed!");
      return;
    }
  }
  Serial.println("LittleFS initialized");
  
  // Print LittleFS info
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  Serial.printf("LittleFS: Total=%u, Used=%u, Free=%u\n", 
                (unsigned)totalBytes, (unsigned)usedBytes, (unsigned)(totalBytes - usedBytes));
  
  delay(100); // Give LittleFS time to fully initialize

  // Create CSV header if file doesn't exist
  if (!LittleFS.exists(dataPath)) {
    File file = LittleFS.open(dataPath, FILE_WRITE);
    if (file) {
      file.println(CSV_HEADER);
      file.close();
      Serial.println("CSV header created");
    }
  }

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);

  // Configure WiFi (AP + STA)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname("ulteraSpace");  // Set hostname for network identification
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWiFiEvent);

  WiFi.softAP(ssid, password);
  apIpStr = WiFi.softAPIP().toString();
  Serial.printf("AP SSID: %s | IP: %s\n", ssid, apIpStr.c_str());
  Serial.printf("Hostname set to: ulteraSpace\n");

  WiFi.disconnect(true);
  delay(100);
  lastWifiScanMillis = millis();
  attemptConnectToKnownNetworks();

  // Delay first storage check to avoid startup blocking
  lastStorageCheckMillis = millis() + 30000UL; // First check after 30 seconds

  // Setup async web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data.json", HTTP_GET, handleDataJson);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/trace", HTTP_GET, handleTrace);
  server.serveStatic("/gpslog.csv", LittleFS, "/gpslog.csv");
  server.begin();
  Serial.println("Async Web server started");
}

void loop() {
  // Read GPS data (non-blocking)
  while (gpsSerial.available() > 0) {
    digitalWrite(LED_PIN, HIGH);
    gps.encode(gpsSerial.read());
  }
  digitalWrite(LED_PIN, LOW);   // Turn OFF LED after reading
  // Check for location update
  if (gps.location.isUpdated()) {
    double curLat = gps.location.lat();
    double curLng = gps.location.lng();
    lastLat = String(curLat, 6);
    lastLng = String(curLng, 6);
    lastTime = String(gps.date.year()) + "/" + 
               String(gps.date.month()) + "/" + 
               String(gps.date.day()) + " " + 
               String(gps.time.hour()) + ":" + 
               String(gps.time.minute()) + ":" + 
               String(gps.time.second());

    if (gps.speed.isValid()) {
      lastSpeedKmph = String(gps.speed.kmph(), 2);
    } else {
      lastSpeedKmph = "N/A";
    }
    if (gps.altitude.isValid()) {
      lastAltitudeMeters = String(gps.altitude.meters(), 1);
    } else {
      lastAltitudeMeters = "N/A";
    }
    if (gps.course.isValid()) {
      lastCourseDeg = String(gps.course.deg(), 1);
    } else {
      lastCourseDeg = "N/A";
    }
    if (gps.satellites.isValid()) {
      lastSatellites = String(gps.satellites.value());
    } else {
      lastSatellites = "0";
    }
    if (gps.hdop.isValid()) {
      lastHdop = String(gps.hdop.hdop(), 1);
    } else {
      lastHdop = "N/A";
    }
    lastFixAgeMs = String(gps.location.age());

    unsigned long now = millis();
    bool shouldLog = false;
    // If we never logged before, log
    if (!hasLastLogged) {
      shouldLog = true;
    } else {
      // distance-based threshold
      double dist = haversineDistance(lastLoggedLat, lastLoggedLng, curLat, curLng);
      if (dist >= SIGNIFICANT_DISTANCE_METERS) {
        shouldLog = true;
      }
      // periodic heartbeat: log even if small movement after max interval
      if (!shouldLog && (now - lastLoggedMillis >= LOG_MAX_INTERVAL_MS)) {
        shouldLog = true;
      }
    }

    if (shouldLog) {
      // Prepare CSV row
      String dataMessage = lastTime + "," + 
                           String(curLat, 6) + "," + 
                           String(curLng, 6) + "," + 
                           String(gps.altitude.meters()) + "," + 
                           String(gps.speed.kmph()) + "," + 
                           String(gps.satellites.value()) + "\r\n";

      
      // Append to file
      appendFile(LittleFS, dataPath, dataMessage.c_str());

      // Update last logged state
      hasLastLogged = true;
      lastLoggedLat = curLat;
      lastLoggedLng = curLng;
      lastLoggedMillis = now;

      // Debug output
      Serial.println("GPS Logged: " + dataMessage);
    } else {
      Serial.println("Change not significant, skipping log.");
    }
  }

  if (gps.location.isValid()) {
    lastFixAgeMs = String(gps.location.age());
  } else {
    lastFixAgeMs = "Invalid";
  }

  // Keep WiFi policy responsive
  maintainWiFi();

  // Maintain GPRS connection and send data
  maintainGPRS();

  // Periodically check storage and rotate if necessary
  manageStorageIfNeeded();

  delay(100);  // Small delay to prevent overwhelming the serial
}