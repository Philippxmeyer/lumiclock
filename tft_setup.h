#pragma once

// Hardware aus der funktionierenden TFT_eSPI/User_Setup.h übernehmen.
// TFT_eSPI lädt diese Datei aus dem Sketch-Verzeichnis auf jedem Build-Rechner.
#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED
#endif
#define USER_SETUP_INFO "Lumiclock ESP32-WROOM-32 mit ILI9488"

#define ILI9488_DRIVER
#define TFT_RGB_ORDER TFT_BGR

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC   17
#define TFT_RST  16
// Kein MISO angeschlossen. Backlight steuert der Sketch separat an GPIO 4.
#define TFT_BL   -1

#define SPI_FREQUENCY 27000000

// Nur die tatsächlich verwendeten Bitmap-Schriften einbinden.
#define LOAD_GLCD
#define LOAD_GFXFF
