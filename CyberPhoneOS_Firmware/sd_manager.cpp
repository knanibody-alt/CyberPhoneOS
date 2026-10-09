#include "sd_manager.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

static bool s_present = false;

static void ensure_dir(const char *path) {
  if (!SD.exists(path)) {
    SD.mkdir(path);
  }
}

bool sd_init() {
  // The SD card sits on the same physical SPI bus as the TFT/touch
  // (SCLK=18, MISO=19, MOSI=23) but with its own CS line (GPIO12).
  // TFT_eSPI already called SPI.begin() with these pins during tft.init(),
  // so we simply reuse the default SPI bus here and only pass our CS pin.
  s_present = SD.begin(PIN_SD_CS, SPI, 20000000);
  if (!s_present) {
    Serial.println("[SD] Card mount failed - check wiring / formatting (FAT32).");
    return false;
  }

  ensure_dir(SD_DIR_SYSTEM);
  ensure_dir(SD_DIR_WWW);
  ensure_dir(SD_DIR_GAMES);
  ensure_dir(SD_DIR_LOGS);

  Serial.println("[SD] Card mounted OK.");
  return true;
}

bool sd_is_present() {
  // Cheap liveness check: re-reading card size is enough to detect a card
  // that was physically removed without doing a full re-mount.
  return s_present && (SD.cardSize() > 0);
}

bool sd_load_config(DeviceConfig &cfg) {
  if (!s_present || !SD.exists(SD_CONFIG_JSON)) return false;

  File f = SD.open(SD_CONFIG_JSON, FILE_READ);
  if (!f) return false;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[SD] config.json parse error: %s\n", err.c_str());
    return false;
  }

  cfg.brightness = doc["brightness"] | cfg.brightness;
  strlcpy(cfg.last_ssid, doc["last_ssid"] | "", sizeof(cfg.last_ssid));
  strlcpy(cfg.last_pass, doc["last_pass"] | "", sizeof(cfg.last_pass));
  strlcpy(cfg.chat_device_name, doc["chat_device_name"] | "CyberPhone", sizeof(cfg.chat_device_name));

  JsonArray cal = doc["touch_cal"];
  if (!cal.isNull() && cal.size() == 5) {
    for (int i = 0; i < 5; i++) cfg.touch_cal[i] = cal[i] | 0;
  }
  return true;
}

bool sd_save_config(const DeviceConfig &cfg) {
  if (!s_present) return false;

  StaticJsonDocument<512> doc;
  doc["brightness"] = cfg.brightness;
  doc["last_ssid"] = cfg.last_ssid;
  doc["last_pass"] = cfg.last_pass;
  doc["chat_device_name"] = cfg.chat_device_name;

  JsonArray cal = doc.createNestedArray("touch_cal");
  for (int i = 0; i < 5; i++) cal.add(cfg.touch_cal[i]);

  File f = SD.open(SD_CONFIG_JSON, FILE_WRITE);
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void sd_log_wifi_scan_row(const String &row) {
  if (!s_present) return;
  bool isNew = !SD.exists(SD_WIFI_LOG_CSV);
  File f = SD.open(SD_WIFI_LOG_CSV, FILE_APPEND);
  if (!f) return;
  if (isNew) f.println("timestamp_ms,ssid,rssi,channel");
  f.println(row);
  f.close();
}
