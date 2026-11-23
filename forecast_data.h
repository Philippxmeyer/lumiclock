#ifndef FORECAST_DATA_H
#define FORECAST_DATA_H

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Vier Tage vorhalten, weiterhin genau drei Spalten anzeigen.
static const int FORECAST_DAYS = 4;
static const int FORECAST_COLUMNS = 3;
static const int DAY_QUARTERS = 4;

struct ForecastDay {
  int weatherCode;
  float tempMax;
  float tempMin;
  int precipProb;
  char date[11];  // Lokales API-Datum; Cache bleibt über Mitternacht zuordenbar.
};

struct ForecastQuarter {
  ForecastDay weather;
  bool valid;
};

static ForecastDay _forecast[FORECAST_DAYS];
static ForecastQuarter _quarters[FORECAST_DAYS * DAY_QUARTERS];
static bool _forecastValid = false;

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

static int _pickWeatherCode(JsonArray times, JsonArray codes, JsonArray probs,
                            JsonArray precip, const char* dayDate,
                            int fallbackCode, int firstHour = 10, int endHour = 19) {
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
    if (!ts || strlen(ts) < 13 || strncmp(ts, dayDate, 10) != 0) continue;

    int hour = ((ts[11] - '0') * 10) + (ts[12] - '0');
    if (hour < firstHour || hour >= endHour) continue;

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

// Erst vollständig geprüfte Tagesdaten ersetzen den bisherigen Cache.
// Fehlende Stundenwerte sperren nur das betroffene Tagesviertel.
static bool _loadForecastData(JsonDocument& doc) {
  JsonArray dates = doc["daily"]["time"];
  JsonArray codes = doc["daily"]["weather_code"];
  JsonArray maxT = doc["daily"]["temperature_2m_max"];
  JsonArray minT = doc["daily"]["temperature_2m_min"];
  JsonArray probs = doc["daily"]["precipitation_probability_max"];
  JsonArray times = doc["hourly"]["time"];
  JsonArray hourlyCodes = doc["hourly"]["weather_code"];
  JsonArray hourlyProbs = doc["hourly"]["precipitation_probability"];
  JsonArray precip = doc["hourly"]["precipitation"];
  JsonArray temps = doc["hourly"]["temperature_2m"];

  ForecastDay days[FORECAST_DAYS] = {};
  ForecastQuarter quarters[FORECAST_DAYS * DAY_QUARTERS] = {};
  for (int day = 0; day < FORECAST_DAYS; ++day) {
    const char* date = dates[day] | "";
    if (strlen(date) != 10 || codes[day].isNull() || maxT[day].isNull() ||
        minT[day].isNull() || probs[day].isNull()) return false;
    ForecastDay& d = days[day];
    memcpy(d.date, date, sizeof(d.date));
    d.weatherCode = _pickWeatherCode(times, hourlyCodes, hourlyProbs, precip,
                                     date, codes[day].as<int>());
    d.tempMax = maxT[day].as<float>();
    d.tempMin = minT[day].as<float>();
    d.precipProb = probs[day].as<int>();

    for (int quarter = 0; quarter < DAY_QUARTERS; ++quarter) {
      ForecastQuarter& q = quarters[day * DAY_QUARTERS + quarter];
      memcpy(q.weather.date, date, sizeof(q.weather.date));
      int firstHour = quarter * 6;
      int hoursSeen = 0;
      unsigned int hourMask = 0;
      bool missing = false;
      int codeCounts[100] = {};
      int dominantCode = 0;
      for (size_t i = 0; i < times.size(); ++i) {
        const char* ts = times[i];
        if (!ts || strlen(ts) < 13 || strncmp(ts, date, 10) != 0) continue;
        int hour = (ts[11] - '0') * 10 + ts[12] - '0';
        if (hour < firstHour || hour >= firstHour + 6) continue;
        if (temps[i].isNull() || hourlyCodes[i].isNull() || hourlyProbs[i].isNull()) {
          missing = true;
          continue;
        }
        float temp = temps[i].as<float>();
        if (hoursSeen == 0 || temp > q.weather.tempMax) q.weather.tempMax = temp;
        if (hoursSeen == 0 || temp < q.weather.tempMin) q.weather.tempMin = temp;
        int prob = hourlyProbs[i].as<int>();
        if (prob > q.weather.precipProb) q.weather.precipProb = prob;
        int code = hourlyCodes[i].as<int>();
        if (code >= 0 && code < 100 && ++codeCounts[code] > codeCounts[dominantCode]) {
          dominantCode = code;
        }
        ++hoursSeen;
        hourMask |= 1U << (hour - firstHour);
      }
      q.valid = hourMask == 0x3f && !missing;
      q.weather.weatherCode = _pickWeatherCode(times, hourlyCodes, hourlyProbs, precip,
                                               date, dominantCode, firstHour, firstHour + 6);
    }
  }
  memcpy(_forecast, days, sizeof(days));
  memcpy(_quarters, quarters, sizeof(quarters));
  _forecastValid = true;
  return true;
}

static int _forecastDayIndex(int day, int month, int year) {
  if (!_forecastValid) return -1;
  char date[11];
  snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);
  for (int i = 0; i < FORECAST_DAYS; ++i) {
    if (strcmp(date, _forecast[i].date) == 0) return i;
  }
  return -1;
}

static int _forecastStartIndex(int day, int month, int year, int hour) {
  int today = _forecastDayIndex(day, month, year);
  if (today < 0 || hour < 0 || hour > 23) return -1;
  int start = today + (hour >= 14 ? 1 : 0);
  return start + FORECAST_COLUMNS <= FORECAST_DAYS ? start : -1;
}

static int _quarterStartIndex(int day, int month, int year, int hour) {
  int today = _forecastDayIndex(day, month, year);
  if (today < 0 || hour < 0 || hour > 23) return -1;
  // Das laufende Viertel überspringen; auch über Mitternacht weiterschalten.
  int start = today * DAY_QUARTERS + hour / 6 + 1;
  if (start + FORECAST_COLUMNS > FORECAST_DAYS * DAY_QUARTERS) return -1;
  for (int i = 0; i < FORECAST_COLUMNS; ++i) {
    if (!_quarters[start + i].valid) return -1;
  }
  return start;
}

static const char* _quarterLabel(int index) {
  static const char* names[] = {"Nacht", "Vormittag", "Nachmittag", "Abend"};
  return names[index % DAY_QUARTERS];
}

#endif
