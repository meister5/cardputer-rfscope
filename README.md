# RFScope

A 2.4 GHz WiFi and BLE channel scanner and RSSI meter for the M5Stack Cardputer ADV (it also runs on the original Cardputer).

## Features

- Spectrum: per-channel occupancy and signal level over a scrolling waterfall, with an AP list per channel
- Connect to a network and watch link RSSI on a gauge with a scrolling trace
- Optional geiger-style audio whose pitch and click rate follow signal strength
- BLE scan with a live device list, plus a hot/cold tracker for one device
- Band view: WiFi channels and BLE advertising channels on one frequency axis
- Diagnostics screen (board, keyboard driver, heap, frame rate) and persistent settings

## What it measures

The ESP32-S3 cannot read raw RF energy at an arbitrary frequency, so this is not a swept-tuned spectrum analyser. Occupancy comes from counting frames in monitor mode per channel, and signal level from the RSSI of those frames, at 14-channel resolution.

- 2.4 GHz only, BLE only (no Bluetooth Classic), advertising channels 37/38/39 only.
- Channel 14 is shown only if the radio accepts it.
- Sweeping and metering cannot run at once: while connected, the sniffer watches only the AP's channel.
- WiFi and BLE share one radio, so the Band view runs at reduced duty cycle.

More detail in [docs/DESIGN.md](docs/DESIGN.md).

## Hardware

M5Stack Cardputer ADV or Cardputer v1.1. Board detection happens at runtime, so one build covers both.

## Install

Prebuilt images are in the repo root.

- M5Launcher: copy `CardputerRFScope-app.bin` to a FAT32 SD card and choose Install, or use `https://raw.githubusercontent.com/meister5/cardputer-rfscope/main/CardputerRFScope-app.bin` as an `OTA > Favorites` entry.
- M5Burner or esptool: use `CardputerRFScope-merged.bin`.

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x0 CardputerRFScope-merged.bin
```

## Usage

The keyboard has no arrow keys, so `;` `.` `,` `/` are used, with or without Fn.

| Key | Action |
| --- | --- |
| `;` / `.` | up / down |
| `,` / `/` | left / right (move cursor on Spectrum) |
| `ENTER` | select, or open a channel's AP list |
| `BKSP` or `` ` `` | back / cancel |
| `TAB` | show/hide password |
| `A` | toggle audio (meter screens) |
| `R` | rescan (network picker) |

Use RFScope only on networks you are authorised to survey. Monitor mode reads frame metadata (RSSI, length, channel) and does not decrypt or store traffic.

## Build

Requires `arduino-cli` with the M5Stack ESP32 core.

```bash
arduino-cli core install m5stack:esp32 \
  --additional-urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli lib install M5Cardputer NimBLE-Arduino
tools/package.sh   # writes dist/CardputerRFScope-app.bin and dist/CardputerRFScope-merged.bin
```

To compile only: `arduino-cli compile --fqbn "m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,PSRAM=disabled" CardputerRFScope`. Host unit tests: `make -C test`.

## License

MIT. See [LICENSE](LICENSE).
