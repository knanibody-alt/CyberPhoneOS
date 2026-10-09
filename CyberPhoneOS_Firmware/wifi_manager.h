#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

enum WifiState {
  WIFI_STATE_IDLE,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED,
  WIFI_STATE_FAILED
};

// Call once from the network task's setup phase.
void wifi_manager_init();

// Kicks off WiFi.scanNetworks(true) (async). Non-blocking.
void wifi_manager_start_scan();

// Returns true and fills `count` once an async scan started by
// wifi_manager_start_scan() has finished. Safe to poll every loop.
bool wifi_manager_scan_done(int &count);

// Accessors for scan result `i` (only valid after wifi_manager_scan_done()==true)
String wifi_manager_scan_ssid(int i);
int32_t wifi_manager_scan_rssi(int i);
int32_t wifi_manager_scan_channel(int i);
bool    wifi_manager_scan_secure(int i);

// Starts an async station connection attempt. Saves credentials to NVS
// (and mirrors them to /system/config.json) on success.
void wifi_manager_connect(const char *ssid, const char *password);

// Non-blocking state poll - call every loop from the network task.
// NOTE: this advances the state machine (it is not a pure getter), so it must
// only ever be called from the network core. Use wifi_manager_current_state()
// to read the state from anywhere.
WifiState wifi_manager_state();

// Side-effect-free snapshot of the current state, safe from any core.
WifiState wifi_manager_current_state();

// Tries to auto-connect using the last saved credentials (called at boot).
// Returns false immediately if none are stored.
bool wifi_manager_autoconnect();

// Signal strength of the current connection, 0-4 "bars" (for the status bar).
int wifi_manager_signal_bars();

bool wifi_manager_is_connected();

#endif // WIFI_MANAGER_H
