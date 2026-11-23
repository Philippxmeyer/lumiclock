#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
forecast_test_dir=$(mktemp -d /tmp/lumiclock-tests.XXXXXX)
trap 'rm -f "$forecast_test_dir/test_forecast"; rmdir "$forecast_test_dir"' EXIT
forecast_json_dir="${ARDUINO_LIBRARIES_DIR:-$HOME/Arduino/libraries}/ArduinoJson/src"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -I. -I"$forecast_json_dir" \
  tests/test_forecast.cpp -o "$forecast_test_dir/test_forecast"
"$forecast_test_dir/test_forecast"
python3 -B tests/test_flash_optimization.py
