#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>
#include <WiFi.h>

// Der Startablauf darf erst nach erfolgreichem WLAN-Aufbau NTP/Wetter erwarten.
// Diese Logik gehört zur Firmware, nicht in die private Zugangsdaten-Datei.
bool initializeWiFi(const char* ssid, const char* password,
                    unsigned long timeoutMs = 20000UL) {
  Serial.print("[WLAN] Verbinde");
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("\n[WLAN] Station-Modus konnte nicht gestartet werden");
    return false;
  }
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startedAt >= timeoutMs) {
      Serial.printf("\n[WLAN] Zeitlimit erreicht, Status: %d\n", WiFi.status());
      return false;
    }
    Serial.print(".");
    delay(250);
  }
  Serial.printf("\n[WLAN] Verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

#endif
