#ifndef TOUCH_CONTROL_H
#define TOUCH_CONTROL_H

#include <Arduino.h>
#include "lumiclock_state.h"
#include "ring_light.h"

extern const int TFT_LED;

#define TOUCH_PIN_CLOCK    T6
#define TOUCH_PIN_WEATHER  T7
#define TOUCH_PIN_MOON     T8
#define TOUCH_THRESHOLD_FACTOR_NUM  80  // 80% vom Leerlaufwert
#define TOUCH_THRESHOLD_FACTOR_DEN 100
#define TOUCH_DEBOUNCE_MS          350UL
#define TOUCH_BOOT_GUARD_MS       2000UL
#define BACKLIGHT_TIMEOUT_MS     10000UL

volatile bool _touchClock   = false;
volatile bool _touchWeather = false;
volatile bool _touchMoon    = false;

uint16_t _touchThresholdClock   = 30;
uint16_t _touchThresholdWeather = 30;
uint16_t _touchThresholdMoon    = 30;

unsigned long _touchInitAt        = 0;
unsigned long _lastTouchAcceptedAt = 0;

unsigned long _backlightOnAt   = 0;
bool          _backlightActive = false;

void IRAM_ATTR _onTouchClock()   { _touchClock   = true; }
void IRAM_ATTR _onTouchWeather() { _touchWeather = true; }
void IRAM_ATTR _onTouchMoon()    { _touchMoon    = true; }

void initTouch() {
  uint16_t idleClock   = touchRead(TOUCH_PIN_CLOCK);
  uint16_t idleWeather = touchRead(TOUCH_PIN_WEATHER);
  uint16_t idleMoon    = touchRead(TOUCH_PIN_MOON);

  _touchThresholdClock   = (idleClock   * TOUCH_THRESHOLD_FACTOR_NUM) / TOUCH_THRESHOLD_FACTOR_DEN;
  _touchThresholdWeather = (idleWeather * TOUCH_THRESHOLD_FACTOR_NUM) / TOUCH_THRESHOLD_FACTOR_DEN;
  _touchThresholdMoon    = (idleMoon    * TOUCH_THRESHOLD_FACTOR_NUM) / TOUCH_THRESHOLD_FACTOR_DEN;

  touchAttachInterrupt(TOUCH_PIN_CLOCK,   _onTouchClock,   _touchThresholdClock);
  touchAttachInterrupt(TOUCH_PIN_WEATHER, _onTouchWeather, _touchThresholdWeather);
  touchAttachInterrupt(TOUCH_PIN_MOON,    _onTouchMoon,    _touchThresholdMoon);

  _touchInitAt = millis();
  Serial.printf("[Touch] Idle C/W/M: %u/%u/%u, Thresholds: %u/%u/%u\n",
                idleClock, idleWeather, idleMoon,
                _touchThresholdClock, _touchThresholdWeather, _touchThresholdMoon);
}

static bool _isTouchAccepted(uint8_t pin, uint16_t threshold) {
  unsigned long now = millis();
  if (now - _touchInitAt < TOUCH_BOOT_GUARD_MS) return false;
  if (now - _lastTouchAcceptedAt < TOUCH_DEBOUNCE_MS) return false;

  uint16_t raw = touchRead(pin);
  bool accepted = raw <= threshold;
  if (accepted) _lastTouchAcceptedAt = now;
  return accepted;
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
    if (_isTouchAccepted(TOUCH_PIN_CLOCK, _touchThresholdClock)) {
      setState(STATE_CLOCK);
      _activateBacklight();
      setRingLight(20, 255, 230, 100);
      Serial.println("[Touch] Uhr");
    }
  }

  if (_touchWeather) {
    _touchWeather = false;
    if (_isTouchAccepted(TOUCH_PIN_WEATHER, _touchThresholdWeather)) {
      setState(STATE_WEATHER);
      _activateBacklight();
      Serial.println("[Touch] Wetter");
    }
  }

  if (_touchMoon) {
    _touchMoon = false;
    if (_isTouchAccepted(TOUCH_PIN_MOON, _touchThresholdMoon)) {
      setState(STATE_MOON);
      _activateBacklight();
      Serial.println("[Touch] Mond");
    }
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