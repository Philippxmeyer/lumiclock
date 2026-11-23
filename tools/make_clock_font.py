#!/usr/bin/env python3
"""Erzeugt den pixelgleichen Ausschnitt '-' bis ':' aus der vollen Uhrenschrift."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Baloo2_Bold40pt7b.h"
TARGET = ROOT / "Baloo2_Clock40pt7b.h"


def read_font(path):
    text = path.read_text()
    bitmap_part = text.split("Bitmaps[] PROGMEM = {", 1)[1].split("};", 1)[0]
    bitmap = bytes(int(n, 16) for n in re.findall(r"0x([0-9A-Fa-f]{2})", bitmap_part))
    glyph_part = text.split("Glyphs[] PROGMEM = {", 1)[1].split("};", 1)[0]
    glyphs = [tuple(map(int, row)) for row in re.findall(
        r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}",
        glyph_part)]
    first, last, advance = re.search(
        r"0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*(\d+)\s*\}", text).groups()
    return bitmap, glyphs, int(first, 16), int(last, 16), int(advance)


def generate():
    bitmap, glyphs, first, _, advance = read_font(SOURCE)
    selected = glyphs[ord('-') - first:ord(':') - first + 1]
    packed = bytearray()
    adjusted = []
    for offset, width, height, *metrics in selected:
        adjusted.append((len(packed), width, height, *metrics))
        packed.extend(bitmap[offset:offset + (width * height + 7) // 8])
    lines = ["#pragma once", "", "// Generiert mit tools/make_clock_font.py; Original-Pixel und Zeichenabstände.",
             "const uint8_t Baloo2_Clock40pt7bBitmaps[] PROGMEM = {"]
    for i in range(0, len(packed), 12):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in packed[i:i+12]) + ",")
    lines.extend(["};", "", "const GFXglyph Baloo2_Clock40pt7bGlyphs[] PROGMEM = {"])
    for code, row in enumerate(adjusted, ord('-')):
        lines.append("  { " + ", ".join(map(str, row)) + f" }}, // {chr(code)}")
    lines.extend(["};", "", "const GFXfont Baloo2_Clock40pt7b PROGMEM = {",
                  "  (uint8_t*)Baloo2_Clock40pt7bBitmaps,",
                  "  (GFXglyph*)Baloo2_Clock40pt7bGlyphs,",
                  f"  0x2D, 0x3A, {advance}", "};", ""])
    TARGET.write_text("\n".join(lines))
    print(f"Uhrenschrift: {len(bitmap)} → {len(packed)} Bitmap-Bytes.")


if __name__ == "__main__":
    generate()
