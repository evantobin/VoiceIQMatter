#!/usr/bin/env bash
# Delta Touch2O Matter — ESP-IDF + ESP-Matter build environment.

if [ -z "${IDF_PATH:-}" ]; then
  delta_idf_export="${ESP_IDF_EXPORT:-$HOME/.espressif/v6.0.2/esp-idf/export.sh}"
  if [ ! -f "$delta_idf_export" ]; then
    echo "ESP-IDF export.sh was not found at: $delta_idf_export" >&2
    return 1 2>/dev/null || exit 1
  fi
  . "$delta_idf_export"
fi

if [ -z "${ESP_MATTER_PATH:-}" ]; then
  export ESP_MATTER_PATH="$HOME/esp/esp-matter"
fi
if [ ! -f "$ESP_MATTER_PATH/export.sh" ]; then
  echo "ESP-Matter export.sh was not found at: $ESP_MATTER_PATH/export.sh" >&2
  return 1 2>/dev/null || exit 1
fi
. "$ESP_MATTER_PATH/export.sh"

if [ ! -f sdkconfig ] || ! grep -q '^CONFIG_IDF_TARGET="esp32c6"$' sdkconfig; then
  idf.py set-target esp32c6
fi

echo "Delta Touch2O Matter XIAO ESP32-C6 environment ready."
