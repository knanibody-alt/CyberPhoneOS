#ifndef WIFI_ANALYZER_H
#define WIFI_ANALYZER_H

#include <Arduino.h>

#define WIFI_CHANNEL_COUNT 14
#define WIFI_TOP_AP_COUNT   5

// Passive results only: how many networks sit on each 2.4GHz channel
// (1-13) and the strongest few APs seen. No packet injection, no
// deauthentication, no association - this only reads public beacon
// frames the same way any Wi-Fi analyzer app does.
struct WifiScanResult {
  int   channelCounts[WIFI_CHANNEL_COUNT]; // index = channel number
  char  topSsid[WIFI_TOP_AP_COUNT][33];
  int   topRssi[WIFI_TOP_AP_COUNT];
  int   topChannel[WIFI_TOP_AP_COUNT];
  int   totalFound;
};

// Runs one full passive scan/tally cycle. Blocking for the duration of the
// underlying WiFi.scanNetworks() call (a few seconds) - call this from the
// network core (core 0) task, never from the UI core.
void wifi_analyzer_run_once(WifiScanResult &out);

#endif // WIFI_ANALYZER_H
