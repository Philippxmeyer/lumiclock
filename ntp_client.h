#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <time.h>

// ---------------------------------------------------------------------------
// Zeitzone: Mitteleuropa mit automatischer Sommerzeit (POSIX-String)
// CET-1  = UTC+1 im Winter
// CEST   = UTC+2 im Sommer
// M3.5.0 = letzter Sonntag im März (Umstellung auf Sommerzeit um 2:00 Uhr)
// M10.5.0/3 = letzter Sonntag im Oktober (Umstellung auf Winterzeit um 3:00 Uhr)
// Der ESP32 übernimmt DST vollständig – keine eigene Logik nötig.
// ---------------------------------------------------------------------------
#define TIMEZONE   "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER "pool.ntp.org"

// Direkte Momentaufnahme ohne Timeout-Schleife. getLocalTime(..., 0) kann
// bei einem Millisekundenwechsel vor dem ersten Lesen fälschlich scheitern.
static bool _getTime(struct tm &t) {
  time_t now = time(nullptr);
  return localtime_r(&now, &t) != nullptr && t.tm_year > (2016 - 1900);
}

// ---------------------------------------------------------------------------
// Initialisierung – einmalig in setup() aufrufen
// NTP bleibt für spätere Synchronisation aktiv, auch ohne WLAN beim Start.
// Direkte Zeitabfragen; die zusätzliche Wartezeit ist auf timeoutMs begrenzt.
// ---------------------------------------------------------------------------
bool initializeNTP(unsigned long timeoutMs = 10000UL) {
  configTzTime(TIMEZONE, NTP_SERVER);

  Serial.print("[NTP] Warte auf Zeitsynchronisation");
  struct tm timeInfo;
  unsigned long startedAt = millis();
  bool ready = _getTime(timeInfo);
  while (!ready) {
    unsigned long elapsed = millis() - startedAt;
    if (elapsed >= timeoutMs) break;
    Serial.print(".");
    unsigned long remaining = timeoutMs - elapsed;
    delay(remaining < 250UL ? remaining : 250UL);
    ready = _getTime(timeInfo);
  }

  if (ready) {
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeInfo);
    Serial.printf("\n[NTP] Zeit synchronisiert: %s\n", buf);
  } else {
    Serial.println("\n[NTP] Zeit noch nicht verfügbar – Synchronisation läuft im Hintergrund");
  }
  return ready;
}

// ---------------------------------------------------------------------------
// Stunden als nullgepaddter String, z.B. "08"
// ---------------------------------------------------------------------------
String getFormattedHours() {
  struct tm t;
  if (!_getTime(t)) return "--";
  char buf[3];
  sprintf(buf, "%02d", t.tm_hour);
  return String(buf);
}

// ---------------------------------------------------------------------------
// Minuten als nullgepaddter String, z.B. "05"
// ---------------------------------------------------------------------------
String getFormattedMinutes() {
  struct tm t;
  if (!_getTime(t)) return "--";
  char buf[3];
  sprintf(buf, "%02d", t.tm_min);
  return String(buf);
}

// ---------------------------------------------------------------------------
// Aktuelle Minute (0–59) – für Änderungserkennung im Clock-State
// ---------------------------------------------------------------------------
int getCurrentMinute() {
  struct tm t;
  if (!_getTime(t)) return -1;
  return t.tm_min;
}

// ---------------------------------------------------------------------------
// Aktuelle Stunde (0–23) – für Nachtmodus-Logik
// ---------------------------------------------------------------------------
int getCurrentHour() {
  struct tm t;
  if (!_getTime(t)) return -1;
  return t.tm_hour;
}

// ---------------------------------------------------------------------------
// Aktuelles Datum – für Mondphasenberechnung
// ---------------------------------------------------------------------------
void getCurrentDate(int &day, int &month, int &year) {
  struct tm t;
  if (!_getTime(t)) {
    day = 1; month = 1; year = 2024;
    return;
  }
  day   = t.tm_mday;
  month = t.tm_mon + 1;
  year  = t.tm_year + 1900;
}

#endif
