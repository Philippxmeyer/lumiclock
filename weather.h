#ifndef WEATHER_H
#define WEATHER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "Baloo2_Bold40pt7b.h"
#include "Baloo2_Bold24pt8b.h"

extern TFT_eSPI tft;
extern bool drawPNG(const char* filename, int16_t x, int16_t y);

// ---------------------------------------------------------------------------
// Standort – hier anpassen
// ---------------------------------------------------------------------------
#define WEATHER_LAT "50.66"   // Daaden
#define WEATHER_LON  "7.98"

#define WEATHER_URL \
  "https://api.open-meteo.com/v1/forecast" \
  "?latitude=" WEATHER_LAT \
  "&longitude=" WEATHER_LON \
  "&current=temperature_2m,apparent_temperature,weather_code,is_day" \
  "&timezone=Europe%2FBerlin"

// ---------------------------------------------------------------------------
// Gecachte Wetterdaten
// ---------------------------------------------------------------------------
static float _temperature         = 0.0f;
static float _apparentTemperature = 0.0f;
static int   _weatherCode         = 0;
static bool  _isDay               = true;   // Open-Meteo: is_day (1=Tag, 0=Nacht)
static bool  _weatherValid        = false;

// ---------------------------------------------------------------------------
// Tag/Nacht bestimmen
// Dämmerung (6–7h morgens, 18–19h abends) → Tag-Icon
// ---------------------------------------------------------------------------
static bool _dayMode() {
  return _isDay;  // direkt von Open-Meteo, inkl. Dämmerungslogik
}

// ---------------------------------------------------------------------------
// WMO-Code → AccuWeather Icon-Nummer
// Tag- und Nacht-Variante getrennt
// Codes ohne Nacht-Pendant → Tag-Icon als Fallback (7, 11, 15, 18, 22, 24, 26, 29)
// ---------------------------------------------------------------------------
static int _wmoToIcon(int code, bool day) {
  // Klar / Bewölkt
  if (code == 0)  return day ? 1  : 33;   // Sunny / Clear
  if (code == 1)  return day ? 2  : 34;   // Mostly sunny / Mostly clear
  if (code == 2)  return day ? 3  : 35;   // Partly sunny / Partly cloudy
  if (code == 3)  return 7;               // Cloudy (Tag+Nacht gleich)

  // Nebel
  if (code == 45 || code == 48) return 11;  // Fog (Tag+Nacht)

  // Nieselregen
  if (code == 51 || code == 53 || code == 55)
                  return day ? 14 : 39;   // P.sunny w/ showers / P.cloudy w/ showers
  if (code == 56 || code == 57) return 26; // Freezing rain (Tag+Nacht)

  // Regen
  if (code == 61 || code == 63 || code == 65) return 18; // Rain (Tag+Nacht)
  if (code == 66 || code == 67) return 26; // Freezing rain (Tag+Nacht)

  // Schnee
  if (code == 71 || code == 73 || code == 75) return 22; // Snow (Tag+Nacht)
  if (code == 77) return 24;              // Ice (Tag+Nacht)

  // Schauer
  if (code == 80 || code == 81)
                  return day ? 14 : 39;   // P.sunny w/ showers / P.cloudy w/ showers
  if (code == 82) return day ? 12 : 40;   // Showers / M.cloudy w/ showers
  if (code == 85 || code == 86)
                  return day ? 23 : 44;   // M.cloudy w/ snow (Tag+Nacht)

  // Gewitter (inkl. Hagel – kein eigenes Icon vorhanden)
  if (code >= 95 && code <= 99) return 15; // T-storms (Tag+Nacht)

  return day ? 1 : 33;  // Fallback: klar
}

// ---------------------------------------------------------------------------
// Icon-Nummer → Dateiname auf LittleFS
// Format: /XX-s.png (einstellige Zahlen mit führender Null)
// ---------------------------------------------------------------------------
static const char* _iconFilename(int iconNum) {
  static char buf[12];
  sprintf(buf, "/%02d-s.png", iconNum);
  return buf;
}

// ---------------------------------------------------------------------------
// Kurze deutsche Beschreibung zum WMO-Code
// ---------------------------------------------------------------------------
static const char* _weatherText(int code) {
  if (code == 0)                        return "Klar";
  if (code == 1)                        return "Meist klar";
  if (code == 2)                        return "Teils bewölkt";
  if (code == 3)                        return "Bedeckt";
  if (code == 45 || code == 48)         return "Nebelig";
  if (code == 51 || code == 53)         return "Leichter Nieselregen";
  if (code == 55)                       return "Nieselregen";
  if (code == 56 || code == 57)         return "Gefrierender Regen";
  if (code == 61 || code == 63)         return "Regen";
  if (code == 65)                       return "Starker Regen";
  if (code == 66 || code == 67)         return "Gefrierender Regen";
  if (code == 71 || code == 73)         return "Schneefall";
  if (code == 75)                       return "Starker Schneefall";
  if (code == 77)                       return "Schneekörner";
  if (code == 80 || code == 81)         return "Regenschauer";
  if (code == 82)                       return "Starke Schauer";
  if (code == 85 || code == 86)         return "Schneeschauer";
  if (code == 95)                       return "Gewitter";
  if (code == 96 || code == 99)         return "Gewitter mit Hagel";
  return "Unbekannt";
}

// ---------------------------------------------------------------------------
// initWeather() – einmalig in setup()
// ---------------------------------------------------------------------------
void initWeather() {
  Serial.println("[Wetter] Open-Meteo – kein API-Key erforderlich");
}

// ---------------------------------------------------------------------------
// fetchWeather() – HTTP-Request, gecachte Daten befüllen
// ---------------------------------------------------------------------------
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Wetter] Kein WLAN – überspringe Abruf");
    return;
  }

  HTTPClient http;
  http.begin(WEATHER_URL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Wetter] HTTP-Fehler: %d\n", httpCode);
    http.end();
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    Serial.printf("[Wetter] JSON-Fehler: %s\n", err.c_str());
    return;
  }

  _temperature         = doc["current"]["temperature_2m"]      | 0.0f;
  _apparentTemperature = doc["current"]["apparent_temperature"] | 0.0f;
  _weatherCode         = doc["current"]["weather_code"]         | 0;
  _isDay               = (doc["current"]["is_day"]              | 1) == 1;
  _weatherValid        = true;

  int iconNum = _wmoToIcon(_weatherCode, _isDay);
  Serial.printf("[Wetter] %.1f°C (gefühlt %.1f°C) | Code %d | %s | Icon %02d | %s\n",
    _temperature, _apparentTemperature,
    _weatherCode, _isDay ? "Tag" : "Nacht",
    iconNum, _weatherText(_weatherCode));
}

// ---------------------------------------------------------------------------
// renderWeather() – zeichnet gecachte Daten auf das TFT
//
// Layout (480×320, Landscape):
//   Icon  links        225×135px  bei (10, 90)
//   Temp  rechts oben             bei (265, 130)
//   Fühlt rechts mitte            bei (265, 185)
//   Text  unten links             bei (10,  272)
// ---------------------------------------------------------------------------
void renderWeather() {
  tft.fillScreen(TFT_BLACK);

  if (!_weatherValid) {
    tft.setFreeFont(&Baloo2_Bold24pt8b);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, 160);
    tft.println("Keine Wetterdaten");
    return;
  }

  int iconNum = _wmoToIcon(_weatherCode, _isDay);
  drawPNG(_iconFilename(iconNum), 10, 90);

  // Temperatur groß
  tft.setFreeFont(&Baloo2_Bold40pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String tempStr = String((int)round(_temperature)) + "\xB0""C";
  tft.setCursor(265, 130);
  tft.println(tempStr);

  // Gefühlte Temperatur kleiner
  tft.setFreeFont(&Baloo2_Bold24pt8b);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  String feelStr = "fühlt " + String((int)round(_apparentTemperature)) + "\xB0""C";
  tft.setCursor(265, 185);
  tft.println(feelStr);

  // Wettertext unten
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 272);
  tft.println(_weatherText(_weatherCode));
}

#endif
