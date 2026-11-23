#pragma once

// build_opt.h bindet diese Optionen auch beim Übersetzen von TFT_eSPI ein.
// Die Hardwarekonfiguration liegt im Projekt, nicht in der installierten Library.
// Core-/C-Dateien benötigen keine TFT-Konfiguration.
#if defined(__cplusplus) && __has_include(<User_Setup_Select.h>)
#include "tft_setup.h"
#include <User_Setup_Select.h>

// Die Uhr nutzt nur die Standardschrift 1 (Start/OTA) und GFX-Bitmapfonts.
#undef LOAD_FONT2
#undef LOAD_FONT4
#undef LOAD_FONT6
#undef LOAD_FONT7
#undef LOAD_FONT8
#undef LOAD_FONT8N
#undef SMOOTH_FONT
#endif
