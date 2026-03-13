#include <WiFi.h>
#include "time.h"

// ============================================================
// WiFi Credentials - Update these with your network details
// ============================================================
const char* WIFI_SSID     = "sarasiruha300";
const char* WIFI_PASSWORD = "9885104058";

// ============================================================
// NTP Configuration - Primary servers
// ============================================================
const char* NTP_SERVER_1  = "pool.ntp.org";
const char* NTP_SERVER_2  = "time.nist.gov";

// Backup NTP server (used if all primary attempts fail)
const char* NTP_BACKUP_SERVER = "time.cloudflare.com";

const long  GMT_OFFSET_SEC      = 0;    // UTC offset in seconds (e.g. UTC+5:30 = 19800)
const int   DAYLIGHT_OFFSET_SEC = 0;   // Daylight saving offset in seconds (3600 if applicable)

// ============================================================
// Timing Constants
// ============================================================
const unsigned long PRINT_INTERVAL_MS   = 4000;           // Print every 4 seconds
const unsigned long SYNC_INTERVAL_MS    = 5 * 60 * 1000;  // Re-sync every 5 minutes
const int           NTP_MAX_ATTEMPTS    = 5;               // Max primary NTP attempts before giving up
const int           NTP_BACKUP_ATTEMPTS = 2;               // Attempts on backup server
const unsigned long NTP_ATTEMPT_DELAY   = 2000;            // Delay between NTP attempts (ms)
const unsigned long NTP_TIMEOUT_MS      = 10000;           // Timeout per attempt (ms)

unsigned long lastPrintTime = 0;
unsigned long lastSyncTime  = 0;
bool timeSynced = false;

// ============================================================
// Connect to WiFi
// ============================================================
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttempt > 15000) {
      Serial.println("\nFailed to connect to WiFi. Check credentials and try again.");
      return;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
// Helper: attempt NTP sync against a given server pair/single
// ============================================================
bool tryNTPServer(const char* server1, const char* server2, int maxAttempts, const char* label) {
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    Serial.print("[");
    Serial.print(label);
    Serial.print("] Attempt ");
    Serial.print(attempt);
    Serial.print(" of ");
    Serial.print(maxAttempts);
    Serial.println("...");

    if (server2 != nullptr) {
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, server1, server2);
    } else {
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, server1);
    }

    struct tm timeInfo;
    unsigned long startWait = millis();
    bool success = false;

    while (millis() - startWait < NTP_TIMEOUT_MS) {
      if (getLocalTime(&timeInfo)) {
        success = true;
        break;
      }
      delay(500);
    }

    if (success) {
      Serial.print("[");
      Serial.print(label);
      Serial.println("] Time synced successfully.");
      return true;
    }

    Serial.print("[");
    Serial.print(label);
    Serial.print("] Attempt ");
    Serial.print(attempt);
    Serial.println(" failed.");

    if (attempt < maxAttempts) {
      Serial.print("Waiting ");
      Serial.print(NTP_ATTEMPT_DELAY / 1000);
      Serial.println("s before next attempt...");
      delay(NTP_ATTEMPT_DELAY);
    }
  }
  return false;
}

// ============================================================
// Set fallback time to 01/01/2026 00:00:00
// ============================================================
void setFallbackTime() {
  Serial.println("All NTP sources failed. Setting fallback time: 00:00:00 - 01/01/2026");
  struct tm fallback = {};
  fallback.tm_year  = 2026 - 1900; // years since 1900
  fallback.tm_mon   = 0;           // January (0-indexed)
  fallback.tm_mday  = 1;
  fallback.tm_hour  = 0;
  fallback.tm_min   = 0;
  fallback.tm_sec   = 0;
  fallback.tm_isdst = -1;

  time_t t = mktime(&fallback);
  struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  Serial.println("Fallback time set. Clock is running but may not be accurate.");
}

// ============================================================
// Sync time via NTP:
//   1. Try primary servers (pool.ntp.org + time.nist.gov) x5
//   2. Try backup server  (time.cloudflare.com)           x2
//   3. Fall back to 01/01/2026 00:00:00
// ============================================================
bool syncTime() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting reconnect...");
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return false;
  }

  // --- Step 1: Primary NTP servers ---
  Serial.println("--- Trying primary NTP servers ---");
  if (tryNTPServer(NTP_SERVER_1, NTP_SERVER_2, NTP_MAX_ATTEMPTS, "Primary")) {
    return true;
  }

  // --- Step 2: Backup NTP server ---
  Serial.println("--- Primary failed. Trying backup NTP server (time.cloudflare.com) ---");
  if (tryNTPServer(NTP_BACKUP_SERVER, nullptr, NTP_BACKUP_ATTEMPTS, "Backup")) {
    return true;
  }

  // --- Step 3: Fallback to hardcoded date ---
  setFallbackTime();
  return true; // Return true so the clock still runs using fallback time
}

// ============================================================
// Print current time in HH:MM:SS - DD/MM/YYYY
// ============================================================
void printCurrentTime() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Failed to read local time.");
    return;
  }

  char timeStr[30];
  snprintf(timeStr, sizeof(timeStr),
    "%02d:%02d:%02d - %02d/%02d/%04d",
    timeInfo.tm_hour,
    timeInfo.tm_min,
    timeInfo.tm_sec,
    timeInfo.tm_mday,
    timeInfo.tm_mon + 1,   // tm_mon is 0-indexed
    timeInfo.tm_year + 1900 // tm_year is years since 1900
  );

  Serial.println(timeStr);
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-C3 Supermini - NTP Time Sync");
  Serial.println("====================================");

  connectWiFi();

  timeSynced = syncTime();

  lastSyncTime  = millis();
  lastPrintTime = millis();
}

// ============================================================
// Loop
// ============================================================
void loop() {
  unsigned long now = millis();

  // Re-sync every 5 minutes
  if (now - lastSyncTime >= SYNC_INTERVAL_MS) {
    timeSynced = syncTime();
    lastSyncTime = millis();
  }

  // Print time every 4 seconds
  if (timeSynced && (now - lastPrintTime >= PRINT_INTERVAL_MS)) {
    printCurrentTime();
    lastPrintTime = millis();
  }

  // Small delay to avoid busy-looping
  delay(100);
}
