#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#include <WiFi.h>

// ---------------------------------------------------------------------------
// WLAN-Zugangsdaten hier eintragen
// Diese Datei gehört in .gitignore und nie ins Repository!
// ---------------------------------------------------------------------------
#define WIFI_SSID     "dein-wlan-name"
#define WIFI_PASSWORD "dein-wlan-passwort"

void connectToWiFi() {
  Serial.printf("[WLAN] Verbinde mit %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.printf("\n[WLAN] Verbunden, IP: %s\n",
    WiFi.localIP().toString().c_str());
}

#endif
