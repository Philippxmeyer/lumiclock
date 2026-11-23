#include <Arduino.h>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <time.h>
#include <vector>

static bool mockTimeAvailable = true;
static unsigned long mockTimeReadyAt = 0;
static int mockNtpStarts = 0;
static String mockTimezone;
static String mockNtpServer;
static std::vector<uint32_t> timeTimeouts;
static bool mockCoreBoundaryMiss = false;

time_t mockSystemTime(time_t* result) {
  time_t value = mockTimeAvailable && millis() >= mockTimeReadyAt ? 1795469400 : 0;
  if (result) *result = value;
  return value;
}

struct tm* mockLocalTime(const time_t* value, struct tm* info) {
  *info = {};
  if (*value == 0) {
    info->tm_year = 70;
    return info;
  }
  info->tm_year = 126;
  info->tm_mon = 10;
  info->tm_mday = 23;
  info->tm_hour = 14;
  info->tm_min = 30;
  return info;
}

void configTzTime(const char* timezone, const char* server) {
  ++mockNtpStarts;
  mockTimezone = timezone;
  mockNtpServer = server;
}

bool getLocalTime(struct tm* info, uint32_t timeout = 5000) {
  timeTimeouts.push_back(timeout);
  if (timeout == 0 && mockCoreBoundaryMiss) {
    delay(1);
    return false; // Core-Timeout-Schleife wird vor dem ersten Lesen übersprungen.
  }
  if (!mockTimeAvailable || millis() < mockTimeReadyAt) {
    // Der ESP32-Core wartet auch bei Timeout 0 intern einmal 10 ms.
    delay(timeout ? timeout : 10);
    return false;
  }
  *info = {};
  info->tm_year = 126;
  info->tm_mon = 10;
  info->tm_mday = 23;
  info->tm_hour = 14;
  info->tm_min = 30;
  return true;
}

#include "wifi_connection.h"
#define time mockSystemTime
#define localtime_r mockLocalTime
#include "ntp_client.h"
#undef time
#undef localtime_r

static int checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " << #expr << '\n'; \
  std::exit(1); } } while (false)

static void reset() {
  mockMillis = 0;
  WiFi = {};
  mockTimeAvailable = true;
  mockTimeReadyAt = 0;
  mockNtpStarts = 0;
  timeTimeouts.clear();
  mockCoreBoundaryMiss = false;
}

static void wifiConnection() {
  reset();
  CHECK(initializeWiFi("test-network", "test-only-password"));
  CHECK(WiFi.selectedMode == WIFI_STA);
  CHECK(WiFi.autoReconnect);
  CHECK(WiFi.beginCalls == 1);
  CHECK(WiFi.ssid == "test-network");
  CHECK(WiFi.password == "test-only-password");
  CHECK(mockMillis == 1000); // NTP darf nicht vor WLAN-Verbindung beginnen.
  CHECK(WiFi.status() == WL_CONNECTED);

  reset();
  WiFi.canConnect = false;
  CHECK(!initializeWiFi("test-network", "test-only-password"));
  CHECK(mockMillis == 20000);
  CHECK(WiFi.status() == WL_DISCONNECTED);

  reset();
  WiFi.modeWorks = false;
  CHECK(!initializeWiFi("test-network", "test-only-password"));
  CHECK(WiFi.beginCalls == 0);
  CHECK(mockMillis == 0);

  reset();
  WiFi.connectAfter = 20000;
  CHECK(initializeWiFi("test-network", "test-only-password"));
  CHECK(mockMillis == 20000); // Erfolg am letzten Poll ist ebenfalls Erfolg.

  reset();
  mockMillis = ULONG_MAX - 100;
  CHECK(initializeWiFi("test-network", "test-only-password"));
  CHECK(WiFi.status() == WL_CONNECTED); // millis()-Überlauf
}

static void ntpConnection() {
  reset();
  CHECK(initializeNTP());
  CHECK(mockMillis == 0);
  CHECK(mockNtpStarts == 1);
  CHECK(mockTimezone == TIMEZONE);
  CHECK(mockNtpServer == NTP_SERVER);

  reset();
  mockTimeReadyAt = 10000;
  CHECK(initializeNTP());
  CHECK(mockMillis >= 10000 && mockMillis <= 10010);
  for (uint32_t timeout : timeTimeouts) CHECK(timeout == 0);

  reset();
  mockTimeAvailable = false;
  CHECK(!initializeNTP());
  CHECK(mockMillis >= 10000 && mockMillis <= 10010); // kein 115-s-Start
  CHECK(mockNtpStarts == 1);
  for (uint32_t timeout : timeTimeouts) CHECK(timeout == 0);

  reset();
  mockTimeAvailable = false;
  CHECK(!initializeNTP(0)); // Ohne WLAN keine NTP-Warteschleife.
  CHECK(mockMillis == 0);
  CHECK(mockNtpStarts == 1); // Hintergrundsynchronisation trotzdem konfiguriert.
  CHECK(timeTimeouts.empty());
  mockTimeAvailable = true;
  CHECK(getFormattedHours() == "14"); // spätere Synchronisation wird übernommen
  CHECK(getFormattedMinutes() == "30");

  reset();
  mockTimeAvailable = false;
  CHECK(getFormattedHours() == "--");
  CHECK(getFormattedMinutes() == "--");
  CHECK(getCurrentHour() == -1);
  CHECK(getCurrentMinute() == -1);
  int day, month, year;
  getCurrentDate(day, month, year);
  CHECK(day == 1 && month == 1 && year == 2024);
  CHECK(mockMillis == 0); // keine Core-Warteschleife im laufenden Betrieb
  for (uint32_t timeout : timeTimeouts) CHECK(timeout == 0);

  reset();
  mockCoreBoundaryMiss = true;
  struct tm sample;
  CHECK(!getLocalTime(&sample, 0)); // Reproduktion des alten Millisekundenfehlers.
  timeTimeouts.clear();
  for (int i = 0; i < 64; ++i) {
    ++mockMillis;
    CHECK(getCurrentMinute() == 30);
    CHECK(getCurrentHour() == 14);
  }
  CHECK(timeTimeouts.empty()); // Die Firmware nutzt die fehleranfällige Abfrage nicht.
}

int main() {
  wifiConnection();
  ntpConnection();
  std::cout << checks << " Prüfungen bestanden (WLAN-Aufbau, Fehlerfälle, NTP-Zeitlimit).\n";
}
