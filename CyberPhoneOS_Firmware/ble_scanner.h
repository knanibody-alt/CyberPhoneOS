#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>

#define BLE_SCAN_MAX_RESULTS 20

struct BleDevice {
  char    name[32];
  char    address[18];
  int     rssi;
};

struct BleScanResult {
  BleDevice devices[BLE_SCAN_MAX_RESULTS];
  int       count;
};

// Passive discovery scan only (listens to public advertisement packets,
// like nRF Connect / a Bluetooth "site survey" app). Runs on the network
// core. Blocking for `scanSeconds`.
void ble_scanner_run_once(BleScanResult &out, int scanSeconds = 4);

#endif // BLE_SCANNER_H
