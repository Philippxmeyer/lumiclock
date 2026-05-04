// =============================================================================
// lumiclock.ino
// ESP32 Kinderuhr mit Wetter, Mondphase und NeoPixel-Nachtlicht
// =============================================================================

#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// --- Eigene Module (Reihenfolge ist wichtig) ---------------------------------
#include "wifi_credentials.h"   // WIFI_SSID, WIFI_PASSWORD, connectToWiFi()
#include "ntp_client.h"         // initializeNTP(), getFormattedHours/Minutes(),
                                //   getCurrentHour/Minute(), getCurrentDate()
#include "lumiclock_state.h"    // DisplayState, setState(), updateState(),
                                //   isClockState/WeatherState/MoonState(),
                                //   stateRendered
#include "ring_light.h"         // initRingLight(), setRingLight(),
                                //   setNightLight(), turnOffRingLight()
#include "touch_control.h"      // initTouch(), handleTouch(),
                                //   isBacklightActive()
#include "weather.h"            // initWeather(), fetchWeather(),
                                //   renderWeather()
#include "moon_phase.h"         // renderMoonPhase()
#include "display_time.h"       // displayTime(hours, minutes)
#include "Baloo2_Bold40pt7b.h"
#include "Baloo2_Bold24pt8b.h"
#include "ota_update.h"         // setupOTA(), handleOTA()

// --- Globale Objekte ---------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
PNG      png;

// Wird von display_time.h, weather.h und moon_phase.h als extern erwartet
// (kein zweites Objekt anlegen – nur dieses hier existiert)

const int TFT_LED = 4;

// Wetter alle 30 Minuten holen (Open-Meteo: kein Key, faires Limit)
#define WEATHER_INTERVAL_MS (30UL * 60UL * 1000UL)

// Nachtmodus: 19:00 – 07:00
#define NIGHT_HOUR_START 19
#define NIGHT_HOUR_END    7

// =============================================================================
// LittleFS / PNG-Callbacks (werden von weather.h und moon_phase.h genutzt)
// =============================================================================
fs::File pngFile;
int16_t  pngXOffset = 0;
int16_t  pngYOffset = 0;

void* pngOpen(const char* filename, int32_t* size) {
  pngFile = LittleFS.open(filename, "r");
  if (!pngFile && filename[0] == '/') {
    pngFile = LittleFS.open(filename + 1, "r");  // Fallback für Images ohne führenden Slash
  }
  if (!pngFile) {
    Serial.printf("[PNG] Datei nicht gefunden: %s\n", filename);
    return NULL;
  }
  *size = pngFile.size();
  return &pngFile;
}

void pngClose(void* handle) {
  if (pngFile) pngFile.close();
}

int32_t pngRead(PNGFILE* handle, uint8_t* buffer, int32_t length) {
  if (!pngFile) return 0;
  return pngFile.read(buffer, length);
}

int32_t pngSeek(PNGFILE* handle, int32_t position) {
  if (!pngFile) return 0;
  return pngFile.seek(position);
}

int pngDraw(PNGDRAW* pDraw) {
  uint16_t lineBuffer[480];  // Breite des 3.5"-Displays (480px)
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xFFFFFFFF);
  // X/Y-Offset wird von weather.h / moon_phase.h als extern gesetzt
  extern int16_t pngXOffset, pngYOffset;
  tft.pushImage(pngXOffset, pngYOffset + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

// Wird von weather.h und moon_phase.h aufgerufen
bool drawPNG(const char* filename, int16_t x, int16_t y) {
  extern int16_t pngXOffset, pngYOffset;
  pngXOffset = x;
  pngYOffset = y;
  int rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (rc != PNG_SUCCESS) {
    Serial.printf("[PNG] Fehler beim Öffnen: %s (rc=%d)\n", filename, rc);
    return false;
  }
  png.decode(NULL, 0);
  png.close();
  return true;
}

// =============================================================================
// Nachtmodus
// =============================================================================
void handleNightMode() {
  int hour = getCurrentHour();
  bool isNight = (hour >= NIGHT_HOUR_START || hour < NIGHT_HOUR_END);

  if (isBacklightActive()) {
    // Touch hat das Backlight aktiviert – Nachtmodus pausieren
    digitalWrite(TFT_LED, HIGH);
    return;
  }

  if (isNight) {
    digitalWrite(TFT_LED, LOW);
    setNightLight();
  } else {
    digitalWrite(TFT_LED, HIGH);
    turnOffRingLight();
  }
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
  Serial.begin(115200);

  // Display hochfahren
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  // Touch früh initialisieren (ISR)
  initTouch();

  // LittleFS
  tft.println("LittleFS...");
  if (!LittleFS.begin(true)) {
    tft.println("LittleFS FEHLER!");
    Serial.println("[LittleFS] Initialisierung fehlgeschlagen!");
    while (1) delay(1000);
  }

  // WLAN
  tft.println("WLAN...");
  connectToWiFi();
  tft.println("WLAN OK");

  // Zeit
  tft.println("NTP...");
  initializeNTP();
  tft.println("Zeit OK");

  // OTA
  setupOTA();

  // NeoPixel
  initRingLight();

  // Wetter beim Start einmal laden
  tft.println("Wetter...");
  initWeather();   // setzt Koordinaten / URL zusammen
  fetchWeather();  // erster API-Abruf
  tft.println("Bereit!");

  delay(1000);
  tft.fillScreen(TFT_BLACK);

  // Startzustand: Uhr anzeigen
  setState(STATE_CLOCK);
}

// =============================================================================
// Loop
// =============================================================================
void loop() {
  handleOTA();
  handleTouch();
  updateState();  // prüft State-Timeout, kehrt ggf. zu STATE_CLOCK zurück

  // --- Anzeige ----------------------------------------------------------------
  switch (currentState) {

    case STATE_CLOCK: {
      // Nur neu zeichnen wenn Minute wechselt oder State gerade gewechselt hat
      static int lastMinute = -1;
      int minute = getCurrentMinute();
      if (!stateRendered || minute != lastMinute) {
        lastMinute    = minute;
        stateRendered = true;
        displayTime(getFormattedHours(), getFormattedMinutes());
      }
      break;
    }

    case STATE_WEATHER: {
      if (!stateRendered) {
        stateRendered = true;
        renderWeather();  // zeichnet Temperatur + Icon aus gecachten Daten
      }
      break;
    }

    case STATE_MOON: {
      if (!stateRendered) {
        stateRendered = true;
        int day, month, year;
        getCurrentDate(day, month, year);
        renderMoonPhase(day, month, year);
      }
      break;
    }
  }

  // --- Wetterdaten periodisch aktualisieren ----------------------------------
  static unsigned long lastWeatherFetch = 0;
  if (millis() - lastWeatherFetch > WEATHER_INTERVAL_MS) {
    lastWeatherFetch = millis();
    fetchWeather();
    // Falls gerade Wetter angezeigt wird: neu zeichnen
    if (isWeatherState()) stateRendered = false;
  }

  // --- Nachtmodus ------------------------------------------------------------
  handleNightMode();
}
