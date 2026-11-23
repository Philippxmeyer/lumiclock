// Native Tests mit der installierten ArduinoJson-Library und echter Prognose-/Touchlogik.
#include <Arduino.h>
#include <cstdlib>
#include <iostream>
#include "forecast_data.h"
#include "lumiclock_state.h"

// Nur LED-Hardware ersetzen; der Touch-Zustandswechsel wird unverändert getestet.
#define RING_LIGHT_H
void setRingLight(uint8_t, uint8_t, uint8_t, uint8_t) {}
extern const int TFT_LED = 4;
#include "touch_control.h"

static int checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " << #expr << '\n'; \
  std::exit(1); } } while (false)

static JsonDocument fixture() {
  JsonDocument doc;
  const char* dates[] = {"2026-12-31", "2027-01-01", "2027-01-02", "2027-01-03"};
  for (int day = 0; day < 4; ++day) {
    doc["daily"]["time"][day] = dates[day];
    doc["daily"]["weather_code"][day] = 3;
    doc["daily"]["temperature_2m_max"][day] = 20 + day;
    doc["daily"]["temperature_2m_min"][day] = -5 + day;
    doc["daily"]["precipitation_probability_max"][day] = 60 + day;
    for (int hour = 0; hour < 24; ++hour) {
      int i = day * 24 + hour;
      char timestamp[17];
      snprintf(timestamp, sizeof(timestamp), "%sT%02d:00", dates[day], hour);
      doc["hourly"]["time"][i] = timestamp;
      doc["hourly"]["weather_code"][i] = 0;
      doc["hourly"]["temperature_2m"][i] = day * 100 + hour - 10.0f;
      doc["hourly"]["precipitation_probability"][i] = hour;
      doc["hourly"]["precipitation"][i] = 0.0f;
    }
  }
  return doc;
}

static void selectionAndValues() {
  _forecastValid = false;
  CHECK(_forecastStartIndex(31, 12, 2026, 13) == -1);
  CHECK(_quarterStartIndex(31, 12, 2026, 13) == -1);
  auto doc = fixture();
  CHECK(_loadForecastData(doc));
  for (int hour = 0; hour < 24; ++hour) {
    int daily = _forecastStartIndex(31, 12, 2026, hour);
    CHECK(daily == (hour < 14 ? 0 : 1));
    CHECK(_forecast[daily + 2].tempMax == (hour < 14 ? 22 : 23));
    int quarter = _quarterStartIndex(31, 12, 2026, hour);
    CHECK(quarter == hour / 6 + 1);
    for (int col = 0; col < 3; ++col) {
      int index = quarter + col;
      const ForecastDay& q = _quarters[index].weather;
      float expectedMin = (index / 4) * 100 + (index % 4) * 6 - 10;
      CHECK(q.tempMin == expectedMin);
      CHECK(q.tempMax == expectedMin + 5);
      CHECK(q.precipProb == (index % 4) * 6 + 5);
      CHECK(q.weatherCode == 0);
    }
  }
  CHECK(strcmp(_quarterLabel(1), "Vormittag") == 0);
  CHECK(strcmp(_quarterLabel(2), "Nachmittag") == 0);
  CHECK(strcmp(_quarterLabel(3), "Abend") == 0);
  CHECK(strcmp(_quarterLabel(4), "Nacht") == 0);
  // Cache vom Vortag nach dem Jahreswechsel weiterhin dem richtigen Datum zuordnen.
  CHECK(_forecastStartIndex(1, 1, 2027, 0) == 1);
  CHECK(_quarterStartIndex(1, 1, 2027, 0) == 5);
  CHECK(_forecastStartIndex(1, 1, 2027, 14) == -1); // +3 fehlt im alten Cache
  CHECK(_forecastStartIndex(30, 12, 2026, 12) == -1);
  CHECK(_quarterStartIndex(4, 1, 2027, 0) == -1);
  CHECK(_forecastStartIndex(31, 12, 2026, -1) == -1);
  CHECK(_quarterStartIndex(31, 12, 2026, 24) == -1);
}

static void weatherAggregation() {
  auto doc = fixture();
  doc["hourly"]["weather_code"][21] = 95; // nur Abend, nicht Tagesicon 10–18 Uhr
  doc["hourly"]["temperature_2m"][7] = -18.5;
  doc["hourly"]["temperature_2m"][10] = 12.5;
  doc["hourly"]["precipitation_probability"][10] = 59;
  CHECK(_loadForecastData(doc));
  CHECK(_forecast[0].weatherCode == 0);
  CHECK(_quarters[3].weather.weatherCode == 95);
  CHECK(_quarters[1].weather.tempMin == -18.5f);
  CHECK(_quarters[1].weather.tempMax == 12.5f);
  CHECK(_quarters[1].weather.precipProb == 59);
  doc["hourly"]["weather_code"][12] = 61;
  doc["hourly"]["weather_code"][13] = 65;
  doc["hourly"]["precipitation"][13] = 4;
  CHECK(_loadForecastData(doc));
  CHECK(_quarters[2].weather.weatherCode == 65);
  CHECK(_forecast[0].weatherCode == 65);
  doc = fixture();
  for (int hour = 6; hour < 12; ++hour) doc["hourly"]["weather_code"][hour] = 3;
  CHECK(_loadForecastData(doc));
  CHECK(_quarters[1].weather.weatherCode == 3);
  doc = fixture();
  for (int hour = 6; hour < 12; ++hour) doc["hourly"]["weather_code"][hour] = 45;
  CHECK(_loadForecastData(doc));
  CHECK(_quarters[1].weather.weatherCode == 45);
}

static void invalidDataAndCache() {
  auto good = fixture();
  CHECK(_loadForecastData(good));
  ForecastDay saved[4];
  ForecastQuarter savedQuarters[16];
  memcpy(saved, _forecast, sizeof(saved));
  memcpy(savedQuarters, _quarters, sizeof(savedQuarters));
  auto bad = fixture();
  bad["daily"]["temperature_2m_max"][3] = nullptr;
  CHECK(!_loadForecastData(bad));
  CHECK(memcmp(saved, _forecast, sizeof(saved)) == 0);
  CHECK(memcmp(savedQuarters, _quarters, sizeof(savedQuarters)) == 0);
  JsonDocument empty;
  CHECK(!_loadForecastData(empty));
  CHECK(memcmp(saved, _forecast, sizeof(saved)) == 0);
  bad = fixture();
  bad["hourly"]["temperature_2m"][8] = nullptr;
  CHECK(_loadForecastData(bad));
  CHECK(!_quarters[1].valid);
  CHECK(_quarters[2].valid);
  CHECK(_quarterStartIndex(31, 12, 2026, 0) == -1);
  CHECK(_quarterStartIndex(31, 12, 2026, 6) == 2);
  CHECK(_forecastStartIndex(31, 12, 2026, 13) == 0);
  bad = fixture();
  bad["hourly"]["time"][8] = "broken";
  CHECK(_loadForecastData(bad));
  CHECK(!_quarters[1].valid);
  bad = fixture();
  bad.remove("hourly");
  CHECK(_loadForecastData(bad));
  CHECK(_forecast[0].weatherCode == 3);
  CHECK(_quarterStartIndex(31, 12, 2026, 0) == -1);
}

static void weatherPress() {
  mockMillis += 400;
  _onTouchWeather();
  handleTouch();
}

static void touchSequenceAndTimeout() {
  mockMillis = 0;
  mockTouchValue = 100;
  initTouch();
  CHECK(_touchThresholdWeather == 50);
  mockTouchValue = 20;
  weatherPress();
  CHECK(isClockState()); // Bootschutz
  mockMillis = 2500;
  weatherPress();
  CHECK(isWeatherState());
  CHECK(isBacklightActive());
  CHECK(!stateRendered);
  _onTouchWeather();
  handleTouch();
  CHECK(isWeatherState()); // Entprellung
  weatherPress();
  CHECK(isQuarterForecastState());
  unsigned long entered = stateEnteredAt;
  stateRendered = true;
  weatherPress();
  CHECK(isForecastState());
  CHECK(!stateRendered);
  CHECK(stateEnteredAt > entered);
  weatherPress();
  CHECK(isWeatherState());
  weatherPress();
  CHECK(isQuarterForecastState());
  mockMillis += STATE_TIMEOUT_MS + 1;
  updateState();
  CHECK(isClockState());
  weatherPress();
  CHECK(isWeatherState());
  setState(STATE_MOON);
  weatherPress();
  CHECK(isWeatherState());
  mockMillis += 400;
  _onTouchMoon();
  handleTouch();
  CHECK(isMoonState());
  mockMillis += 400;
  _onTouchClock();
  handleTouch();
  CHECK(isClockState());
  mockTouchValue = 100;
  weatherPress();
  CHECK(isClockState()); // Fehltrigger

  mockTouchValue = 20;
  stateRendered = true;
  mockMillis += 400;
  _onTouchClock();
  handleTouch();
  CHECK(isClockState());
  CHECK(stateRendered); // Wiederholtes Berühren darf den TFT-Inhalt nicht löschen.
  CHECK(isBacklightActive());
}

int main() {
  selectionAndValues();
  weatherAggregation();
  invalidDataAndCache();
  touchSequenceAndTimeout();
  std::cout << checks << " Prüfungen bestanden (Tagesviertel, 14-Uhr-Grenze, Jahreswechsel, Cache, Touch).\n";
}
