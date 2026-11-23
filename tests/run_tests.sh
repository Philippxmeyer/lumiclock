#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
forecast_test_dir=$(mktemp -d /tmp/lumiclock-tests.XXXXXX)
trap 'rm -f "$forecast_test_dir/test_forecast" "$forecast_test_dir/test_startup" "$forecast_test_dir/test_clock_render"; rmdir "$forecast_test_dir"' EXIT
forecast_json_dir="${ARDUINO_LIBRARIES_DIR:-$HOME/Arduino/libraries}/ArduinoJson/src"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -I. -I"$forecast_json_dir" \
  tests/test_forecast.cpp -o "$forecast_test_dir/test_forecast"
"$forecast_test_dir/test_forecast"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -I. \
  tests/test_startup.cpp -o "$forecast_test_dir/test_startup"
"$forecast_test_dir/test_startup"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -I. \
  -I"${ARDUINO_LIBRARIES_DIR:-$HOME/Arduino/libraries}/TFT_eSPI" \
  tests/test_clock_render.cpp -o "$forecast_test_dir/test_clock_render"
"$forecast_test_dir/test_clock_render"
python3 -B tests/test_flash_optimization.py
