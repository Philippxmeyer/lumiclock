"""Vergleicht echte Font-Pixel und TFT-Konfiguration vor/nach der Verkleinerung."""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("clock_font", ROOT / "tools/make_clock_font.py")
font_tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(font_tool)


class FlashOptimizationTests(unittest.TestCase):
    def test_clock_glyphs_are_pixel_identical(self):
        before, glyphs_before, first_before, _, advance_before = font_tool.read_font(font_tool.SOURCE)
        after, glyphs_after, first_after, last_after, advance_after = font_tool.read_font(font_tool.TARGET)
        self.assertEqual((first_after, last_after), (ord('-'), ord(':')))
        self.assertEqual(advance_before, advance_after)
        for code in range(first_after, last_after + 1):
            with self.subTest(character=chr(code)):
                old = glyphs_before[code - first_before]
                new = glyphs_after[code - first_after]
                self.assertEqual(old[1:], new[1:])  # Breite, Höhe, Laufweite und Offsets
                size = (old[1] * old[2] + 7) // 8
                self.assertEqual(before[old[0]:old[0]+size], after[new[0]:new[0]+size])
        self.assertLess(len(after), len(before) // 4)

    def test_font_range_covers_every_clock_string(self):
        _, _, first, last, _ = font_tool.read_font(font_tool.TARGET)
        for hour in ["--"] + [f"{h:02}" for h in range(24)]:
            for minute in ["--"] + [f"{m:02}" for m in range(60)]:
                self.assertTrue(all(first <= ord(c) <= last for c in f"{hour}:{minute}"))

    def macros(self, include, language="c++", extra_include_paths=(), following=()):
        libraries = Path(os.environ.get("ARDUINO_LIBRARIES_DIR", Path.home() / "Arduino/libraries"))
        command = ["g++", "-E", "-dM", "-x", language, "-I", str(ROOT)]
        for path in extra_include_paths:
            command.extend(["-I", str(path)])
        command.extend(["-I", str(libraries / "TFT_eSPI")])
        for path in (include, *following):
            command.extend(["-include", path])
        command.append("/dev/null")
        result = subprocess.run(
            command,
            check=True, capture_output=True, text=True)
        return dict(line.removeprefix("#define ").split(" ", 1)
                    if " " in line.removeprefix("#define ")
                    else (line.removeprefix("#define "), "")
                    for line in result.stdout.splitlines())

    def test_display_matches_working_ili9488_hardware(self):
        after = self.macros("lumiclock_build.h")
        expected = {
            "TFT_DRIVER": "0x9488", "TFT_WIDTH": "320", "TFT_HEIGHT": "480",
            "TFT_MOSI": "23", "TFT_SCLK": "18", "TFT_CS": "5",
            "TFT_DC": "17", "TFT_RST": "16", "TFT_BL": "-1",
            "SPI_FREQUENCY": "27000000", "TFT_RGB_ORDER": "TFT_BGR",
        }
        for name, value in expected.items():
            self.assertEqual(after.get(name), value, name)
        self.assertIn("ILI9488_DRIVER", after)
        self.assertIn("SPI_18BIT_DRIVER", after)
        self.assertIn("USER_SETUP_LOADED", after)
        self.assertNotIn("ILI9341_DRIVER", after)
        self.assertNotIn("TFT_MISO", after)  # Referenz nutzt kein MISO, insbesondere nicht GPIO 12.
        removed = {"LOAD_FONT2", "LOAD_FONT4", "LOAD_FONT6", "LOAD_FONT7", "LOAD_FONT8",
                   "LOAD_FONT8N", "SMOOTH_FONT"}
        for name in removed:
            self.assertNotIn(name, after)
        self.assertIn("LOAD_GLCD", after)  # Start-/OTA-Text
        self.assertIn("LOAD_GFXFF", after)  # Alle sichtbaren Bitmap-Schriften

    def test_direct_sketch_setup_and_forced_setup_agree(self):
        # TFT_eSPI.h lädt tft_setup.h auch selbst. Beide Einbindepfade müssen
        # denselben Treiber und dieselben Klasseneinstellungen verwenden.
        direct = self.macros("tft_setup.h", following=("User_Setup_Select.h",))
        forced = self.macros("lumiclock_build.h")
        self.assertEqual(direct, forced)

    def test_global_library_setup_is_not_loaded(self):
        with tempfile.TemporaryDirectory(prefix="lumiclock-tft-test-") as folder:
            (Path(folder) / "User_Setup.h").write_text(
                '#error "Die globale TFT-Konfiguration darf nicht geladen werden"\n')
            # Kontrollieren, dass der vorgeschaltete Library-Default tatsächlich
            # gefunden würde, wenn die Projektkonfiguration fehlen sollte.
            with self.assertRaises(subprocess.CalledProcessError):
                self.macros("User_Setup_Select.h", extra_include_paths=(folder,))
            after = self.macros("lumiclock_build.h", extra_include_paths=(folder,))
            self.assertEqual(after["TFT_DRIVER"], "0x9488")

    def test_c_files_do_not_import_display_settings(self):
        after = self.macros("lumiclock_build.h", "c")
        self.assertNotIn("USER_SETUP_LOADED", after)
        self.assertNotIn("TFT_DRIVER", after)


if __name__ == "__main__":
    unittest.main()
