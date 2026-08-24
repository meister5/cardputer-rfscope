#!/usr/bin/env bash
# Builds the firmware and produces the two binary flavours that matter:
#
#   dist/CardputerRFScope-app.bin      raw app image  -> M5Launcher (SD/WebUI/OTA)
#   dist/CardputerRFScope-merged.bin   full image     -> M5Burner / esptool
#
# Usage: tools/package.sh [output-dir]
set -euo pipefail

SKETCH="CardputerRFScope"
FQBN="m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,PSRAM=disabled"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/dist}"
BUILD="$ROOT/build"

# The device has 8 MB of flash and no PSRAM (ESP32-S3FN8 on a Stamp-S3A).
FLASH_SIZE="8MB"
FLASH_MODE="dio"
FLASH_FREQ="80m"

# M5Launcher writes app images into an OTA partition, so the app must stay
# comfortably inside one. Fail the build rather than ship something the
# launcher would reject.
MAX_APP_BYTES=$((3342336))

cd "$ROOT"
rm -rf "$BUILD"
arduino-cli compile --fqbn "$FQBN" --output-dir "$BUILD" "$SKETCH"

APP="$BUILD/$SKETCH.ino.bin"
BOOTLOADER="$BUILD/$SKETCH.ino.bootloader.bin"
PARTITIONS="$BUILD/$SKETCH.ino.partitions.bin"

app_size=$(stat -c%s "$APP")
echo "app image: $app_size bytes (limit $MAX_APP_BYTES)"
if [ "$app_size" -gt "$MAX_APP_BYTES" ]; then
    echo "ERROR: app image will not fit an OTA partition" >&2
    exit 1
fi

# The app image must start with the ESP image magic or Launcher will not
# recognise it as an application binary.
magic=$(od -An -tx1 -N1 "$APP" | tr -d ' \n')
if [ "$magic" != "e9" ]; then
    echo "ERROR: app image magic is 0x$magic, expected 0xe9" >&2
    exit 1
fi

BOOT_APP0="$(dirname "$(command -v arduino-cli)")/../../.arduino15/packages/m5stack/hardware/esp32/3.3.9/tools/partitions/boot_app0.bin"
if [ ! -f "$BOOT_APP0" ]; then
    BOOT_APP0="$(find "$HOME/.arduino15/packages/m5stack" -name boot_app0.bin | head -1)"
fi
ESPTOOL="$(find "$HOME/.arduino15/packages/m5stack/tools/esptool_py" -maxdepth 2 -name esptool -type f | head -1)"

mkdir -p "$OUT"
cp "$APP" "$OUT/$SKETCH-app.bin"

if [ -n "$ESPTOOL" ] && [ -f "$BOOT_APP0" ]; then
    "$ESPTOOL" --chip esp32s3 merge-bin \
        -o "$OUT/$SKETCH-merged.bin" \
        --flash-mode "$FLASH_MODE" --flash-freq "$FLASH_FREQ" --flash-size "$FLASH_SIZE" \
        0x0 "$BOOTLOADER" \
        0x8000 "$PARTITIONS" \
        0xe000 "$BOOT_APP0" \
        0x10000 "$APP"
else
    echo "esptool or boot_app0 not found; falling back to the core's merged image"
    cp "$BUILD/$SKETCH.ino.merged.bin" "$OUT/$SKETCH-merged.bin"
fi

echo
echo "artifacts in $OUT:"
ls -l "$OUT"
