# PlatformIO build for the Spotpear ESP32-S3 1.28" weather clock

Original firmware and manufacturer page:
<https://spotpear.com/wiki/ESP32-S3-N16R8-AI-DeepSeek-XiaoZhi-XiaGe-Qwen-DouBao-1.28-inch-Round-LCD-BOX-TouchScreen.html>

This folder turns the vendor Arduino package into a PlatformIO project.
The sketch stays in `AstronautClock_Prism`; the libraries come from the
PlatformIO registry, so the repository holds the firmware only.

## Open the project

1. Install the **PlatformIO IDE** extension in VS Code.
2. `File > Open Folder...` and select this `Arduino` folder (the one that
   contains `platformio.ini`).
3. PlatformIO detects the project automatically and installs the missing
   toolchain packages on the first build.

## Build, upload, monitor

```bash
pio run                 # compile
pio run -t upload       # compile + flash (USB: /dev/ttyACM0)
pio device monitor      # serial output, 115200 baud
```

The same commands are available in the PlatformIO toolbar of VS Code.

## Project layout

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Project and environment configuration, including the panel setup |
| `config.h.example` | Template for the local settings (WiFi, API key, time zone) |
| `lv_conf.h` | LVGL configuration, found through `-DLV_CONF_INCLUDE_SIMPLE` |
| `partitions_16mb.csv` | 16 MB flash layout, 4 MB application slot |
| `AstronautClock_Prism/` | Vendor sketch, fonts, icons, animation data |

## Libraries

Everything comes from the PlatformIO registry, so the repository carries the
sketch only:

| Library | Version |
| --- | --- |
| `TFT_eSPI` | pinned commit `83d4d16` (2.5.43 + S3 SPI fix) |
| `lvgl` | 8.3.7 (the version the vendor sketch was written against) |
| `AnimatedGIF` | 2.1.1 |
| `ArduinoJson` | 6.x, because the sketch uses `DynamicJsonDocument` |
| `TJpg_Decoder` | 1.0.8 |
| `Time` | 1.6.1 |

`TFT_eSPI` is the one exception: release 2.5.43 still defines `SPI_PORT` as
`FSPI` for the ESP32-S3, which maps to register base `0` and crashes the first
SPI write. The fix is on `master` only, so `platformio.ini` pins the commit
that contains it. See the crash analysis below.

The panel setup lives in `build_flags` instead of the vendor `User_Setup.h`:

| Signal | GPIO |
| --- | --- |
| SCLK | 4 |
| MOSI | 2 |
| CS | 5 |
| DC | 47 |
| RST | 38 |
| Backlight | 42 |

Driver: `GC9A01`, 240 x 240, colour order RGB, inversion on, 40 MHz SPI.

## Hardware assumptions

* ESP32-S3 with 16 MB flash (the vendor binaries are ~9.6 MB, so 8 MB is not
  enough). Change `board_build.flash_size` / `board_upload.flash_size` and the
  partition table if your board differs.
* 4 MB partition for the application; the image currently uses ~1.84 MB
  because all icons, fonts and GIF frames are compiled into flash.
* PSRAM is not used by this sketch.

## Configuration

Everything a user tunes lives in `config.h.example` in the project root, right
next to `platformio.ini`. Copy it once and edit the copy:

```bash
cp config.h.example config.h
```

`config.h` is git-ignored, `config.h.example` is the tracked template. The sketch
finds it through the `-I.` include path and falls back to the vendor defaults
when it does not exist, so it compiles either way.

| Macro | Purpose |
| --- | --- |
| `WIFI_SSID`, `WIFI_PASSWORD` | Network the clock joins, 2.4 GHz only |
| `WEATHER_API_KEY` | OpenWeatherMap key, the vendor default is shared and rate limited |
| `CITY_AUTO_DETECT` | `1` resolves the city through `ip-api.com`, `0` uses `CITY_CODE` |
| `CITY_CODE` | City name for OpenWeatherMap when `CITY_AUTO_DETECT` is `0` |
| `NTP_SERVER`, `NTP_SYNC_MINUTES` | Time source and resync interval |
| `TIMEZONE_SOURCE` | `0` takes the offset from OpenWeatherMap, `1` uses the fixed zone below |
| `TIMEZONE_STD_OFFSET_HOURS`, `TIMEZONE_EU_DST` | Winter offset in hours and European summer time, Germany `1` / `1` |
| `FOLLOWER_TEXT` | Text in the top-right corner |

The real configuration is private by design: `opencode.jsonc` denies agents any
read or edit access to `config.h`.

## Troubleshooting: backlight on but the screen stays black

The backlight of this board is active low, and the sketch drives GPIO42 low in
the first two lines of `setup()`, so a lit backlight only proves that the
firmware booted - not that the panel receives data.

`tools/display_test` is a standalone project that initialises nothing but the
panel, prints every step on Serial and cycles through red/green/blue/white:

```bash
pio run -d tools/display_test -t upload
pio device monitor -d tools/display_test      # 115200 baud
```

What the result means:

| Result | Meaning |
| --- | --- |
| Colours appear, `tft.init()` logged | Panel, wiring and TFT_eSPI are fine, the problem is inside the sketch |
| Serial output but no colours | SPI/panel init problem, check the panel defines in `build_flags` |
| No serial output at all | Firmware did not start, or the partition table / flash layout is wrong |

Notes:

* This board has no USB-UART bridge; `D-`/`D+` are wired to GPIO19/GPIO20.
  Both environments therefore build with `-DARDUINO_USB_CDC_ON_BOOT=1`,
  otherwise `Serial` is routed to UART0 (GPIO43/GPIO44) and nothing shows up in
  the monitor.
* `tkr()` used to run before `tft.begin()`, so LVGL flushed its first frame
  into an uninitialised SPI bus. The call order in `setup()` was changed so the
  TFT is initialised first.

### Root cause of the black screen: TFT_eSPI 2.5.0 vs ESP-IDF 4.4 on the S3

`TFT_eSPI::init()` panicked with `StoreProhibited`, `EXCVADDR = 0x00000010`:

```
PC : 0x42002972  -> TFT_eSPI::begin_tft_write()
                    -> TFT_eSPI::init()
```

ESP-IDF 4.4 addresses the SPI peripherals with

```c
#define REG_SPI_BASE(i) (((i)>=2) ? (DR_REG_SPI2_BASE + (i-2) * 0x1000) : (0))
```

but `TFT_eSPI` used `SPI_PORT FSPI` for the S3, and `FSPI == 1`. The register
base therefore evaluated to `0`, and the first write to `SPI_USER_REG(SPI_PORT)`
(= offset 0x10) faulted. The panel never received a single command, the app
rebooted in a loop and only the backlight stayed lit.

Fix (one line in the S3 branch of the processor header):

```c
#elif CONFIG_IDF_TARGET_ESP32S3
  #define SPI_PORT 2 // FSPI == SPI2; IDF's REG_SPI_BASE() expects 2 here
```

Release 2.5.43 still ships `FSPI` there, so `platformio.ini` pins the upstream
commit that contains the fix instead of the release. The vendor copy this
project started from carried the same one-line patch.

## Credits and license

Hardware, original firmware, sketch, fonts, icons and the city data come from
Spotpear:

* Product and wiki page: <https://spotpear.com/wiki/ESP32-S3-N16R8-AI-DeepSeek-XiaoZhi-XiaGe-Qwen-DouBao-1.28-inch-Round-LCD-BOX-TouchScreen.html>
* Vendor source package: <https://github.com/SpotlightOn/ESP32S3-S3-1.28-Weather-Clock>

That package ships **no license file** for its own code and assets, so no license
is granted and all rights stay with Spotpear. This repository is a personal build
setup of it and does not relicense anything: keep the attribution, run it on your
own clock, and ask Spotpear for written permission before publishing or selling a
firmware derived from it.

The libraries keep their own licenses (TFT_eSPI, lvgl, AnimatedGIF, ArduinoJson,
TJpg_Decoder, Time), see the projects the PlatformIO packages come from. The
build system, the `config.h` template, the English translation and this
documentation are offered under the same terms as the vendor package.


