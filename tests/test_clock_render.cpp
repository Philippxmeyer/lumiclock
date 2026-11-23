#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdlib>
#include <iostream>
#include <time.h>

TFT_eSPI tft;
static bool mockTimeValid = true;
static int mockDay = 23;
static bool _getTime(struct tm& t) {
  t = {};
  t.tm_year = 126;
  t.tm_mon = 10;
  t.tm_mday = mockDay;
  t.tm_wday = mockDay == 23 ? 1 : 2;
  return mockTimeValid;
}
#include "display_time.h"

static int checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " << #expr << '\n'; std::exit(1); } } while (false)

int main() {
  auto untouched = tft.pixels;
  mockSpriteAllocationFails = true;
  CHECK(!displayTime("18", "58", true));
  CHECK(tft.pixels == untouched);
  CHECK(tft.pushes.empty());
  mockSpriteAllocationFails = false;
  CHECK(displayTime("18", "58", true));
  CHECK(tft.clears == 0);
  CHECK(tft.spriteDepth == 1);
  CHECK(tft.allocatedBytes == 19201);
  CHECK(tft.pushes.size() == 2);
  CHECK(tft.pushes[0].color == TFT_RED);
  CHECK(tft.pushes[1].color == TFT_DARKGREY);
  CHECK(tft.pushes[0].height == 200 && tft.pushes[1].y == 200);
  CHECK(std::count(tft.pixels.begin(), tft.pixels.end(), TFT_RED) > 0);
  CHECK(std::count(tft.pixels.begin(), tft.pixels.end(), TFT_DARKGREY) > 0);
  CHECK(std::count(tft.pixels.begin(), tft.pixels.end(), 0x07E0) == 0);

  auto first = tft.pixels;
  CHECK(displayTime("18", "58"));
  CHECK(tft.pushes.size() == 2);
  CHECK(tft.pixels == first);
  CHECK(displayTime("18", "59"));
  CHECK(tft.pushes.size() == 3);
  CHECK(tft.pushes.back().color == TFT_RED);
  CHECK(tft.clears == 0);
  CHECK(tft.pixels != first);
  CHECK(std::equal(first.begin() + 480 * 200, first.end(), tft.pixels.begin() + 480 * 200));

  auto wide = tft.pixels;
  CHECK(displayTime("11", "11"));
  CHECK(tft.pushes.size() == 4);
  CHECK(std::count(tft.pixels.begin(), tft.pixels.begin() + 480 * 200, TFT_RED)
        < std::count(wide.begin(), wide.begin() + 480 * 200, TFT_RED));
  CHECK(std::equal(wide.begin() + 480 * 200, wide.end(), tft.pixels.begin() + 480 * 200));

  mockDay = 24;
  CHECK(displayTime("00", "00"));
  CHECK(tft.pushes.size() == 6);
  CHECK(tft.clears == 0);
  std::fill(tft.pixels.begin(), tft.pixels.end(), 0x07E0); // Andere Ansicht war aktiv.
  CHECK(displayTime("00", "00", true));
  CHECK(tft.pushes.size() == 8);
  CHECK(std::count(tft.pixels.begin(), tft.pixels.end(), 0x07E0) == 0);

  mockTimeValid = false;
  CHECK(displayTime("--", "--"));
  CHECK(std::count(tft.pixels.begin() + 480 * 200, tft.pixels.end(), TFT_DARKGREY) == 0);
  mockTimeValid = true;
  CHECK(displayTime("00", "01"));
  CHECK(std::count(tft.pixels.begin() + 480 * 200, tft.pixels.end(), TFT_DARKGREY) > 0);
  CHECK(tft.clears == 0);
  CHECK(tft.allocations == 2); // Ein fehlgeschlagener Versuch und ein wiederverwendeter Puffer.
  std::cout << checks << " Prüfungen bestanden (Uhr ohne Löschbild, Minutenwechsel, Datum, Puffer).\n";
}
