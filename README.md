# RFScope

A 2.4 GHz WiFi and BLE spectrum analyser, channel scanner and RSSI meter for the
**M5Stack Cardputer ADV** (and the original Cardputer).

Pick a network, type the password, and watch the signal strength live on an
analogue-style gauge with a scrolling trace — or sweep the whole band and see
which channels are actually busy.

---

## What it can honestly measure

The ESP32-S3 has **no RF spectrum-sweep peripheral**. There is no way to read raw
energy at an arbitrary frequency. So this is not a swept-tuned analyser, and any
firmware that claims otherwise on this chip is drawing you a picture.

What it does instead is real and useful:

| Claim | How it is measured | Resolution |
|---|---|---|
| Per-channel occupancy | Monitor mode, counting **every** frame the PHY delivers during a dwell window | 14 channels, not MHz |
| Per-channel signal level | Peak/mean/floor of `rx_ctrl.rssi` over that window | 1 dBm |
| Access points per channel | Periodic active scan overlaid on the sweep | — |
| Link RSSI | `esp_wifi_sta_get_ap_info` while associated | 1 dBm, ~25 Hz |
| BLE device RSSI | NimBLE observer, per advertisement | 1 dBm |
| BLE band activity | Advertisement rate on channels 37/38/39 | 3 channels only |

Known limits, stated up front:

- **2.4 GHz only.** The radio has no 5 GHz or 6 GHz front end.
- **BLE only — no Bluetooth Classic.** The ESP32-S3 has no Classic radio at all.
- **BLE sees only the advertising channels** (37/38/39 at 2402/2426/2480 MHz). The
  37 data channels are frequency-hopping between connected peers and are not
  observable with a scanner.
- **Channel 14** is used only where regulations allow it. The firmware asks the
  radio at boot and hides the channel if it is refused.
- **Sweeping and metering cannot run at once.** Associating with an AP pins the
  radio to that AP's channel, so while the meter is connected the sniffer can
  only watch that one channel.
- **WiFi + BLE share one radio.** The Band view runs both and says `COEX —
  reduced duty cycle` on screen, because both sides lose airtime.

## Screens

- **Spectrum** — per-channel bars over a scrolling waterfall. The wide dim bar is
  traffic volume, the narrow bright bar is signal strength, the yellow tick is
  the strongest beaconing AP. `,` `/` move the cursor, `ENTER` drills into a
  channel's AP list.
- **Connect** — network picker sorted by strength, showing auth mode, channel and
  a `*` for networks whose password is already saved. `ENTER` to join.
- **Meter** — the main event. Arc gauge with needle and peak-hold marker, large
  dBm readout, link quality %, min/avg/max/jitter, and a scrolling RSSI trace.
  Optional geiger-style audio whose pitch and click rate track signal strength,
  so you can survey a room without watching the screen.
- **BLE Scan** — live device list with RSSI bars, ageing out stale entries.
- **BLE Tracker** — the same gauge and trace for one chosen device. A hot/cold
  finder for a tag or beacon.
- **Band** — WiFi channels and BLE advertising channels plotted together on a
  true frequency axis.
- **Diagnostics** — board detected, keyboard driver in use, display mode, heap,
  frame rate, and a key echo. Check this first on a new device.
- **Settings** — theme, sweep dwell, audio, brightness, AP overlay, local sniff,
  forget saved networks. Persisted to NVS.

## Controls

The Cardputer keyboard has no arrow keys. The `;` `.` `,` `/` cluster is used
instead (with or without `Fn` — both are accepted).

| Key | Action |
|---|---|
| `;` / `.` | up / down |
| `,` / `/` | left / right |
| `ENTER` | select |
| `BKSP` | back |
| `` ` `` | back / cancel (and the way out of Diagnostics) |
| `TAB` | show/hide password |
| `Fn`+`TAB` | caps lock, in the password field |
| `A` | toggle audio, on the meter screens |
| `R` | rescan, on the network picker |

## Cardputer ADV notes

The ADV is **not** a drop-in for the original Cardputer, and this firmware
handles the differences explicitly:

- **Board detection is at runtime.** M5GFX probes G8/G9 to tell an ADV from an
  original, so the same FQBN (`m5stack:esp32:m5stack_cardputer`) is correct for
  both. There is no ADV-specific board in the M5Stack core.
- **The keyboard is a TCA8418 I²C controller, not a 74HC138 matrix.** The M5
  driver reads it as a **FIFO of press/release events** and processes only one
  event per `update()` call, whereas the original rescans the whole matrix every
  time. Two consequences are handled in `input.cpp` and `key_events.h`:
  1. The FIFO is drained several times per frame so fast typing cannot queue up.
  2. The driver's held-key list is stateful — a **dropped release event would
     leave a key held forever**. A watchdog force-releases any key held past a
     timeout and refuses to re-press it until the driver actually stops
     reporting it. This logic is unit-tested on the host.
- Audio goes through an **ES8311 codec** on the ADV rather than the original's
  NS4168; M5Unified handles the enable path once the board is detected.

## Building

Requires `arduino-cli` with the M5Stack ESP32 core.

```bash
arduino-cli core install m5stack:esp32
arduino-cli lib install M5Cardputer NimBLE-Arduino

# compile
arduino-cli compile \
  --fqbn "m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,PSRAM=disabled" \
  CardputerRFScope

# or build both release binaries at once
tools/package.sh
```

`tools/package.sh` writes:

| File | Use |
|---|---|
| `dist/CardputerRFScope-app.bin` | **M5Launcher** — copy to SD, or upload via WebUI/OTA |
| `dist/CardputerRFScope-merged.bin` | **M5Burner** custom firmware, or `esptool` at offset `0x0` |

## Installing

### M5Launcher

Launcher installs application binaries into an OTA app partition, so use the
**app** binary. A prebuilt copy sits at the repo root, refreshed at each
release, so you do not have to build anything:

<https://raw.githubusercontent.com/meister5/cardputer-rfscope/main/CardputerRFScope-app.bin>

That URL is directly downloadable, which is what an `OTA > Favorites` entry
needs. To install from SD instead:

1. Copy `CardputerRFScope-app.bin` to a FAT32 SD card.
2. In Launcher, open `SD`, select the file, choose `Install`.

It also works through `WUI` (browser upload) or as an `OTA > Favorites` entry
pointing at a release asset URL. The build is kept at ~1.3 MB — well inside a
standard OTA slot — and `tools/package.sh` fails the build if it ever outgrows
one.

### M5Burner

Use the **merged** binary as a custom firmware upload — it contains the
bootloader, partition table and app.

### esptool

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x0 \
  dist/CardputerRFScope-merged.bin
```

### Getting back to Launcher

RFScope does not touch the OTA boot partition — deliberately, so it cannot brick
a Launcher install. Return to Launcher the way Launcher documents for your
device.

## Tests

The signal statistics, channel-plan arithmetic and keyboard event logic are pure
C++ with no Arduino dependencies, so they are unit-tested on the host:

```bash
make -C test
```

Everything else is verified by a clean compile, in CI on every push.

## Layout

```
CardputerRFScope/
  CardputerRFScope.ino    entry point
  app.{h,cpp}             screen state machine, menu, settings, diagnostics
  screens_wifi.cpp        spectrum, channel detail, network picker, password, meter
  screens_ble.cpp         BLE list, tracker, band view
  radio_wifi.{h,cpp}      channel-hopping monitor-mode sweep engine
  radio_ble.{h,cpp}       NimBLE observer
  net_manager.{h,cpp}     association lifecycle
  input.{h,cpp}           Cardputer/ADV keyboard glue
  key_events.h            pure edge/repeat/stuck-key logic          (tested)
  signal_stats.h          RSSI ring, EWMA, peak hold, quality curve (tested)
  channel_map.h           2.4 GHz channel plan and overlap          (tested)
  storage.{h,cpp}         NVS settings and saved credentials
  ui_theme.{h,cpp}        retro and heat-map palettes
  ui_widgets.{h,cpp}      canvas, gauge, trace, bars
  audio_tone.{h,cpp}      geiger-style signal audio
test/                     host unit tests
tools/package.sh          release build
```

## Licence

MIT — see [LICENSE](LICENSE).

Use it on networks you are authorised to survey. Monitor mode observes frame
metadata (RSSI, length, channel) only; it does not decrypt or store traffic.
