#ifndef TOUCH_CONTROL_H
#define TOUCH_CONTROL_H

#include <Arduino.h>
#include "lumiclock_state.h"
#include "ring_light.h"

extern const int TFT_LED;

#define TOUCH_PIN_CLOCK    T6
#define TOUCH_PIN_WEATHER  T7
#define TOUCH_PIN_MOON     T8
#define TOUCH_THRESHOLD    30
#define BACKLIGHT_TIMEOUT_MS 10000UL

volatile bool _touchClock   = false;
volatile bool _touchWeather = false;
volatile bool _touchMoon    = false;

unsigned long _backlightOnAt   = 0;
bool          _backlightActive = false;

void IRAM_ATTR _onTouchClock()   { _touchClock   = true; }
void IRAM_ATTR _onTouchWeather() { _touchWeather = true; }
void IRAM_ATTR _onTouchMoon()    { _touchMoon    = true; }

void initTouch() {
  touchAttachInterrupt(TOUCH_PIN_CLOCK,   _onTouchClock,   TOUCH_THRESHOLD);
  touchAttachInterrupt(TOUCH_PIN_WEATHER, _onTouchWeather, TOUCH_THRESHOLD);
  touchAttachInterrupt(TOUCH_PIN_MOON,    _onTouchMoon,    TOUCH_THRESHOLD);
}

static void _activateBacklight() {
  digitalWrite(TFT_LED, HIGH);
  _backlightOnAt   = millis();
  _backlightActive = true;
  stateRendered    = false;  // Anzeige nach Aufwecken neu zeichnen
}

void handleTouch() {
  if (_touchClock) {
    _touchClock = false;
    setState(STATE_CLOCK);
    _activateBacklight();
    setRingLight(20, 255, 230, 100);
    Serial.println("[Touch] Uhr");
  }

  if (_touchWeather) {
    _touchWeather = false;
    setState(STATE_WEATHER);
    _activateBacklight();
    Serial.println("[Touch] Wetter");
  }

  if (_touchMoon) {
    _touchMoon = false;
    setState(STATE_MOON);
    _activateBacklight();
    Serial.println("[Touch] Mond");
  }

  // Backlight-Timeout
  if (_backlightActive && millis() - _backlightOnAt > BACKLIGHT_TIMEOUT_MS) {
    _backlightActive = false;
    Serial.println("[Touch] Backlight-Timeout");
  }
}

bool isBacklightActive() {
  return _backlightActive;
}

#endif