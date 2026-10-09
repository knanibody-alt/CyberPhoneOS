/**
 * CyberPhoneOS
 * ESP32 + 2.8" ILI9341/XPT2046 touch TFT + SD card, LVGL-based phone-like OS.
 *
 * ARCHITECTURE
 *   Core 1 (Arduino's default loop() core): LVGL rendering + touch + games.
 *   Core 0 (a dedicated FreeRTOS task started below): Wi-Fi, the local chat
 *   AP + WebSocket server, Wi-Fi channel analysis, and BLE scanning.
 *   The two cores only ever talk through the FreeRTOS queues declared in
 *   shared_types.h - nothing else is shared between them.
 *
 * REQUIRED LIBRARIES (Arduino Library Manager unless noted):
 *   - TFT_eSPI            by Bodmer         (needs the custom User_Setup.h from this project)
 *   - lvgl                by kisvegabor     v8.3.x (needs the custom lv_conf.h from this project)
 *   - ESPAsyncWebServer    by me-no-dev / ESP32Async
 *   - AsyncTCP             by me-no-dev / ESP32Async   (ESPAsyncWebServer's dependency)
 *   - ArduinoJson          by Benoit Blanchon, v6.x
 *   - SD, WiFi, Preferences, BLEDevice: bundled with the ESP32 Arduino core (no install needed)
 *
 * BOARD: "ESP32 Dev Module" (or your specific ESP32 board) in Tools > Board.
 *
 * See README.md for the full wiring recap and SD card folder layout.
 */

#include "config.h"
#include "shared_types.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "wifi_analyzer.h"
#include "ble_scanner.h"
#include "chat_server.h"
#include "ui_manager.h"

QueueHandle_t g_chatInQueue;
QueueHandle_t g_netCmdQueue;
QueueHandle_t g_wifiListQueue;
QueueHandle_t g_wifiScanQueue;
QueueHandle_t g_bleScanQueue;

static TaskHandle_t s_networkTaskHandle;

// =====================================================================================
//  Core 0: network task
// =====================================================================================

static void handle_net_command(const NetCommand &cmd) {
  switch (cmd.type) {
    case NET_CMD_START_CHAT_AP:
      chat_server_start(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
      break;

    case NET_CMD_STOP_CHAT_AP:
      chat_server_stop();
      break;

    case NET_CMD_SEND_CHAT:
      chat_server_send_from_device("Ben", cmd.strA);
      break;

    case NET_CMD_WIFI_CONNECT:
      wifi_manager_connect(cmd.strA, cmd.strB);
      break;

    case NET_CMD_RUN_WIFI_SCAN: {
      wifi_manager_start_scan();
      int count = 0;
      unsigned long start = millis();
      while (!wifi_manager_scan_done(count) && millis() - start < 8000) {
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      NetworkListResult result{};
      result.count = min(count, NET_LIST_MAX);
      for (int i = 0; i < result.count; i++) {
        strlcpy(result.nets[i].ssid, wifi_manager_scan_ssid(i).c_str(), sizeof(result.nets[i].ssid));
        result.nets[i].rssi = wifi_manager_scan_rssi(i);
        result.nets[i].channel = wifi_manager_scan_channel(i);
        result.nets[i].secure = wifi_manager_scan_secure(i);
      }
      xQueueOverwrite(g_wifiListQueue, &result);
      break;
    }

    case NET_CMD_RUN_WIFI_ANALYZER: {
      WifiScanResult result;
      wifi_analyzer_run_once(result);
      xQueueOverwrite(g_wifiScanQueue, &result);
      break;
    }

    case NET_CMD_RUN_BLE_SCAN: {
      BleScanResult result;
      ble_scanner_run_once(result, 4);
      xQueueOverwrite(g_bleScanQueue, &result);
      break;
    }
  }
}

static void network_task(void *param) {
  wifi_manager_init();

  // Try to rejoin whatever network was last saved on the SD card / NVS.
  wifi_manager_autoconnect();

  NetCommand cmd;
  for (;;) {
    if (xQueueReceive(g_netCmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      handle_net_command(cmd);
    }

    // Poll the (non-blocking) station connection state machine so
    // wifi_manager_state() can flip from CONNECTING -> CONNECTED/FAILED
    // even when no command is currently pending.
    wifi_manager_state();

    chat_server_loop();
  }
}

// =====================================================================================
//  setup() / loop() both execute on Core 1 by default in the Arduino core.
// =====================================================================================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[CyberPhoneOS] Booting...");

  randomSeed(esp_random());

  g_chatInQueue   = xQueueCreate(CHAT_QUEUE_LEN, sizeof(ChatMessage));
  g_netCmdQueue   = xQueueCreate(4, sizeof(NetCommand));
  g_wifiListQueue = xQueueCreate(1, sizeof(NetworkListResult));
  g_wifiScanQueue = xQueueCreate(1, sizeof(WifiScanResult));
  g_bleScanQueue  = xQueueCreate(1, sizeof(BleScanResult));

  // ui_init() brings up the TFT (which configures the shared SPI bus) and
  // then mounts the SD card itself - see ui_manager.cpp for why the
  // ordering matters. Both cores call the sd_manager functions afterwards;
  // SD.h isn't re-entrant across cores, but each side only touches it in
  // short, queue-serialized bursts so this stays safe in practice.
  ui_init();

  xTaskCreatePinnedToCore(
      network_task,
      "network_task",
      NETWORK_TASK_STACK,
      NULL,
      NETWORK_TASK_PRIO,
      &s_networkTaskHandle,
      NETWORK_TASK_CORE);

  Serial.println("[CyberPhoneOS] Hazir.");
}

void loop() {
  ui_handler();
  delay(5); // yield so LVGL's tick and the idle task both get scheduled
}
