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

// ---------------------------------------------------------------------------
// Initialisierung – einmalig in setup() aufrufen
// Blockiert bis die Zeit synchronisiert ist (max. ~10 Sekunden)
// ---------------------------------------------------------------------------
void initializeNTP() {
  configTzTime(TIMEZONE, NTP_SERVER);

  Serial.print("[NTP] Warte auf Zeitsynchronisation");
  struct tm timeInfo;
  int retries = 0;
  while (!getLocalTime(&timeInfo) && retries < 20) {
    Serial.print(".");
    delay(500);
    retries++;
  }

  if (retries < 20) {
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeInfo);
    Serial.printf("\n[NTP] Zeit synchronisiert: %s\n", buf);
  } else {
    Serial.println("\n[NTP] Zeitlimit erreicht – Zeit noch nicht verfügbar");
  }
}

// ---------------------------------------------------------------------------
// Interne Hilfsfunktion: befüllte tm-Struktur holen
// ---------------------------------------------------------------------------
static bool _getTime(struct tm &t) {
  return getLocalTime(&t);
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
