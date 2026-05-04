#ifndef DISPLAY_TIME_H
#define DISPLAY_TIME_H

#include <TFT_eSPI.h>
#include "Baloo2_Bold40pt7b.h"
#include "Baloo2_Bold24pt8b.h"

extern TFT_eSPI tft;

// ---------------------------------------------------------------------------
// Wochentage und Monate auf Deutsch
// ---------------------------------------------------------------------------
static const char* _weekday[] = {
  "Sonntag", "Montag", "Dienstag", "Mittwoch",
  "Donnerstag", "Freitag", "Samstag"
};

static const char* _month[] = {
  "", "Jan", "Feb", "Mär", "Apr", "Mai", "Jun",
  "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"
};

// ---------------------------------------------------------------------------
// displayTime() – zeichnet Uhrzeit und Datum auf das TFT
//
// Layout (480×320):
//   - Uhrzeit  vertikal zentriert, leicht nach oben verschoben
//   - Datum    darunter, kleiner, zentriert
// ---------------------------------------------------------------------------
void displayTime(const String& hours, const String& minutes) {
  tft.fillScreen(TFT_BLACK);

  // --- Uhrzeit ---
  tft.setFreeFont(&Baloo2_Bold40pt7b);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED, TFT_BLACK);

  String timeStr = hours + ":" + minutes;
  int16_t timeW  = tft.textWidth(timeStr);
  int16_t timeX  = (tft.width() - timeW) / 2;
  int16_t timeY  = (tft.height() / 2) + 20;

  tft.setCursor(timeX, timeY);
  tft.println(timeStr);

  // --- Datum (Wochentag, TT. Monat JJJJ) ---
  struct tm t;
  if (getLocalTime(&t)) {
tft.setFreeFont(&FreeSans18pt7b);
tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d. %s %04d",
      _weekday[t.tm_wday],
      t.tm_mday,
      _month[t.tm_mon + 1],
      t.tm_year + 1900
    );

    String dateStr  = String(dateBuf);
    int16_t dateW   = tft.textWidth(dateStr);
    int16_t dateX   = (tft.width() - dateW) / 2;
    int16_t dateY   = timeY + tft.fontHeight() + 12;

    tft.setCursor(dateX, dateY);
    tft.println(dateStr);
  }
}

#endif
