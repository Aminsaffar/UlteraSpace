/*
 * Configuration Template for ESP32 GPS Tracker with SIM800L
 * 
 * Copy this file to config.h and update with your settings
 * Add config.h to .gitignore to keep credentials private
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi Configuration =====
// Access Point (Always active)
const char* WIFI_AP_SSID = "UlTeRa_SpAcE";
const char* WIFI_AP_PASSWORD = "12345678";

// Known Networks (for auto-connect)
// Add your trusted WiFi networks here
struct KnownNetwork {
  const char* ssid;
  const char* password;
};

const KnownNetwork knownNetworks[] = {
  {"YourHomeWiFi", "your-wifi-password"},
  {"YourOfficeWiFi", "office-password"}
};

// ===== SIM800L Configuration =====
// Your mobile carrier's APN settings
const char* GPRS_APN = "internet";           // e.g., "hologram", "fast.t-mobile.com"
const char* GPRS_USER = "";                  // Usually empty
const char* GPRS_PASSWORD = "";              // Usually empty

// ===== Server Configuration =====
// Your webserver endpoint for receiving GPS data
const char* SERVER_URL = "http://yourserver.com/api/gps";
const int SERVER_PORT = 80;

// ===== Data Sending Configuration =====
const unsigned long DATA_SEND_INTERVAL_MS = 30000;  // Send every 30 seconds
const unsigned long GPRS_CHECK_INTERVAL_MS = 60000; // Check GPRS every 60 seconds

// ===== GPS Configuration =====
const double SIGNIFICANT_DISTANCE_METERS = 10.0;    // Minimum distance to log
const unsigned long LOG_MAX_INTERVAL_MS = 30000;    // Maximum time between logs

#endif
