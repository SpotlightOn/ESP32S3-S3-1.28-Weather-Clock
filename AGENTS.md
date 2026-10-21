# AGENTS.md

Briefing for coding agents working in this repository. Read it before touching
code; it saves the troubleshooting that led to the current state.

## What this project is

The vendor Arduino package of the **Spotpear ESP32-S3 1.28" round weather clock**
(GC9A01 240x240 IPS panel), turned into a PlatformIO project. The sketch stays in
`AstronautClock_Prism`; all libraries come from the PlatformIO registry, so the
repository carries no library copies.

Firmware behaviour: butterfly GIF splash, then a text page with the WiFi hints,
then a blocking WiFi connect, NTP time sync, OpenWeatherMap weather, and finally
a clock UI (TFT_eSPI sprites) with an LVGL astronaut GIF.

## Hard rules

1. **English only** in code, identifiers, comments, serial output, on-screen text
   and commit messages. The sketch itself is fully translated. Chinese survives
   only in data assets (`CityCode/citycode.json`, the `.vlw` glyph tables) and in
   third-party libraries, which are out of scope - do not reintroduce it in the
   code you write.
2. **Never read, edit, grep or `cat` `config.h` in the project root.**
   It holds the local WiFi credentials and API key, is git-ignored, and is blocked
   in `opencode.jsonc`. Work with `config.h.example` only, and drive every new
   setting through a macro there plus an `#ifndef` fallback in the sketch.
   `platformio.ini` adds the project root to the include path (`-I.`), so
   `#include "config.h"` resolves from the sketch subdirectory. The shell rule
   matches the raw command text, so any command that spells the file name is
   denied as well: reference the template through the glob `*.example`, use the
   edit tool for text changes, and `git commit -F <file>` for messages that have
   to name it.
3. **Never downgrade TFT_eSPI to a release.** Releases up to and including 2.5.43
   define `SPI_PORT` as `FSPI` (= 1) for the ESP32-S3, so `REG_SPI_BASE()` yields
   `0` and `tft.begin()` panics with `StoreProhibited` while the panel stays
   black. `platformio.ini` pins the upstream commit that sets `SPI_PORT 2`; check
   that pin before touching `lib_deps`. See `README.md` for the register-level
   explanation.
4. **Do not restructure vendor code** without a concrete reason. Keep diffs small
   and explainable, and do not silence vendor warnings (LVGL const casts).
5. **No absolute paths, no home directory.** Never write this machine's home
   path, user name or absolute project paths into source, comments, docs or
   commit messages - use relative paths or `~`. `.vscode/` and `.pio/` stay
   git-ignored because the tooling writes absolute paths into them, and
   PlatformIO's ino2cpp step emits `#line "<absolute path>"` markers into its
   generated `.ino.cpp`; neither belongs in the repository.
6. **Leave data and binary assets alone:** `CityCode/citycode.json` (10 MB city
   dataset, Chinese names), `*.vlw` fonts, `img/*.h`, `font/*.h`, `AW4.h`.

## Layout

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Single env `spotpear_esp32s3`, `src_dir` points at the sketch |
| `partitions_16mb.csv` | 16 MB flash, 4 MB factory app (stock 1.2 MB is too small) |
| `config.h.example` | Tracked config template, copy it to `config.h` in the root |
| `config.h` | Local WiFi credentials and API key, git-ignored, off limits |
| `AstronautClock_Prism/*.ino` | Main firmware |
| `AstronautClock_Prism/weathernum.cpp/.h` | Weather icon lookup by OWM icon code |
| `AstronautClock_Prism/CityCode/` | City dataset + `.vlw` fonts, data only |
| `lv_conf.h` | LVGL config, picked up via `-DLV_CONF_INCLUDE_SIMPLE` |
| `tools/display_test/` | Standalone panel test, own `platformio.ini` |
| `opencode.jsonc` | Agent permissions: `config.h` is off limits |

## Build, flash, monitor

`pio` is usually not on `PATH`; use the PlatformIO venv binary:

```bash
~/.platformio/penv/bin/pio run -e spotpear_esp32s3          # ~45 s, ~44 % flash
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyACM0
~/.platformio/penv/bin/pio run -d tools/display_test -t upload
```

Serial capture without the monitor:

```bash
stty -F /dev/ttyACM0 115200 raw -echo && timeout 20 cat /dev/ttyACM0
```

A successful boot prints `Successfully opened GIF` and then WiFi dots. A clean
build must stay at `SUCCESS` with the same flash usage class; warnings are vendor
noise.

After a change to `lib_deps` the first build can fail at link time with
`undefined reference to setup()` / `loop()`; run it once more and it succeeds.

Opening and closing the port in a loop (a capture `while` loop, repeated `stty`)
toggles DTR/RTS on the native USB port and can leave the S3 stuck in download
mode: the log shows `boot:0x2 (DOWNLOAD(USB/UART0))` and `waiting for download`,
the panel stays dark, and only a power cycle recovers it. Open the port once, and
when a full boot log is needed start the capture before resetting instead of
reopening it.

## Hardware facts

* ESP32-S3, **16 MB** flash, native USB on GPIO19/20 (no USB-UART bridge). That is
  why `platformio.ini` sets `-DARDUINO_USB_CDC_ON_BOOT=1`; without it `Serial`
  goes to UART0 and nothing appears on the PC.
* Panel pins come from `build_flags` in `platformio.ini`: SCLK 4, MOSI 2, CS 5,
  DC 47, RST 38, backlight 42.
* Backlight is **active low** (GPIO42 low = on), so a lit backlight proves only
  that `setup()` reached its second line, nothing about SPI.
* No PSRAM use. App image is ~1.8 MB because every font, icon and GIF frame is
  compiled into flash.

## Firmware flow and pitfalls

* `setup()` order matters: backlight, `Serial.begin()`, `tft.begin()`,
  `tft.setRotation(4)`, then `tkr()` (LVGL init must not flush before the bus is
  up), then the GIF splash, the WiFi hint page, `get_wifi()`, NTP, city and
  weather.
* `get_wifi()` **blocks forever** in `while (WiFi.status() != WL_CONNECTED)` and
  only prints dots. Wrong credentials mean a permanent hang that needs a power
  cycle and looks exactly like the old black-screen bug. Keep that in mind when
  diagnosing "nothing happens": check the serial dots first.
* Time zone: `currentTimeZone(utc)` next to the NTP code decides the offset.
  `TIMEZONE_SOURCE 0` uses the OpenWeatherMap `timezone` field read in
  `getCityTime()` (divided as double, so half-hour zones such as +05:30 work);
  `TIMEZONE_SOURCE 1` uses `TIMEZONE_STD_OFFSET_HOURS` plus the EU summer time
  rule from `isEuSummerTime()` (last Sunday in March/October, 01:00 UTC). The
  config template ships Germany (1 + EU DST), the sketch fallback stays 0,
  because automatic mode silently falls back to UTC when the weather request
  fails and the vendor API key often does.
* Keep the offset arithmetic integral: `utc + (time_t)(timeZone * SECS_PER_HOUR)`.
  A 2026 timestamp does not fit a float mantissa, so a float sum would round the
  seconds away.
* `cityCode` is sent as `.../weather?q=<cityCode>`, i.e. OpenWeatherMap expects a
  **city name**. The numeric ids in `CityCode/citycode.json` belong to a Chinese
  weather service and do not answer this query, which is why
  `CITY_AUTO_DETECT = 1` (city from `ip-api.com`) is the default.
* NTP uses `time.nist.gov`; if it is unreachable `getNtpTime()` returns 0 and the
  clock shows 1970 until the next sync interval.
* Sprites (`TFT_eSprite clk`, `clkb`) are reused for every text page; `setColorDepth(8)`,
  `loadFont()` and `unloadFont()` must stay paired or the panel shows garbage.
* `tft.print*` comes from the Arduino `Print` base class, so `String` and
  `const char *` arguments both work.

## Conventions

* One logical change per commit; the message is a single short English sentence
  with no body.
* Keep `README.md` (user facing) and this file in sync when build steps, pins or
  configuration macros change.
* Never commit `config.h`, `*.bin` or `.pio/`; `.gitignore` covers them.
