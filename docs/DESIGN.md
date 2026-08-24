# RFScope design notes

Why the firmware is shaped the way it is. The README covers what it does; this
covers the constraints that decided how.

## The radio is a mode, not a setting

There is one 2.4 GHz radio, and the ESP-IDF rules about it are strict:

- Monitor (promiscuous) mode can hop freely **only while unassociated**.
- Associating pins the radio to the AP's channel; promiscuous mode then sees
  that channel and nothing else.
- A scan cannot run while promiscuous mode owns the radio.
- BLE and WiFi share the front end and trade airtime.

So screens map onto exclusive radio states rather than pretending to coexist:

| Screen | Radio |
|---|---|
| Spectrum / Channel detail | unassociated, monitor, hopping 1–13(14) |
| Network picker | unassociated, active scan |
| Meter | associated; monitor pinned to the link's channel, if enabled |
| BLE list / tracker | NimBLE observer |
| Band | both, with coexistence stated on screen |

`App::applyRadioForScreen()` owns every transition. One subtlety: the meter is
entered while the connection is still being established, so the decision to
sniff the local channel is re-evaluated in `App::loop()` once the link is
actually up — and undone if it drops.

## Sweep engine

`WifiSweeper` parks on a channel for a dwell window (default 120 ms, adjustable
40–400 ms) and accumulates frame count, byte count, and RSSI sum/peak/min from
the promiscuous callback. The callback runs on the WiFi task, so it writes into
a spinlock-guarded accumulator and never allocates or blocks. The UI task drains
it once per dwell.

Occupancy bars are scaled against the busiest channel of the current sweep
rather than an absolute limit, so the display stays readable whether the band is
dead or saturated.

Every `AP_SCAN_INTERVAL_MS` the sweep yields the radio to an asynchronous scan
so the bars can be labelled with SSIDs and AP counts, then resumes. The scan is
a small state machine (`Idle → Requested → Running`) rather than a blocking call.

Channel 14 is probed once at boot with `esp_wifi_set_channel(14, …)`. If the
regulatory configuration refuses it, the channel is marked unusable and drawn as
a dash instead of being silently omitted.

## Cardputer ADV keyboard

This is the sharpest hardware difference and the one most likely to produce a
bug that only shows up on real hardware.

The original Cardputer scans a 74HC138 matrix on every `update()`, so the set of
held keys is always ground truth. The ADV uses a TCA8418 I²C controller that
reports **press and release events through a FIFO**, and M5Cardputer's reader
maintains its held-key list by adding on press and erasing on release —
**processing at most one event per `updateKeyList()` call**.

Two failure modes follow:

1. **Queueing.** One event per frame at 25 fps means fast typing lags behind.
   `Input::poll()` drains the FIFO up to 12 times per frame on the ADV (and
   exactly once on the original, where extra calls would mean redundant full
   matrix scans).
2. **Stuck keys.** If a release event is ever lost, that key stays in the list
   forever and the UI sees it held until reboot. `KeyEventGen` force-releases any
   key held past `stuckTimeoutMs` (4 s — long enough that a deliberate hold still
   delivers ~50 auto-repeats first) and then **suppresses** it until the driver
   stops reporting it, so it cannot immediately re-press in a loop.

`KeyEventGen` is deliberately free of Arduino headers so this recovery logic is
unit-tested on the host, where the failure can actually be simulated.

The keyboard has no arrow keys, so `;` `.` `,` `/` are the navigation cluster,
accepted with or without `Fn`.

## Rendering

The Stamp-S3A has **no PSRAM** — roughly 320 KB of usable SRAM. A single
240×135 16-bit canvas costs 64.8 KB, which is affordable exactly once, so there
is one full-screen sprite and no per-widget buffers. If the allocation fails,
`ui::gfx()` returns the panel instead and the app runs undoubled-buffered;
Diagnostics reports which mode is active.

Screens draw against `lgfx::LovyanGFX&`, the common base of `M5GFX` and
`M5Canvas`, so they are written once and work either way.

## BLE

Advertisements arrive on the NimBLE host task. Rather than lock the device
vector or allocate in a callback, each report is copied into a fixed POD ring
buffer under a spinlock and merged into the device table later on the UI task.
Overflow increments a dropped counter that Diagnostics displays — if it is
non-zero, the frame rate is starving the staging ring, which is worth knowing
rather than hiding.

NimBLE is initialised lazily, on first entry to a BLE screen, because the stack
costs heap that the sweep-only screens do not need to pay for.

## Distribution

Two artifacts, because the two tools want different things:

- **App binary** (`0xE9` header, no bootloader). M5Launcher writes this into an
  OTA app partition. `tools/package.sh` fails the build if it exceeds an OTA
  slot, since a too-large image is exactly the failure Launcher cannot work
  around.
- **Merged binary** (bootloader `0x0` + partition table `0x8000` + `boot_app0`
  `0xe000` + app `0x10000`). For M5Burner and `esptool`.

Partition scheme is `default_8MB`: dual OTA with 3.1 MB slots, matching the
ADV's real 8 MB flash. `huge_app` was rejected — it has no OTA slot at all,
which would make the firmware un-installable by Launcher.

The firmware never writes the OTA boot partition. Returning to Launcher is
Launcher's job, and a spectrum analyser has no business rearranging someone's
boot configuration.
