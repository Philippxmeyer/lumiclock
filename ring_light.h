#ifndef RING_LIGHT_H
#define RING_LIGHT_H

#include <Adafruit_NeoPixel.h>

#define LED_PIN  12   // WS2812-Datenleitung
#define NUM_LEDS  8   // Anzahl LEDs im Ring

Adafruit_NeoPixel ring(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------------------------------------------------------------------
// Initialisierung – einmalig in setup() aufrufen
// ---------------------------------------------------------------------------
void initRingLight() {
  ring.begin();
  ring.show();  // Alle LEDs aus
}

// ---------------------------------------------------------------------------
// Alle LEDs auf eine Farbe setzen
// brightness: 0–255
// ---------------------------------------------------------------------------
void setRingLight(uint8_t brightness, uint8_t red, uint8_t green, uint8_t blue) {
  ring.setBrightness(brightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(red, green, blue));
  }
  ring.show();
}

// ---------------------------------------------------------------------------
// Alle LEDs ausschalten
// ---------------------------------------------------------------------------
void turnOffRingLight() {
  ring.setBrightness(0);
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, 0);
  }
  ring.show();
}

// ---------------------------------------------------------------------------
// Nachtlicht: sanftes Warmweiß, jede zweite LED aktiv
// Sehr niedrige Helligkeit damit Kinder nicht geweckt werden
// ---------------------------------------------------------------------------
void setNightLight() {
  ring.setBrightness(2);
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i % 2 == 0) {
      ring.setPixelColor(i, 0);
    } else {
      ring.setPixelColor(i, ring.Color(170, 128, 0));  // Warmweiß
    }
  }
  ring.show();
}

#endif
