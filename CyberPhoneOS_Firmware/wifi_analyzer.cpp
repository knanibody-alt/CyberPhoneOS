#include "wifi_analyzer.h"
#include "sd_manager.h"
#include <WiFi.h>

static String csv_escape(const String &raw) {
  String out;
  out.reserve(raw.length() + 2);
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if (c == '"' || c == '\n' || c == '\r') {
      out += '"'; // CSV doubles the quote char to escape it
    }
    out += c;
  }
  return out;
}

void wifi_analyzer_run_once(WifiScanResult &out) {
  memset(&out, 0, sizeof(out));

  // Synchronous scan is fine here - this function only ever runs on the
  // network core (Core 0), so it never blocks the LVGL/UI core.
  int n = WiFi.scanNetworks(false, true /*show hidden*/, false, 300 /*ms per channel*/);
  out.totalFound = max(n, 0);

  for (int i = 0; i < n; i++) {
    int ch = WiFi.channel(i);
    if (ch >= 1 && ch < WIFI_CHANNEL_COUNT) {
      out.channelCounts[ch]++;
    }

    int rssi = WiFi.RSSI(i);
    // Insertion into the "top N strongest" list, weakest gets evicted.
    for (int slot = 0; slot < WIFI_TOP_AP_COUNT; slot++) {
      if (out.topSsid[slot][0] == '\0' || rssi > out.topRssi[slot]) {
        // shift the rest down
        for (int k = WIFI_TOP_AP_COUNT - 1; k > slot; k--) {
          strlcpy(out.topSsid[k], out.topSsid[k-1], sizeof(out.topSsid[k]));
          out.topRssi[k] = out.topRssi[k-1];
          out.topChannel[k] = out.topChannel[k-1];
        }
        strlcpy(out.topSsid[slot], WiFi.SSID(i).c_str(), sizeof(out.topSsid[slot]));
        out.topRssi[slot] = rssi;
        out.topChannel[slot] = ch;
        break;
      }
    }

    // Optional forensics log requested in the SD folder layout.
    // SSIDs are arbitrary bytes and very often contain commas, so wrap the
    // field in double quotes and neutralise embedded quotes - otherwise a
    // single AP shifts every following column in wifi_scan.csv.
    String row = String(millis()) + ",\"" + csv_escape(WiFi.SSID(i)) + "\"," +
                 String(rssi) + "," + String(ch);
    sd_log_wifi_scan_row(row);
  }

  WiFi.scanDelete();
}
