<div align="center">

# CyberPhoneOS

**A pocket-sized, phone-like operating system for an ESP32 + 2.8" TFT handheld.**

LVGL touch UI · local Wi-Fi chat server · passive Wi-Fi channel analyzer ·
passive BLE scanner · SD-card game loader with a working Snake demo

[![License: MIT](https://img.shields.io/badge/License-MIT-7fd97a.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20Arduino-59c1ff.svg)](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
[![LVGL](https://img.shields.io/badge/LVGL-v8.3-d9a057.svg)](https://docs.lvgl.io/8.3/)

</div>

---

## Table of Contents

- [What is this?](#what-is-this)
- [A note on the original "BLE spam" brief](#a-note-on-the-original-ble-spam-brief)
- [Features](#features)
- [Hardware](#hardware)
  - [Wiring](#wiring)
  - [Pin map](#pin-map)
- [Getting started](#getting-started)
  - [1. Required libraries](#1-required-libraries)
  - [2. Install the two override headers](#2-install-the-two-override-headers)
  - [3. Open and build the sketch](#3-open-and-build-the-sketch)
  - [4. Prepare the SD card](#4-prepare-the-sd-card)
  - [5. Flash and first boot](#5-flash-and-first-boot)
- [SD card layout](#sd-card-layout)
- [Architecture](#architecture)
- [Source layout](#source-layout)
- [Configuration](#configuration)
- [Extending it](#extending-it)
- [Troubleshooting](#troubleshooting)
- [Known limitations](#known-limitations)
- [Safety & scope](#safety--scope)
- [License](#license)

---

## What is this?

CyberPhoneOS turns an ESP32 and a cheap 2.8" SPI TFT module into a small
touchscreen device that behaves like a phone rather than a serial-port
dashboard:

- A **home launcher** with app icons, a status bar (Wi-Fi, SD, clock) and a
  back/home nav bar.
- A **local chat** app. The ESP32 raises a Wi-Fi Access Point; any phone that
  joins it gets a web chat page served straight off the SD card, mirrored
  live onto the touchscreen.
- A **Wi-Fi analyzer** that passively tallies how congested each 2.4 GHz
  channel is and lists the strongest nearby access points.
- A **BLE scanner** that passively lists nearby Bluetooth devices and their
  signal strength.
- A **game loader** that scans `/games` on the SD card at runtime, so new
  entries show up in the launcher without reflashing. A complete Snake game
  is included as a working reference engine.

Everything is passive except the chat AP, which only exists on a network the
user explicitly starts.

## A note on the original "BLE spam" brief

The original project brief asked for a **"BLE spam"** mode that would broadcast
fake advertisement packets (fake Android crash dialogs, fake Apple pairing
popups, fake Windows "Pair" prompts) to flood phones belonging to other people.

**That is not implemented, deliberately.** It is a denial-of-service attack
against devices you do not own, it is a crime in most jurisdictions, and it is
the kind of thing that gets hardware confiscated.

The menu slot is filled by a **passive BLE scanner** instead — the same thing
nRF Connect or a Bluetooth site-survey app does. It listens to ordinary
public advertisement packets and shows you device names, MAC addresses and
RSSI. It never transmits.

The Wi-Fi analyzer is likewise strictly passive: beacon frames only, no
deauthentication, no probe injection, no association attempts.

## Features

| Area | Detail |
|---|---|
| UI | LVGL v8.3, 320×240 landscape, single-band draw buffer (40 lines) |
| Input | XPT2046 resistive touch, polled (no `T_IRQ` needed), one-time calibration stored on the SD card |
| Storage | SD card over the shared SPI bus, FAT32, auto-created folder tree |
| Networking | Station mode (join a network) + AP mode (chat), both from the UI |
| Chat | Async WebSocket server, static hosting from `SD:/www`, on-screen QWERTY keyboard |
| Diagnostics | Passive Wi-Fi channel congestion chart + CSV export, passive BLE survey |
| Games | Runtime `/games` folder scan, per-game high scores, working Snake engine |
| Concurrency | Dual-core: UI on Core 1, all networking on Core 0, queues in between |

## Hardware

Tested combination:

- **ESP32 Dev Module** (any 30-pin / 38-pin ESP32 DevKitC works)
- **2.8" 240×320 SPI TFT**, ILI9341 driver, **XPT2046** resistive touch
  (the common "ILI9341 + XPT2046" module where touch shares the TFT's SPI bus)
- **MicroSD card module**, SPI mode, FAT32
- Optional: a LiPo cell + TP4056 charger + voltage divider on a spare ADC pin

### Wiring

The TFT, the touch controller and the SD card **share one VSPI bus**. Only the
three chip-select lines are unique.

**Shared bus (tie these together):**

| Signal | ESP32 GPIO | Connects to |
|---|---|---|
| SCLK | **GPIO18** | TFT `CLK`, Touch `T_CLK`, SD `SCK` |
| MOSI | **GPIO23** | TFT `DI`, Touch `T_DI`, SD `MOSI` |
| MISO | **GPIO19** | TFT `DO`, Touch `T_DO`, SD `MISO` |

**Unique lines:**

| Function | ESP32 GPIO | Connects to |
|---|---|---|
| TFT chip select | **GPIO17** | TFT `CS` |
| TFT data/command | **GPIO16** | TFT `DC` |
| TFT reset | **GPIO5** | TFT `RST` |
| TFT backlight | **GPIO32** | TFT `LED` / `BL` |
| Touch chip select | **GPIO21** | Touch `T_CS` |
| SD chip select | **GPIO12** | SD `CS` |

Notes:

- `T_IRQ` is **not connected**. Touch is polled every LVGL tick instead, which
  is what LVGL's `read_cb` expects anyway.
- Power the SD card and the backlight from **3V3**. Running the SD module off
  5V while its logic lines go to 3.3 V ESP32 pins is the single most common
  cause of "card mount failed".
- GPIO12 is a **boot-strapping pin** (MTDI). It must be low at reset or the
  ESP32 boots at 1.25 V instead of 3.3 V. Most SD modules pull it through a
  10 kΩ pull-down, which is correct — just don't re-purpose GPIO12 elsewhere.

## Getting started

### 1. Required libraries

Install these via **Arduino IDE → Library Manager** (or `arduino-cli lib install`):

| Library | Author | Version | Needed for |
|---|---|---|---|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | latest | Display + touch driver |
| [lvgl](https://github.com/lvgl/lvgl) | kisvegabor / LVGL | **v8.3.x** | UI toolkit |
| [ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer) | ESP32Async | latest | Chat HTTP + WebSocket server |
| [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) | ESP32Async | latest | ESPAsyncWebServer dependency |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Benoit Blanchon | **v6.x** | JSON config + chat protocol |

`SD.h`, `SPI.h`, `WiFi.h`, `Preferences.h` and `BLEDevice.h` ship with the
ESP32 Arduino core — install nothing extra.

> **LVGL v9 is not compatible with this code.** The v9 API changed
> (`lv_disp_draw_buf_t` → `lv_display_t`, etc). Pin the library to `8.3.11` or
> similar.

### 2. Install the two override headers

This project ships pre-tuned versions of two headers that libraries read from
*their own* folders, so they must be copied into place manually:

1. **`lv_conf.h`** → your Arduino *sketchbook* root, **next to** the `lvgl`
   folder — **not inside it**.

   ```
   ~/Arduino/libraries/lv_conf.h          <-- here
   ~/Arduino/libraries/lvgl/              <-- library itself
   ```

   On Windows: `Documents\Arduino\libraries\lv_conf.h`.

2. **`User_Setup.h`** → overwrite the one inside the TFT_eSPI library:

   ```
   ~/Arduino/libraries/TFT_eSPI/User_Setup.h
   ```

   The stock file is backed up as `User_Setup_Select.h.original`, so you can
   restore it if you use TFT_eSPI for other projects.

3. Open `~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h` and confirm that
   **only** this line is active:

   ```c
   #include <User_Setup.h>
   ```

   Comment out the bundled PCB-specific defaults (`#include<User_Setup_ILI9341.h>`
   and friends) if any of them are enabled.

> Both files live in this repo under `CyberPhoneOS_Firmware/` so you always
> have a reference copy. The pins in `User_Setup.h` and `config.h` are two
> views of the same map — **change both** if you rewire.

### 3. Open and build the sketch

1. Open `CyberPhoneOS_Firmware/CyberPhoneOS_Firmware.ino` in the Arduino IDE
   (opening the `.ino` pulls in every `.cpp`/`.h` in the same folder).
2. **Tools → Board → ESP32 Arduino → ESP32 Dev Module**.
3. Set the partition scheme to *Default 4MB with spiffs* (or anything with at
   least 1.2 MB app space — LVGL is big).
4. Build and upload.

### 4. Prepare the SD card

1. Format a microSD card as **FAT32** (exFAT also works with the ESP32 SD
   library, but FAT32 is the safest default).
2. Copy the **contents** of `sdcard_contents/` — the `system/`, `www/`,
   `games/` and `logs/` folders, **not** the `sdcard_contents` folder itself —
   into the **root** of the card.

   ```
   (SD card root)
   ├── system/config.json
   ├── www/{index.html,style.css,script.js}
   ├── games/{snake,flappy}/
   └── logs/
   ```

The firmware also calls `SD.mkdir()` for any of those four folders that are
missing, so an empty card still boots — you just have to add `www/` yourself
for the chat page.

### 5. Flash and first boot

1. Insert the card, then upload.
2. **On first boot the screen asks you to tap the corners** to calibrate touch.
   This writes `touch_cal` into `/system/config.json`. It happens once.
   To recalibrate later, delete `/system/config.json` (or just the `touch_cal`
   array) and reboot — the wizard runs again.
3. Open **Settings** → *Ağ Tara* to scan for networks, pick one, type the
   password with the on-screen keyboard, tap **Bağlan**.
4. Open **Sohbet** and type something — the AP starts automatically on the
   first message. Join `CyberPhone-Chat` / `cyberphone123` from a phone and
   browse to `http://192.168.4.1`.

> The AP password is a **default, not a secret**. Change
> `DEFAULT_AP_SSID` / `DEFAULT_AP_PASS` in `config.h` before putting the device
> anywhere shared.

## SD card layout

```
/                            <- SD card root
├── system/
│   └── config.json          brightness, last Wi-Fi credentials,
│                            chat device name, touch calibration
├── www/
│   ├── index.html           chat page (served at http://192.168.4.1)
│   ├── style.css
│   └── script.js            WebSocket client
├── games/
│   ├── snake/
│   │   ├── highscore.txt    read/written automatically
│   │   └── config.txt       reserved: speed, wall rules, etc.
│   └── flappy/
│       ├── highscore.txt
│       └── README.txt       how to turn this stub into a real game
└── logs/
    └── wifi_scan.csv        appended to on every analyzer run
```

To add a game, create a new folder under `/games`. It appears in the **Games**
screen automatically (`game_loader.cpp` → `game_loader_scan()`). Only `snake`
has a built-in engine today; other folders are listed and show a "engine not
written yet" message when tapped. See `games/flappy/README.txt` for a
walkthrough.

## Architecture

Two FreeRTOS cores, one direction of ownership, no shared mutable globals:

```
┌───────────────────── Core 1 (Arduino loop) ──────────────────────┐
│  LVGL render · XPT2046 touch polling · game logic                 │
│  ui_manager.cpp                                                 │
└───────────────┬──────────────────────────────▲──────────────────┘
                │ NetCommand                   │ results / chat
                │ (g_netCmdQueue, depth 4)     │ (3 × depth-1 overwrite queues)
┌───────────────▼──────────────────────────────┴──────────────────┐
│                  Core 0 (network_task, prio 1)                  │
│  Wi-Fi station state machine · chat AP + WebSocket + static HTTP  │
│  Wi-Fi channel analyzer · BLE scanner                           │
│  wifi_manager · chat_server · wifi_analyzer · ble_scanner        │
└────────────────────────────────────────────────────────────────┘
```

Rules this codebase sticks to:

- **All** network I/O lives on Core 0. `wifi_analyzer_run_once()` and
  `ble_scanner_run_once()` block for seconds — they must never run on the UI core.
- The two cores communicate **only** through the FreeRTOS queues declared in
  `shared_types.h`. Nothing else is written from both sides.
- Result queues are depth 1 and written with `xQueueOverwrite()`, so the UI
  always sees the *latest* scan rather than a backlog of stale ones.
- Functions that mutate state owned by Core 0 (e.g. `wifi_manager_state()`)
  are Core-0-only. Pure readers (e.g. `wifi_manager_current_state()`) are
  explicitly marked safe from anywhere.

> **Known wrinkle:** `SD.h` is not re-entrant across cores. Both cores do touch
> it — the network core appends Wi-Fi CSV rows, the UI core reads configs and
> high scores. Each side does so in short, queue-serialized bursts, which works
> in practice but is the first thing to break if you add long-running SD work.

## Source layout

| File | Responsibility |
|---|---|
| `CyberPhoneOS_Firmware.ino` | `setup()` / `loop()`, queue creation, the Core-0 network task |
| `config.h` | Pin map, geometry, AP defaults, NTP timezone, SD paths |
| `shared_types.h` | Every message struct and the inter-core queue declarations |
| `ui_manager.cpp` | TFT + LVGL glue, status bar, nav bar, keyboard, and every screen |
| `wifi_manager.cpp` | Station state machine, async scans, NVS credential storage, NTP |
| `wifi_analyzer.cpp` | Passive channel census + strongest-AP list + CSV logging |
| `ble_scanner.cpp` | Passive BLE advertisement survey |
| `chat_server.cpp` | AP bring-up, WebSocket protocol, static hosting from `SD:/www` |
| `sd_manager.cpp` | SD mount, folder bootstrap, `config.json` load/save, CSV append |
| `game_loader.cpp` | `/games` folder scan and high-score persistence |
| `lv_conf.h` | LVGL build configuration (copy to sketchbook root) |
| `User_Setup.h` | TFT_eSPI pin/strobe configuration (copy into the library) |

## Configuration

Everything you are likely to change lives in **`config.h`**:

| Macro | Default | Meaning |
|---|---|---|
| `PIN_SCLK` / `PIN_MOSI` / `PIN_MISO` | 18 / 23 / 19 | Shared VSPI bus |
| `PIN_TFT_CS` / `PIN_TFT_DC` / `PIN_TFT_RST` | 17 / 16 / 5 | TFT control lines |
| `PIN_TFT_BL` | 32 | Backlight |
| `PIN_TOUCH_CS` | 21 | XPT2046 chip select |
| `PIN_SD_CS` | 12 | SD chip select |
| `TFT_HOR_RES` / `TFT_VER_RES` | 320 / 240 | Panel geometry |
| `TFT_ROTATION` | 1 | `0`–`3`; try these if the image is sideways or mirrored |
| `LVGL_BUF_LINES` | 40 | Draw-buffer height. Lower = less RAM, more flicker |
| `DEFAULT_AP_SSID` / `DEFAULT_AP_PASS` | `CyberPhone-Chat` / `cyberphone123` | Chat AP credentials |
| `NTP_GMT_OFFSET_SEC` | `3 * 3600` | Timezone. `0` = UTC, `-5*3600` = New York |
| `NETWORK_TASK_STACK` / `NETWORK_TASK_PRIO` | 8192 / 1 | Core-0 task tuning |

Two more knobs worth knowing:

- **`LV_TICK_CUSTOM`** is enabled in `lv_conf.h` and ticks off `millis()`, so
  there is no separate LVGL timer task.
- **`LV_MEM_SIZE`** is 48 kB. Raise it if you add image widgets; the display
  buffer plus LVGL heap plus Wi-Fi/BT stacks are all competing for DRAM.

## Extending it

**Add a screen.** Append to the `AppScreen` enum, add a `build_xxx()` function,
add a case to `show_app()`, and add a launcher icon in `build_home()`.

**Add a network command.** Add a value to `NetCommandType`, send it with
`xQueueSend(g_netCmdQueue, ...)` from the UI, and handle it in
`handle_net_command()` on Core 0. If the answer comes back, add a depth-1
overwrite queue in `shared_types.h` and drain it in `ui_handler()`.

**Add a game.** Create `/games/<name>/highscore.txt` on the card, then add a
branch to `game_entry_cb()` in `ui_manager.cpp` and write a `build_<name>()`
engine. Assets must be LVGL `lv_img_dsc_t` C arrays — raw PNGs will not render.

**Dim the backlight.** `ui_init()` currently just does `digitalWrite(PIN_TFT_BL, HIGH)`
for cross-core-version compatibility. Swap it for `ledcAttachPin` + `ledcWrite`
(or `analogWrite` on core 2.x) and drive it from the `brightness` field that is
already being loaded out of `config.json`.

**Add a battery gauge.** Wire a voltage divider to a spare input-only ADC1 pin
(GPIO34–GPIO39 are input-only; avoid GPIO32–39 conflicting with the Wi-Fi
startup strapping), read it with `analogReadMilliVolts()` in
`status_bar_refresh()`, and render a battery glyph.

## Troubleshooting

**SD card mount fails at boot**
Format FAT32. Check `SCK`/`MOSI`/`MISO`/`CS` continuity. Confirm the module is
powered from 3V3, not 5V. Make sure GPIO12 idles low.

**Touch registers clicks in the wrong place**
Calibration is stored in `/system/config.json`. Delete that file and reboot to
run the wizard again. If taps are *mirrored*, try `TFT_ROTATION 3` instead of `1`.

**Screen is white / black / garbled**
Confirm `ILI9341_DRIVER` is set in your installed `User_Setup.h` and that
`User_Setup_Select.h` includes only `<User_Setup.h>`. If colors look
byte-swapped, set `LV_COLOR_16_SWAP 1` in your installed `lv_conf.h`.

**Build fails with hundreds of LVGL "implicit declaration" errors**
You have LVGL v9 installed, or your `lv_conf.h` is in the wrong folder. It must
be *next to* the `lvgl` folder, not inside it. Pin LVGL to 8.3.x.

**Wi-Fi shows as connected but the chat page will not load**
The AP and station modes coexist, but if the ESP32 has never joined an upstream
network you will still be able to reach `192.168.4.1`. Confirm your phone
joined `CyberPhone-Chat` and not a nearby network with a similar name.

**BLE scan returns nothing**
Active scanning (which asks for scan responses and therefore reveals names)
is what this build uses; a few devices simply refuse to respond. Devices with
no name show as `(isimsiz cihaz)`.

**`Wi-Fi` never appears on the status bar before NTP**
Expected. The status bar shows `up H:MM` until an internet-capable connection
has completed an NTP sync.

## Known limitations

- **No battery gauge.** The supplied pin map has no ADC input reserved for it.
  See *Extending it* above.
- **No real-time clock.** There is no battery-backed RTC on an ESP32, so the
  clock only becomes correct after NTP sync; before that the status bar shows
  uptime.
- **No internet in AP mode.** Phones on the chat AP can talk to each other and
  to the ESP32, but the ESP32 gives them no upstream route. This is why the web
  page pulls in zero external assets — it is served entirely from the SD card.
- **Only Snake runs.** Other `/games` folders are listed but have no engine.
- **Credentials are stored in plaintext** in both NVS and
  `/system/config.json`. Do not use a password you would not write on a sticky
  note.
- **Chat messages are dropped if you are not on the Chat screen.** The UI drains
  the incoming queue every loop iteration and only appends while
  `APP_CHAT` is the active screen. A backlog buffer would fix this.

## Safety & scope

- **Scanning is passive.** The Wi-Fi analyzer reads beacon frames. The BLE
  scanner only listens. Neither injects, deauthenticates, associates, or
  transmits crafted packets.
- **Use it on networks you own or are authorised to test.** Passive radio
  observation is legal almost everywhere; *active* probing is not, and the
  distinction matters.
- **Radio output is weak but not zero.** An always-on Wi-Fi AP and active BLE
  scanning are both detectable. Both are started explicitly by you, from the UI.
- **No credential harvesting, no packet capture, no firmware modification** of
  any other device.

## License

MIT — see [LICENSE](LICENSE).

Do whatever you want with it, but keep it off other people's networks unless
they asked you to.