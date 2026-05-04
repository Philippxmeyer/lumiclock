#ifndef MOON_PHASE_H
#define MOON_PHASE_H

#include <math.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern bool drawPNG(const char* filename, int16_t x, int16_t y);

// ---------------------------------------------------------------------------
// Mondphasenberechnung
//
// Algorithmus nach Conway / Meeus "Astronomical Algorithms":
// Berechnet den Anteil des beleuchteten Mondes (0.0 – 1.0) sowie
// ob der Mond zu- oder abnimmt.
//
// Rückgabe: Alter des Mondes in Tagen seit letztem Neumond (0.0 – 29.53)
// ---------------------------------------------------------------------------
static double _moonAge(int day, int month, int year) {
  // Julianisches Datum berechnen
  if (month <= 2) {
    month += 12;
    year  -= 1;
  }
  long A = year / 100;
  long B = 2 - A + (A / 4);
  double jd = (long)(365.25 * (year + 4716))
            + (long)(30.6001 * (month + 1))
            + day + B - 1524.5;

  // Tage seit bekanntem Neumond (2000-01-06 18:14 UTC = JD 2451550.1)
  double daysSinceNew = jd - 2451550.1;

  // Modulo 29.53 (Synodische Periode)
  double age = fmod(daysSinceNew, 29.53059);
  if (age < 0) age += 29.53059;
  return age;
}

// ---------------------------------------------------------------------------
// Mondphase (0–7) aus Mondalter ableiten
//
// 0 = Neumond          (0 – 1.85 Tage)
// 1 = Zunehmende Sichel
// 2 = Erstes Viertel   (~7.38 Tage)
// 3 = Zunehmender Gibbous
// 4 = Vollmond         (~14.76 Tage)
// 5 = Abnehmender Gibbous
// 6 = Letztes Viertel  (~22.15 Tage)
// 7 = Abnehmende Sichel
// ---------------------------------------------------------------------------
static int _moonPhaseIndex(double age) {
  if (age < 1.85)  return 0;
  if (age < 7.38)  return 1;
  if (age < 9.22)  return 2;
  if (age < 14.76) return 3;
  if (age < 16.61) return 4;
  if (age < 22.15) return 5;
  if (age < 24.0)  return 6;
  if (age < 29.53) return 7;
  return 0;  // Neumond (Übergang)
}

// ---------------------------------------------------------------------------
// PNG-Dateiname für jede Phase
// Dateien müssen als /moon0.png – /moon7.png auf LittleFS liegen
// ---------------------------------------------------------------------------
static const char* _moonIcon(int phaseIndex) {
  switch (phaseIndex) {
    case 0: return "/moon0.png";  // Neumond
    case 1: return "/moon1.png";  // Zunehmende Sichel
    case 2: return "/moon2.png";  // Erstes Viertel
    case 3: return "/moon3.png";  // Zunehmender Gibbous
    case 4: return "/moon4.png";  // Vollmond
    case 5: return "/moon5.png";  // Abnehmender Gibbous
    case 6: return "/moon6.png";  // Letztes Viertel
    case 7: return "/moon7.png";  // Abnehmende Sichel
    default: return "/moon0.png";
  }
}

// ---------------------------------------------------------------------------
// Deutscher Name der Mondphase
// ---------------------------------------------------------------------------
static const char* _moonPhaseName(int phaseIndex) {
  switch (phaseIndex) {
    case 0: return "Neumond";
    case 1: return "Zunehmende Sichel";
    case 2: return "Erstes Viertel";
    case 3: return "Zunehmender Mond";
    case 4: return "Vollmond";
    case 5: return "Abnehmender Mond";
    case 6: return "Letztes Viertel";
    case 7: return "Abnehmende Sichel";
    default: return "Unbekannt";
  }
}

// ---------------------------------------------------------------------------
// renderMoonPhase() – zeichnet Mondphase auf das TFT
// Layout (3.5" Display, 480×320):
//   - Icon zentriert oben  (160×160 px) bei (160, 20)
//   - Phasename unten zentriert         bei y=260
//   - Mondalter in Tagen                bei y=295
// ---------------------------------------------------------------------------
void renderMoonPhase(int day, int month, int year) {
  double age        = _moonAge(day, month, year);
  int    phaseIndex = _moonPhaseIndex(age);

  Serial.printf("[Mond] Alter: %.1f Tage, Phase: %d (%s)\n",
    age, phaseIndex, _moonPhaseName(phaseIndex));

  tft.fillScreen(TFT_BLACK);

  // Icon zentriert
  drawPNG(_moonIcon(phaseIndex), 10, 5);

// Phasename
tft.setFreeFont(nullptr);        // FreeFont deaktivieren
tft.setTextFont(4);              // Standardfont, deutlich kleiner
tft.setTextSize(1);
tft.setTextColor(TFT_WHITE, TFT_BLACK);

String phaseName = String(_moonPhaseName(phaseIndex));
int16_t nameWidth = tft.textWidth(phaseName);
tft.setCursor((tft.width() - nameWidth) / 2, 250);
tft.println(phaseName);

// Mondalter in Tagen
tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

String ageStr = "Tag " + String((int)age + 1) + " von 30";
int16_t ageWidth = tft.textWidth(ageStr);
tft.setCursor((tft.width() - ageWidth) / 2, 275);
tft.println(ageStr);
}

#endif
