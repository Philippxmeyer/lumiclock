#pragma once
#include <stdint.h>
#include <string>

using String = std::string;

#define IRAM_ATTR
#define T6 14
#define T7 27
#define T8 33
#define HIGH 1

inline unsigned long mockMillis = 0;
inline uint16_t mockTouchValue = 100;
inline unsigned long millis() { return mockMillis; }
inline void delay(unsigned long ms) { mockMillis += ms; }
inline uint16_t touchRead(uint8_t) { return mockTouchValue; }
inline void touchAttachInterrupt(uint8_t, void (*)(), uint16_t) {}
inline void digitalWrite(int, int) {}
struct MockSerial {
  template <typename... Args> void printf(const char*, Args...) {}
  void print(const char*) {}
  void println(const char*) {}
};
inline MockSerial Serial;
