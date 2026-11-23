#ifndef DISPLAY_TIME_H
#define DISPLAY_TIME_H

#include <TFT_eSPI.h>
#include "Baloo2_Clock40pt7b.h"
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
bool displayTime(const String& hours, const String& minutes, bool enteringView = false) {
  // 1 Bit pro Pixel: nur 19.200 Byte statt 307.200 Byte für einen RGB565-Puffer.
  // Ein fertiges Bild ersetzt das vorige direkt, ohne vorheriges TFT-Löschen.
  static TFT_eSprite frame(&tft);
  static bool frameValid = false;
  static String lastTime;
  static String lastDate;

  String timeStr = hours + ":" + minutes;
  String dateStr;
  struct tm t;
  if (_getTime(t)) {
    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d. %s %04d",
      _weekday[t.tm_wday], t.tm_mday, _month[t.tm_mon + 1], t.tm_year + 1900);
    dateStr = String(dateBuf);
  }

  bool timeChanged = enteringView || !frameValid || timeStr != lastTime;
  bool dateChanged = enteringView || !frameValid || dateStr != lastDate;
  if (!timeChanged && !dateChanged) return true;

  frame.setColorDepth(1);
  if (!frame.createSprite(tft.width(), tft.height())) {
    // Das vorhandene TFT-Bild bleibt auch bei Speichermangel stehen.
    return false;
  }
  frame.fillSprite(TFT_BLACK);

  // --- Uhrzeit ---
  frame.setFreeFont(&Baloo2_Clock40pt7b);
  frame.setTextSize(2);
  frame.setTextColor(TFT_WHITE, TFT_BLACK);

  int16_t timeW  = frame.textWidth(timeStr);
  int16_t timeX  = (tft.width() - timeW) / 2;
  int16_t timeY  = (tft.height() / 2) + 20;

  frame.setCursor(timeX, timeY);
  frame.print(timeStr);

  // --- Datum (Wochentag, TT. Monat JJJJ) ---
  if (dateStr.length()) {
    frame.setFreeFont(&FreeSans18pt7b);
    frame.setTextSize(1);
    int16_t dateW   = frame.textWidth(dateStr);
    int16_t dateX   = (tft.width() - dateW) / 2;
    int16_t dateY   = timeY + frame.fontHeight() + 12;

    frame.setCursor(dateX, dateY);
    frame.print(dateStr);
  }

  // Gleiche Positionen/Farben; das unveränderte Datum nicht minütlich übertragen.
  const int16_t splitY = timeY + 20;
  if (timeChanged) {
    frame.setBitmapColor(TFT_RED, TFT_BLACK);
    frame.pushSprite(0, 0, 0, 0, tft.width(), splitY);
  }
  if (dateChanged) {
    frame.setBitmapColor(TFT_DARKGREY, TFT_BLACK);
    frame.pushSprite(0, splitY, 0, splitY, tft.width(), tft.height() - splitY);
  }
  lastTime = timeStr;
  lastDate = dateStr;
  frameValid = true;
  return true;
}

#endif
