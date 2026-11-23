#pragma once
#include <Arduino.h>
#include <algorithm>
#include <vector>

#define PROGMEM
struct GFXglyph {
  uint32_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};
#include <Fonts/GFXFF/FreeSans18pt7b.h>

constexpr uint16_t TFT_BLACK = 0;
constexpr uint16_t TFT_RED = 0xF800;
constexpr uint16_t TFT_WHITE = 0xFFFF;
constexpr uint16_t TFT_DARKGREY = 0x7BEF;
inline bool mockSpriteAllocationFails = false;

class TFT_eSPI {
public:
  struct Push { int x, y, width, height; uint16_t color; };
  std::vector<uint16_t> pixels = std::vector<uint16_t>(480 * 320, 0x07E0);
  std::vector<Push> pushes;
  int clears = 0;
  int allocations = 0;
  int spriteDepth = 0;
  size_t allocatedBytes = 0;
  uint16_t bitmapFg = TFT_WHITE, bitmapBg = TFT_BLACK;
  int width() const { return 480; }
  int height() const { return 320; }
  void fillScreen(uint16_t color) { ++clears; std::fill(pixels.begin(), pixels.end(), color); }
};

class TFT_eSprite {
  TFT_eSPI* display;
  std::vector<uint8_t> bits;
  const GFXfont* font = nullptr;
  int w = 0, h = 0, scale = 1, cursorX = 0, cursorY = 0;
public:
  explicit TFT_eSprite(TFT_eSPI* target) : display(target) {}
  void setColorDepth(int depth) { display->spriteDepth = depth; }
  void* createSprite(int width, int height) {
    if (!bits.empty()) return bits.data();
    ++display->allocations;
    if (mockSpriteAllocationFails) return nullptr;
    w = width; h = height;
    bits.resize(((w + 7) / 8) * h + 1);
    display->allocatedBytes = bits.size();
    return bits.data();
  }
  void fillSprite(uint16_t color) { std::fill(bits.begin(), bits.end(), color ? 255 : 0); }
  void setFreeFont(const GFXfont* value) { font = value; }
  void setTextSize(int value) { scale = value; }
  void setTextColor(uint16_t, uint16_t) {}
  int fontHeight() const { return font->yAdvance * scale; }
  int textWidth(const String& text) const {
    int width = 0;
    for (size_t i = 0; i < text.size(); ++i) {
      unsigned char c = text[i];
      if (c < font->first || c > font->last) continue;
      const auto& g = font->glyph[c - font->first];
      width += (i + 1 == text.size() ? g.width + g.xOffset : g.xAdvance) * scale;
    }
    return width;
  }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }
  void print(const String& text) {
    for (unsigned char c : text) {
      if (c < font->first || c > font->last) continue;
      const auto& g = font->glyph[c - font->first];
      for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
          int bit = y * g.width + x;
          if (!(font->bitmap[g.bitmapOffset + bit / 8] & (0x80 >> (bit % 8)))) continue;
          for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx) {
            int px = cursorX + (g.xOffset + x) * scale + dx;
            int py = cursorY + (g.yOffset + y) * scale + dy;
            if (px >= 0 && px < w && py >= 0 && py < h) {
              int index = py * ((w + 7) / 8) + px / 8;
              bits[index] |= 0x80 >> (px % 8);
            }
          }
        }
      }
      cursorX += g.xAdvance * scale;
    }
  }
  void setBitmapColor(uint16_t fg, uint16_t bg) { display->bitmapFg = fg; display->bitmapBg = bg; }
  bool pushSprite(int tx, int ty, int sx, int sy, int width, int height) {
    if (bits.empty()) return false;
    display->pushes.push_back({tx, ty, width, height, display->bitmapFg});
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      int px = sx + x, py = sy + y;
      bool set = bits[py * ((w + 7) / 8) + px / 8] & (0x80 >> (px % 8));
      display->pixels[(ty + y) * display->width() + tx + x] = set ? display->bitmapFg : display->bitmapBg;
    }
    return true;
  }
};
