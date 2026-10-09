#include "ble_scanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

void ble_scanner_run_once(BleScanResult &out, int scanSeconds) {
  memset(&out, 0, sizeof(out));

  BLEDevice::init("");
  BLEScan *scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);

  BLEScanResults results = *(scanner->start(scanSeconds, false));

  int n = min((int)results.getCount(), BLE_SCAN_MAX_RESULTS);
  for (int i = 0; i < n; i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    BleDevice &slot = out.devices[out.count];

    String name = dev.haveName() ? dev.getName().c_str() : "(isimsiz cihaz)";
    strlcpy(slot.name, name.c_str(), sizeof(slot.name));
    strlcpy(slot.address, dev.getAddress().toString().c_str(), sizeof(slot.address));
    slot.rssi = dev.haveRSSI() ? dev.getRSSI() : 0;
    out.count++;
  }

  scanner->clearResults();
  BLEDevice::deinit(false);
}
