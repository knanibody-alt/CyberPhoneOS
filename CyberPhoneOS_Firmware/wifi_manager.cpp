#include "wifi_manager.h"
#include "config.h"
#include "sd_manager.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;
static volatile WifiState s_state = WIFI_STATE_IDLE;
static unsigned long s_connectStarted = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 15000;
static bool s_scanRequested = false;

void wifi_manager_init() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  prefs.begin("cyberphone", false); // namespace "cyberphone", read/write
}

void wifi_manager_start_scan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true /*async*/, false /*show_hidden*/);
  s_scanRequested = true;
}

bool wifi_manager_scan_done(int &count) {
  if (!s_scanRequested) return false;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  // WIFI_SCAN_FAILED (-1) is also "finished" - report zero networks so the
  // caller stops waiting instead of blocking until its own timeout expires.
  count = (n == WIFI_SCAN_FAILED) ? 0 : n;
  s_scanRequested = false;
  return true;
}

String wifi_manager_scan_ssid(int i)      { return WiFi.SSID(i); }
int32_t wifi_manager_scan_rssi(int i)     { return WiFi.RSSI(i); }
int32_t wifi_manager_scan_channel(int i)  { return WiFi.channel(i); }
bool wifi_manager_scan_secure(int i)      { return WiFi.encryptionType(i) != WIFI_AUTH_OPEN; }

void wifi_manager_connect(const char *ssid, const char *password) {
  WiFi.disconnect();
  delay(50);
  WiFi.begin(ssid, password);
  s_state = WIFI_STATE_CONNECTING;
  s_connectStarted = millis();
}

WifiState wifi_manager_state() {
  if (s_state == WIFI_STATE_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      s_state = WIFI_STATE_CONNECTED;

      // Persist credentials: NVS is the authoritative store (survives
      // reboots reliably), SD config.json is mirrored for the "Settings"
      // screen and for the user's own inspection / backups.
      prefs.putString("ssid", WiFi.SSID());
      prefs.putString("pass", WiFi.psk());

      DeviceConfig cfg;
      sd_load_config(cfg); // keep any other existing fields
      strlcpy(cfg.last_ssid, WiFi.SSID().c_str(), sizeof(cfg.last_ssid));
      strlcpy(cfg.last_pass, WiFi.psk().c_str(), sizeof(cfg.last_pass));
      sd_save_config(cfg);

      // Status bar clock: ESP32 has no battery-backed RTC, so we sync via
      // NTP whenever we get an internet-capable connection. The timezone
      // lives in config.h (NTP_GMT_OFFSET_SEC).
      configTime(NTP_GMT_OFFSET_SEC, 0, NTP_SERVER_1, NTP_SERVER_2);
      Serial.printf("[WiFi] Baglandi, NTP senkronlandi (UTC%+d).\n", NTP_GMT_OFFSET_SEC / 3600);

    } else if (millis() - s_connectStarted > CONNECT_TIMEOUT_MS) {
      s_state = WIFI_STATE_FAILED;
      WiFi.disconnect();
    }
  }
  return s_state;
}

// Read-only snapshot of the connection state machine. Unlike
// wifi_manager_state() this never advances the machine, so it is safe to call
// from the UI core (Core 1) while the network task drives it on Core 0.
WifiState wifi_manager_current_state() {
  return s_state;
}

bool wifi_manager_autoconnect() {
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  if (ssid.length() == 0) return false;
  wifi_manager_connect(ssid.c_str(), pass.c_str());
  return true;
}

int wifi_manager_signal_bars() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  long rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

bool wifi_manager_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}
