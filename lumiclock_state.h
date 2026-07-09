#ifndef LUMICLOCK_STATE_H
#define LUMICLOCK_STATE_H

// ---------------------------------------------------------------------------
// Lumiclock – zentrale State Machine
//
// Einziger Ort wo DisplayState definiert und verwaltet wird.
// touch_control.h ruft setState() auf.
// lumiclock.ino liest currentState und stateRendered.
// ---------------------------------------------------------------------------

enum DisplayState {
  STATE_CLOCK,    // Uhrzeit + Datum
  STATE_WEATHER,  // Wetterdaten
  STATE_MOON,     // Mondphase
  STATE_FORECAST, // 3-Tages-Prognose
};

DisplayState  currentState    = STATE_CLOCK;
unsigned long stateEnteredAt  = 0;
bool          stateRendered   = false;

// Nicht-Uhr-States kehren nach diesem Timeout automatisch zur Uhr zurück
#define STATE_TIMEOUT_MS 10000UL

// ---------------------------------------------------------------------------
// Zustand wechseln – auch von touch_control.h aus aufrufbar
// ---------------------------------------------------------------------------
void setState(DisplayState newState) {
  if (newState != currentState) {
    currentState  = newState;
    stateEnteredAt = millis();
    stateRendered  = false;
  }
}

// ---------------------------------------------------------------------------
// Timeout prüfen – in jedem loop()-Durchlauf aufrufen
// ---------------------------------------------------------------------------
void updateState() {
  if (currentState != STATE_CLOCK) {
    if (millis() - stateEnteredAt > STATE_TIMEOUT_MS) {
      setState(STATE_CLOCK);
    }
  }
}

// Lesbare Abfragen für die .ino
bool isClockState()    { return currentState == STATE_CLOCK;    }
bool isWeatherState()  { return currentState == STATE_WEATHER;  }
bool isMoonState()     { return currentState == STATE_MOON;     }
bool isForecastState() { return currentState == STATE_FORECAST; }

#endif
