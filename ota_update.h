#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <ArduinoOTA.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// ---------------------------------------------------------------------------
// OTA-Setup – einmalig in setup() aufrufen
// ---------------------------------------------------------------------------
void setupOTA() {
  ArduinoOTA.setHostname("lumiclock");
  ArduinoOTA.setPassword("lumiclock-ota");

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Sketch" : "Filesystem";
    Serial.println("[OTA] Update startet: " + type);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("OTA Update...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = (progress * 100) / total;
    Serial.printf("[OTA] %u%%\r", pct);
    // Fortschrittsbalken auf Display
    tft.fillRect(10, 60, (pct * 460) / 100, 20, TFT_GREEN);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Abgeschlossen – starte neu");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Fehler [%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Authentifizierung");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Start");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Verbindung");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Empfang");
    else if (error == OTA_END_ERROR)     Serial.println("Abschluss");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Bereit – Hostname: lumiclock");
}

// ---------------------------------------------------------------------------
// OTA-Handler – in jedem loop()-Durchlauf aufrufen
// ---------------------------------------------------------------------------
void handleOTA() {
  ArduinoOTA.handle();
}

#endif
