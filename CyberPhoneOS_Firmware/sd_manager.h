#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>

struct DeviceConfig {
  int   brightness      = 200;     // 0-255
  int   touch_cal[5]    = {0,0,0,0,0}; // reserved for touch calibration data
  char  last_ssid[33]   = "";
  char  last_pass[65]   = "";
  char  chat_device_name[24] = "CyberPhone";
};

// Mounts the SD card and verifies /system, /www, /games, /logs exist
// (creates any that are missing). Returns true on success.
bool sd_init();

// True if the card is currently readable (used by the status bar icon).
bool sd_is_present();

// Loads /system/config.json into `cfg`. Leaves defaults untouched if the
// file is missing or malformed.
bool sd_load_config(DeviceConfig &cfg);

// Serializes `cfg` back to /system/config.json.
bool sd_save_config(const DeviceConfig &cfg);

// Appends one row ("timestamp,ssid,rssi,channel") to /logs/wifi_scan.csv
void sd_log_wifi_scan_row(const String &row);

#endif // SD_MANAGER_H
