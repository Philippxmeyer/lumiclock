#ifndef WEATHER_H
#define WEATHER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <math.h>

#include "Baloo2_Bold40pt7b.h"
#include "Baloo2_Bold24pt8b.h"

extern TFT_eSPI tft;
extern bool drawPNG(const char* filename, int16_t x, int16_t y);

// ---------------------------------------------------------------------------
// Standort
// ---------------------------------------------------------------------------
#define WEATHER_LAT "50.73542714"
#define WEATHER_LON "7.96705156"

#define WEATHER_URL \
  "https://api.open-meteo.com/v1/forecast" \
  "?latitude=" WEATHER_LAT \
  "&longitude=" WEATHER_LON \
  "&current=temperature_2m,apparent_temperature,weather_code,is_day" \
  "&hourly=weather_code,precipitation_probability,precipitation" \
  "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max" \
  "&forecast_days=3" \
  "&timezone=Europe%2FBerlin"

// ---------------------------------------------------------------------------
// Gecachte Wetterdaten
// ---------------------------------------------------------------------------
static float _temperature         = 0.0f;
static float _apparentTemperature = 0.0f;
static int   _weatherCode         = 0;
static int   _displayWeatherCode  = 0;
static bool  _isDay               = true;
static bool  _weatherValid        = false;
static bool  _morningFog          = false;

// ---------------------------------------------------------------------------
// WMO-Code → AccuWeather Icon-Nummer
// ---------------------------------------------------------------------------
static int _wmoToIcon(int code, bool day) {
  if (code == 0)                              return day ? 1  : 33;
  if (code == 1)                              return day ? 2  : 34;
  if (code == 2)                              return day ? 3  : 35;
  if (code == 3)                              return 7;
  if (code == 45 || code == 48)               return 11;
  if (code == 51 || code == 53 || code == 55) return day ? 14 : 39;
  if (code == 56 || code == 57)               return 26;
  if (code == 61 || code == 63 || code == 65) return 18;
  if (code == 66 || code == 67)               return 26;
  if (code == 71 || code == 73 || code == 75) return 22;
  if (code == 77)                             return 24;
  if (code == 80 || code == 81)               return day ? 14 : 39;
  if (code == 82)                             return day ? 12 : 40;
  if (code == 85 || code == 86)               return day ? 23 : 44;
  if (code >= 95 && code <= 99)               return 15;
  return day ? 1 : 33;
}

static const char* _iconFilename(int iconNum) {
  static char buf[12];
  sprintf(buf, "/%02d-s.png", iconNum);
  return buf;
}

static const char* _weatherText(int code) {
  if (code == 0)                    return "Klar";
  if (code == 1)                    return "Meist klar";
  if (code == 2)                    return "Teils bewölkt";
  if (code == 3)                    return "Bedeckt";
  if (code == 45 || code == 48)     return "Nebelig";
  if (code == 51 || code == 53)     return "Leichter Nieselregen";
  if (code == 55)                   return "Nieselregen";
  if (code == 56 || code == 57)     return "Gefrierender Regen";
  if (code == 61 || code == 63)     return "Regen";
  if (code == 65)                   return "Starker Regen";
  if (code == 66 || code == 67)     return "Gefrierender Regen";
  if (code == 71 || code == 73)     return "Schneefall";
  if (code == 75)                   return "Starker Schneefall";
  if (code == 77)                   return "Schneekörner";
  if (code == 80 || code == 81)     return "Regenschauer";
  if (code == 82)                   return "Starke Schauer";
  if (code == 85 || code == 86)     return "Schneeschauer";
  if (code == 95)                   return "Gewitter";
  if (code == 96 || code == 99)     return "Gewitter mit Hagel";
  return "Unbekannt";
}

// ---------------------------------------------------------------------------
// Wettercode für Tagesanzeige optimieren
// Bewertet 10–18 Uhr statt stumpf daily.weather_code.
// Dadurch wird z.B. Morgennebel nicht als ganzer Nebeltag angezeigt.
// ---------------------------------------------------------------------------
static int _pickDisplayWeatherCode(JsonDocument& doc) {
  JsonArray times = doc["hourly"]["time"];
  JsonArray codes = doc["hourly"]["weather_code"];
  JsonArray pops  = doc["hourly"]["precipitation_probability"];
  JsonArray rain  = doc["hourly"]["precipitation"];

  if (times.isNull() || codes.isNull()) {
    return doc["current"]["weather_code"] | 0;
  }

  int count[100] = {0};
  int bestCode = doc["current"]["weather_code"] | 0;

  int maxPop = 0;
  float rainSum = 0.0f;

  _morningFog = false;

  for (size_t i = 0; i < times.size() && i < codes.size(); i++) {
    const char* ts = times[i];
    if (!ts || strlen(ts) < 13) continue;

    int hour = atoi(ts + 11);
    int code = codes[i] | 0;

    // Morgennebel nur als Zusatz merken
    if (hour >= 5 && hour <= 9 && (code == 45 || code == 48)) {
      _morningFog = true;
    }

    // Hauptbewertung: Tageslicht-/Erlebniszeit
    if (hour >= 10 && hour <= 18) {
      if (code >= 0 && code < 100) count[code]++;

      if (!pops.isNull() && i < pops.size()) {
        int p = pops[i] | 0;
        if (p > maxPop) maxPop = p;
      }

      if (!rain.isNull() && i < rain.size()) {
        rainSum += rain[i] | 0.0f;
      }
    }
  }

  // Niederschlag schlägt Wolkenlogik
  if (rainSum > 1.0f || maxPop >= 60) {
    if (count[95] || count[96] || count[99]) return 95;
    if (count[82]) return 82;
    if (count[80] || count[81]) return 80;
    if (count[65]) return 65;
    if (count[61] || count[63]) return 61;
    if (count[51] || count[53] || count[55]) return 51;
  }

  // Häufigster Code zwischen 10 und 18 Uhr
  int bestCount = -1;
  for (int c = 0; c < 100; c++) {
    if (count[c] > bestCount) {
      bestCount = count[c];
      bestCode = c;
    }
  }

  // Kleine Glättung: ein paar Wolken ruinieren keinen sonnigen Tag
  int clearish = count[0] + count[1];
  int partly   = count[2];
  int cloudy   = count[3];

  if (clearish >= 5) return 1;
  if (clearish + partly >= 5) return 2;
  if (cloudy >= 5) return 3;

  return bestCode;
}

// ---------------------------------------------------------------------------
// initWeather()
// ---------------------------------------------------------------------------
void initWeather() {
  Serial.println("[Wetter] Open-Meteo HTTPS – optimierte Tagesbewertung aktiv");
}

// ---------------------------------------------------------------------------
// fetchWeather()
// ---------------------------------------------------------------------------
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Wetter] Kein WLAN – überspringe Abruf");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, WEATHER_URL);
  http.setTimeout(8000);
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("User-Agent", "lumiclock/1.1");

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Wetter] HTTP-Fehler: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("[Wetter] Leere Antwort");
    return;
  }

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[Wetter] JSON-Fehler: %s | Prefix: %.80s\n", err.c_str(), payload.c_str());
    return;
  }

  _temperature         = doc["current"]["temperature_2m"]      | 0.0f;
  _apparentTemperature = doc["current"]["apparent_temperature"] | 0.0f;
  _weatherCode         = doc["current"]["weather_code"]         | 0;
  _isDay               = (doc["current"]["is_day"]              | 1) == 1;

  _displayWeatherCode  = _pickDisplayWeatherCode(doc);
  _weatherValid        = true;

  int iconNum = _wmoToIcon(_displayWeatherCode, _isDay);

  Serial.printf("[Wetter] %.1f°C (gefühlt %.1f°C) | Jetzt Code %d | Anzeige Code %d | Icon %02d | %s%s\n",
    _temperature,
    _apparentTemperature,
    _weatherCode,
    _displayWeatherCode,
    iconNum,
    _weatherText(_displayWeatherCode),
    _morningFog ? " | Morgennebel erkannt" : "");
}

// ---------------------------------------------------------------------------
// renderWeather()
// ---------------------------------------------------------------------------
void renderWeather() {
  tft.fillScreen(TFT_BLACK);

  if (!_weatherValid) {
    tft.setFreeFont(&Baloo2_Bold24pt8b);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 80);
    tft.println("Keine Wetterdaten");
    return;
  }

  int iconNum = _wmoToIcon(_displayWeatherCode, _isDay);
  drawPNG(_iconFilename(iconNum), 10, 90);

  tft.setFreeFont(&Baloo2_Bold24pt8b);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED);

  String tempStr = String((int)round(_temperature)) + "°C";
  tft.setCursor(265, 130);
  tft.println(tempStr);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);

  String feelStr = "(" + String((int)round(_apparentTemperature)) + "°C)";
  tft.setCursor(265, 185);
  tft.println(feelStr);

  tft.setFreeFont(&FreeSans18pt7b);
  tft.setTextColor(TFT_LIGHTGREY);

  String text = String(_weatherText(_displayWeatherCode));
  if (_morningFog && _displayWeatherCode != 45 && _displayWeatherCode != 48) {
    text = "Morgens Nebel";
  }

  int16_t codeWidth = tft.textWidth(text);
  tft.setCursor((tft.width() - codeWidth) / 2, 265);
  tft.println(text);
}

#endif
