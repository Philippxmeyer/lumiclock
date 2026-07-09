#ifndef WEATHER_H
#define WEATHER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <string.h>
#include "Baloo2_Bold40pt7b.h"
#include "Baloo2_Bold24pt8b.h"

extern TFT_eSPI tft;
extern bool drawPNG(const char* filename, int16_t x, int16_t y);

// ---------------------------------------------------------------------------
// Standort – hier anpassen
// ---------------------------------------------------------------------------
#define WEATHER_LAT "50.735"
#define WEATHER_LON  "7.967"

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
static bool  _isDay               = true;
static bool  _weatherValid        = false;

// ---------------------------------------------------------------------------
// WMO-Code → AccuWeather Icon-Nummer (Tag/Nacht)
// ---------------------------------------------------------------------------
static int _wmoToIcon(int code, bool day) {
  if (code == 0)                          return day ? 1  : 33;
  if (code == 1)                          return day ? 2  : 34;
  if (code == 2)                          return day ? 3  : 35;
  if (code == 3)                          return 7;
  if (code == 45 || code == 48)           return 11;
  if (code == 51 || code == 53 || code == 55) return day ? 14 : 39;
  if (code == 56 || code == 57)           return 26;
  if (code == 61 || code == 63 || code == 65) return 18;
  if (code == 66 || code == 67)           return 26;
  if (code == 71 || code == 73 || code == 75) return 22;
  if (code == 77)                         return 24;
  if (code == 80 || code == 81)           return day ? 14 : 39;
  if (code == 82)                         return day ? 12 : 40;
  if (code == 85 || code == 86)           return day ? 23 : 44;
  if (code >= 95 && code <= 99)           return 15;
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

// Tages-Prognose - gecachte Daten (3 Tage)
// ---------------------------------------------------------------------------
struct ForecastDay {
  int   weatherCode;
  float tempMax;
  float tempMin;
  int   precipProb;
};

static const int FORECAST_DAYS = 3;
static ForecastDay _forecast[FORECAST_DAYS];
static bool        _forecastValid = false;

#define FORECAST_URL \
  "https://api.open-meteo.com/v1/forecast" \
  "?latitude=" WEATHER_LAT \
  "&longitude=" WEATHER_LON \
  "&hourly=weather_code,precipitation_probability,precipitation" \
  "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" \
  "&forecast_days=3" \
  "&timezone=Europe%2FBerlin"

static bool _isThunderCode(int code) {
  return code >= 95 && code <= 99;
}

static bool _isRainCode(int code) {
  return (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
}

static bool _isSnowCode(int code) {
  return (code >= 71 && code <= 77) || code == 85 || code == 86;
}

static bool _isFogCode(int code) {
  return code == 45 || code == 48;
}

static int _rainSeverity(int code) {
  if (code == 82 || code == 65 || code == 67) return 4;
  if (code == 81 || code == 63 || code == 57 || code == 55) return 3;
  if (code == 80 || code == 61 || code == 56 || code == 53) return 2;
  if (code == 51) return 1;
  return 0;
}

static int _pickDayWeatherCode(JsonArray times, JsonArray codes, JsonArray probs,
                               JsonArray precip, const char* dayDate,
                               int fallbackCode) {
  int clearHours = 0;
  int mostlyClearHours = 0;
  int partlyCloudyHours = 0;
  int overcastHours = 0;
  int fogHours = 0;
  int rainHours = 0;
  int snowHours = 0;
  int thunderCode = 0;
  int bestRainCode = 0;
  int bestRainSeverity = 0;
  int bestSnowCode = 0;
  int hoursSeen = 0;
  int maxProb = 0;
  float precipTotal = 0.0f;

  for (size_t i = 0; i < times.size(); i++) {
    const char* ts = times[i];
    if (!ts || strncmp(ts, dayDate, 10) != 0) continue;

    int hour = ((ts[11] - '0') * 10) + (ts[12] - '0');
    if (hour < 10 || hour > 18) continue;

    int code = codes[i] | fallbackCode;
    int prob = probs[i] | 0;
    float amount = precip[i] | 0.0f;

    hoursSeen++;
    if (prob > maxProb) maxProb = prob;
    precipTotal += amount;

    if (_isThunderCode(code)) {
      thunderCode = code;
    } else if (_isRainCode(code)) {
      rainHours++;
      int severity = _rainSeverity(code);
      if (severity > bestRainSeverity) {
        bestRainSeverity = severity;
        bestRainCode = code;
      }
    } else if (_isSnowCode(code)) {
      snowHours++;
      bestSnowCode = code;
    } else if (_isFogCode(code)) {
      fogHours++;
    } else if (code == 0) {
      clearHours++;
    } else if (code == 1) {
      mostlyClearHours++;
    } else if (code == 2) {
      partlyCloudyHours++;
    } else if (code == 3) {
      overcastHours++;
    }
  }

  if (hoursSeen == 0) return fallbackCode;

  if (thunderCode != 0) return thunderCode;
  if (precipTotal >= 0.7f || maxProb >= 60 || rainHours >= 2) {
    if (bestRainCode != 0) return bestRainCode;
    return (precipTotal >= 3.0f || maxProb >= 80) ? 63 : 61;
  }
  if (snowHours >= 2) return bestSnowCode;
  if (fogHours >= 4) return 45;

  int brightHours = clearHours + mostlyClearHours;
  int cloudHours = partlyCloudyHours + overcastHours;
  if (clearHours >= 5 && overcastHours <= 1) return 0;
  if (brightHours >= 5) return 1;
  if (overcastHours >= 6) return 3;
  if (cloudHours >= 5) return 2;
  if (partlyCloudyHours > 0) return 2;
  return fallbackCode;
}

// ---------------------------------------------------------------------------
// initWeather()
// ---------------------------------------------------------------------------
void initWeather() {
  Serial.println("[Wetter] Open-Meteo HTTPS – kein API-Key erforderlich");
}

// ---------------------------------------------------------------------------
// fetchWeather() – HTTPS mit setInsecure() (kein Root-CA nötig)
// setInsecure() deaktiviert die Zertifikatsprüfung – für ein lokales
// Hobbygerät ohne sensible Daten akzeptabel, spart das CA-Bundle.
// ---------------------------------------------------------------------------
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Wetter] Kein WLAN – überspringe Abruf");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Zertifikat nicht prüfen

  HTTPClient http;
  http.begin(client, WEATHER_URL);
  http.setTimeout(8000);  // 8 Sekunden Timeout
  http.addHeader("Accept-Encoding", "identity");  // kein gzip, JSON-Stream bleibt parsebar
  http.addHeader("User-Agent", "lumiclock/1.0");
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

  // Open-Meteo-Response: typischerweise < 1KB
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[Wetter] JSON-Fehler: %s | Prefix: %.80s\n", err.c_str(), payload.c_str());
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
// renderWeather()
// ---------------------------------------------------------------------------
void renderWeather() {
  tft.fillScreen(TFT_BLACK);

  if (!_weatherValid) {
   tft.setFreeFont(&Baloo2_Bold24pt8b);
   tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 80);
    tft.println("Keine Wetterdaten");
    return;
  }

  int iconNum = _wmoToIcon(_weatherCode, _isDay);
  drawPNG(_iconFilename(iconNum), 20, 50);

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
  int16_t codeWidth = tft.textWidth(_weatherText(_weatherCode));
  tft.setCursor((tft.width() - codeWidth) / 2, 265);
  tft.println(_weatherText(_weatherCode));
}

// fetchForecast() - 3-Tages-Prognose mit Tages- und Stundenwerten
// ---------------------------------------------------------------------------
void fetchForecast() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Forecast] Kein WLAN – überspringe Abruf");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, FORECAST_URL);
  http.setTimeout(8000);
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("User-Agent", "lumiclock/1.0");
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Forecast] HTTP-Fehler: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("[Forecast] Leere Antwort");
    return;
  }

  DynamicJsonDocument doc(24576);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[Forecast] JSON-Fehler: %s\n", err.c_str());
    return;
  }

  JsonArray dayDates      = doc["daily"]["time"];
  JsonArray dailyCodes    = doc["daily"]["weather_code"];
  JsonArray maxT          = doc["daily"]["temperature_2m_max"];
  JsonArray minT          = doc["daily"]["temperature_2m_min"];
  JsonArray dailyProb     = doc["daily"]["precipitation_probability_max"];
  JsonArray hourlyTimes   = doc["hourly"]["time"];
  JsonArray hourlyCodes   = doc["hourly"]["weather_code"];
  JsonArray hourlyProb    = doc["hourly"]["precipitation_probability"];
  JsonArray hourlyPrecip  = doc["hourly"]["precipitation"];

  for (int i = 0; i < FORECAST_DAYS; i++) {
    const char* dayDate = dayDates[i] | "";
    int fallbackCode = dailyCodes[i] | 0;
    _forecast[i].weatherCode = _pickDayWeatherCode(hourlyTimes, hourlyCodes, hourlyProb,
                                                   hourlyPrecip, dayDate, fallbackCode);
    _forecast[i].tempMax     = maxT[i]      | 0.0f;
    _forecast[i].tempMin     = minT[i]      | 0.0f;
    _forecast[i].precipProb  = dailyProb[i] | 0;
  }
  _forecastValid = true;
  Serial.printf("[Forecast] OK - Codes 10-18 Uhr: %d/%d/%d\n",
    _forecast[0].weatherCode, _forecast[1].weatherCode,
    _forecast[2].weatherCode);
}

// ---------------------------------------------------------------------------
// renderForecast() – kompakte 3-Spalten-Tagesübersicht
// ---------------------------------------------------------------------------

static const char* _iconFilenameSmall(int iconNum) {
  static char buf[13];
  sprintf(buf, "/%02d-xs.png", iconNum);
  return buf;
}

// Tomohiko Sakamoto – gibt 0=So, 1=Mo, 2=Di, 3=Mi, 4=Do, 5=Fr, 6=Sa zurück
static int _weekdayFromDate(int d, int m, int y) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static const char* _dayLabel(int dayIndex, int todayWeekday) {
  if (dayIndex == 0) return "heute";
  if (dayIndex == 1) return "morgen";
  static const char* names[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  return names[(todayWeekday + dayIndex) % 7];
}

void renderForecast() {
  tft.fillScreen(TFT_BLACK);

  if (!_forecastValid) {
    tft.setFreeFont(&Baloo2_Bold24pt8b);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 80);
    tft.println("Keine Prognosedaten");
    return;
  }

  int today_d, today_m, today_y;
  getCurrentDate(today_d, today_m, today_y);
  int todayWeekday = _weekdayFromDate(today_d, today_m, today_y);
  int startIndex   = 0;

  const int COL_W = 160;

  // Trennlinien
  tft.drawFastVLine(COL_W + 10,     0, 320, TFT_DARKGREY);
  tft.drawFastVLine(2 * COL_W + 10, 0, 320, TFT_DARKGREY);

  for (int col = 0; col < FORECAST_DAYS; col++) {
    int dayIdx = startIndex + col;
    const ForecastDay& d = _forecast[dayIdx];
    int cx = col * COL_W + COL_W / 2;  // Spaltenmitte

    // --- Tagesname ---
    tft.setFreeFont(&Baloo2_Bold24pt8b);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    const char* label = _dayLabel(dayIdx, todayWeekday);
    int lw = tft.textWidth(label);
    tft.setCursor(cx - lw / 2, 35);
    tft.print(label);

    // --- Wetter-Icon (klein) ---
    int iconNum = _wmoToIcon(d.weatherCode, true);
    drawPNG(_iconFilenameSmall(iconNum), cx - 40, 45);

    // --- Höchsttemperatur (rot) ---
    tft.setFreeFont(&Baloo2_Bold24pt8b);
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED);
    String maxStr = String((int)round(d.tempMax)) + "\xB0";
    int mxw = tft.textWidth(maxStr);
    tft.setCursor(cx - mxw / 2, 180);
    tft.print(maxStr);

    // --- Tiefsttemperatur (blau) ---
    tft.setTextColor(TFT_BLUE);
    String minStr = String((int)round(d.tempMin)) + "\xB0";
    int mnw = tft.textWidth(minStr);
    tft.setCursor(cx - mnw / 2, 235);
    tft.print(minStr);

    // --- Regenwahrscheinlichkeit (blau) ---
    tft.setTextColor(TFT_CYAN);
    String rainStr = String(d.precipProb) + "%";
    int rw = tft.textWidth(rainStr);
    tft.setCursor(cx - rw / 2, 290);
    tft.print(rainStr);
  }
}

#endif
