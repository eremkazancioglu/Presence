// Bedside Focus Badge — EXPERIMENT, not the production firmware.
//
// Tests whether UIApplication.open() succeeds from inside a CoreBluetooth
// background delegate callback (didConnect/didDisconnect, under the
// `bluetooth-central` background mode) -- a different OS-granted
// background execution context than the CLMonitor-triggered wake that
// hit a hard wall (LSApplicationWorkspaceErrorDomain 115) despite
// delivering the beacon event itself reliably. See
// docs/trigger-mechanism-investigation.md for that history, and the
// "no Full Keyboard Access" discussion for why this test exists.
//
// Minimal connectable BLE peripheral, no HID, no bonding: button press
// toggles connectable advertising on/off. The companion app's
// CentralConnectionMonitor.swift (ios/) scans for and connects to this by
// service UUID, and calls FocusTrigger.setFocus() straight from its
// didConnect/didDisconnect delegate methods.
//
// Flash this TEMPORARILY in place of firmware/badge/badge.ino to run the
// test, then reflash badge.ino afterward -- this does not replace it.
//
// Board: Seeed Studio XIAO ESP32C6
// Library: NimBLE-Arduino (h2zero)

#include <NimBLEDevice.h>

// TEMP: no external tactile button wired yet -- jumper D0 to GND to
// simulate a press. See firmware/badge/badge.ino for the same note.
#define BUTTON_PIN D0
#define DEBOUNCE_MS 50
#define DEVICE_NAME "PresenceBadgeTest"

// Must match CentralConnectionMonitor.swift's testServiceUUID.
#define TEST_SERVICE_UUID "8E400001-F315-4F60-9FB8-838830DAEA50"

bool connectableOn = false;

int lastReading = HIGH;
int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

NimBLEServer* server;
NimBLEAdvertising* advertising;

void toggleAdvertising() {
  connectableOn = !connectableOn;
  Serial.printf("Advertising %s\n", connectableOn ? "ON" : "OFF");

  if (connectableOn) {
    advertising->start();
  } else {
    for (uint16_t connHandle : server->getPeerDevices()) {
      server->disconnect(connHandle);
    }
    advertising->stop();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  NimBLEDevice::init(DEVICE_NAME);

  server = NimBLEDevice::createServer();
  NimBLEService* testService = server->createService(TEST_SERVICE_UUID);
  testService->start();
  server->start();

  advertising = NimBLEDevice::getAdvertising();

  // Name (20 bytes) + 128-bit service UUID (18 bytes) together exceed the
  // legacy advertising packet's 31-byte limit -- split across the primary
  // packet (service UUID, so the app's withServices: scan filter matches)
  // and the scan response packet (name, cosmetic only).
  NimBLEAdvertisementData advData;
  advData.setCompleteServices(NimBLEUUID(TEST_SERVICE_UUID));
  advertising->setAdvertisementData(advData);

  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(DEVICE_NAME);
  advertising->setScanResponseData(scanResponseData);
  advertising->enableScanResponse(true);

  Serial.println("Test badge ready, OFF (not advertising). Press to toggle.");
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != debouncedState) {
      debouncedState = reading;
      // Active LOW: button pressed pulls the pin to GND.
      if (debouncedState == LOW) {
        toggleAdvertising();
      }
    }
  }

  lastReading = reading;
}
