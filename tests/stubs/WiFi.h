#pragma once
#include <Arduino.h>

#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6

struct MockIP {
  String toString() const { return "192.0.2.1"; }
};

struct MockWiFi {
  bool modeWorks = true;
  bool autoReconnect = false;
  bool canConnect = true;
  unsigned long connectAfter = 1000;
  unsigned long beganAt = 0;
  int beginCalls = 0;
  int selectedMode = 0;
  String ssid;
  String password;

  bool mode(int value) { selectedMode = value; return modeWorks; }
  void setAutoReconnect(bool value) { autoReconnect = value; }
  void begin(const char* name, const char* secret) {
    ++beginCalls;
    beganAt = millis();
    ssid = name;
    password = secret;
  }
  int status() const {
    return canConnect && beginCalls && millis() - beganAt >= connectAfter
      ? WL_CONNECTED : WL_DISCONNECTED;
  }
  MockIP localIP() const { return {}; }
};

inline MockWiFi WiFi;
