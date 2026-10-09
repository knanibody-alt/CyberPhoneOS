#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================================
//  CyberPhoneOS - PIN MAP
//  Derived from the wiring table: shared VSPI bus (SCK/MOSI/MISO) across
//  TFT, XPT2046 touch controller and SD card. Only the CS lines are unique.
// =====================================================================================

// ---- Shared VSPI bus (TFT + Touch + SD all sit on this bus) ----
#define PIN_SCLK      18   // SD_SCK / T_CLK / TFT SCLK
#define PIN_MOSI      23   // SD_MOSI / T_DI / TFT MOSI
#define PIN_MISO      19   // SD_MISO / T_DO / TFT MISO

// ---- TFT (ILI9341 2.8") ----
#define PIN_TFT_CS    17
#define PIN_TFT_DC    16
#define PIN_TFT_RST    5
#define PIN_TFT_BL    32   // backlight / "LED" pin, PWM capable

// ---- XPT2046 touch controller ----
#define PIN_TOUCH_CS  21
// T_IRQ is left unconnected on this board, so we poll the touch controller
// instead of using an interrupt (handled in ui_manager.cpp).

// ---- SD Card ----
#define PIN_SD_CS     12

// ---- Display geometry ----
#define TFT_HOR_RES   320
#define TFT_VER_RES   240
#define TFT_ROTATION  1     // landscape, USB/SD slot to the left - adjust 0-3 to taste

// ---- LVGL draw buffer ----
// One horizontal-band buffer, ~1/8th of the screen. Plenty for LVGL v8 on ESP32.
#define LVGL_BUF_LINES 40

// ---- Device identity / defaults ----
#define DEVICE_NAME        "CyberPhoneOS"
#define DEFAULT_AP_SSID     "CyberPhone-Chat"
#define DEFAULT_AP_PASS     "cyberphone123"   // must be >= 8 chars for WPA2
#define AP_CHAT_PORT        80

// ---- Clock ----
// The ESP32 has no battery-backed RTC, so the status-bar clock is only
// accurate after NTP sync (which needs an internet-capable Wi-Fi connection).
// Until then the status bar shows uptime. Adjust the offset to your timezone:
//   UTC+3 (Turkey) = 3 * 3600, UTC+0 (London) = 0, UTC-5 (New York) = -5 * 3600.
#define NTP_GMT_OFFSET_SEC (3 * 3600)
#define NTP_SERVER_1       "pool.ntp.org"
#define NTP_SERVER_2       "time.google.com"

// ---- FreeRTOS task / core layout ----
// Core 1 (APP_CPU on Arduino terms is actually core 1; Arduino loop() already
// runs pinned to core 1) -> LVGL + touch + games.
// Core 0                  -> Wi-Fi, web server, WebSocket, Wi-Fi analyzer scans.
#define NETWORK_TASK_CORE   0
#define NETWORK_TASK_STACK  8192
#define NETWORK_TASK_PRIO   1

// ---- Inter-core queues sizes ----
#define CHAT_QUEUE_LEN      16
#define WIFI_SCAN_QUEUE_LEN 2

// ---- SD card folder layout (see README.md for the full tree) ----
#define SD_DIR_SYSTEM   "/system"
#define SD_DIR_WWW      "/www"
#define SD_DIR_GAMES    "/games"
#define SD_DIR_LOGS     "/logs"
#define SD_CONFIG_JSON  "/system/config.json"
#define SD_WIFI_LOG_CSV "/logs/wifi_scan.csv"

#endif // CONFIG_H
