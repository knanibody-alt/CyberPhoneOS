#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "wifi_analyzer.h"
#include "ble_scanner.h"

// ---- Messages passed between Core 0 (network) and Core 1 (UI/LVGL) ----

struct ChatMessage {
  char sender[16];
  char text[144];
};

// Commands the UI (Core 1) sends to the network task (Core 0).
enum NetCommandType {
  NET_CMD_START_CHAT_AP,
  NET_CMD_STOP_CHAT_AP,
  NET_CMD_SEND_CHAT,
  NET_CMD_WIFI_CONNECT,
  NET_CMD_RUN_WIFI_SCAN,     // for the Settings network list
  NET_CMD_RUN_WIFI_ANALYZER, // for the analyzer bar chart
  NET_CMD_RUN_BLE_SCAN
};

struct NetCommand {
  NetCommandType type;
  char strA[64]; // ssid / chat text / sender name depending on type
  char strB[64]; // password, when type == NET_CMD_WIFI_CONNECT
};

// Plain network list for the Settings screen (pick-a-network-to-join).
#define NET_LIST_MAX 15
struct NetworkInfo {
  char ssid[33];
  int  rssi;
  int  channel;
  bool secure;
};
struct NetworkListResult {
  NetworkInfo nets[NET_LIST_MAX];
  int count;
};

// Declared in CyberPhoneOS_Firmware.ino, used by both ui_manager.cpp and
// chat_server.cpp / network task.
extern QueueHandle_t g_chatInQueue;    // network -> UI  (ChatMessage), depth CHAT_QUEUE_LEN
extern QueueHandle_t g_netCmdQueue;    // UI -> network  (NetCommand), depth 4
extern QueueHandle_t g_wifiListQueue;  // network -> UI  (NetworkListResult), depth 1 (xQueueOverwrite)
extern QueueHandle_t g_wifiScanQueue;  // network -> UI  (WifiScanResult), depth 1 (xQueueOverwrite)
extern QueueHandle_t g_bleScanQueue;   // network -> UI  (BleScanResult), depth 1 (xQueueOverwrite)

#endif // SHARED_TYPES_H
