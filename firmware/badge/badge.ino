// Bedside Focus Badge — phase 1 firmware
//
// Reads a momentary push button. Each press toggles an on/off state and
// updates a continuous BLE advertisement reflecting that state, in
// standard iBeacon format so iOS can reliably react to it even while
// backgrounded/locked via CoreLocation region monitoring — see
// docs/ble-protocol.md for why iBeacon rather than custom manufacturer
// data, and the exact payload layout. No pairing, no GATT service.
//
// Board: Seeed Studio XIAO ESP32C6
// Library: NimBLE-Arduino (h2zero) — install via Arduino Library Manager.

#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>

// TEMP: no external tactile button wired yet. Until it arrives, jumper a
// wire from the header pin labeled "D0" to a GND pin to simulate a press
// (D0 = GPIO0 in the XIAO ESP32C6's pin mapping, active LOW via the
// internal pull-up below). Once the real button is wired, change
// BUTTON_PIN to that pin — the debounce/toggle logic doesn't need to
// change. Use the D-number macro (not a raw GPIO number) so the pin in
// code always matches the pin printed on the board's silkscreen.
#define BUTTON_PIN D0

#define DEBOUNCE_MS 50

#define DEVICE_NAME "PresenceBadge"

// iBeacon fields — see docs/ble-protocol.md.
#define BEACON_UUID "651bbfec-f197-444e-bf25-d72c1d4ccd84"
#define BEACON_MAJOR 1
#define BEACON_MINOR_OFF 0
#define BEACON_MINOR_ON 1
#define BEACON_TX_POWER (-59) // standard iBeacon calibration reference

bool focusActive = false;

int lastReading = HIGH;
int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

NimBLEAdvertising *advertising;

void updateAdvertisement() {
  NimBLEBeacon beacon;
  beacon.setManufacturerId(0x004C); // Apple, required for iBeacon format
  beacon.setProximityUUID(NimBLEUUID(BEACON_UUID));
  beacon.setMajor(BEACON_MAJOR);
  beacon.setMinor(focusActive ? BEACON_MINOR_ON : BEACON_MINOR_OFF);
  beacon.setSignalPower(BEACON_TX_POWER);

  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setManufacturerData(beacon.getData());

  // Name goes in the scan response, not the main advertisement — the
  // iBeacon payload alone nearly fills the 31-byte legacy advertising
  // packet. Purely for human-readable identification when debugging with
  // a generic BLE scanner; CoreLocation matches on UUID/Major/Minor only.
  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(DEVICE_NAME);

  advertising->stop();
  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanResponseData);
  advertising->start();
}

void toggleFocus() {
  focusActive = !focusActive;
  Serial.printf("Focus %s (Minor %d)\n", focusActive ? "ON" : "OFF", focusActive ? BEACON_MINOR_ON : BEACON_MINOR_OFF);
  updateAdvertisement();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  NimBLEDevice::init(DEVICE_NAME);
  advertising = NimBLEDevice::getAdvertising();
  updateAdvertisement();

  Serial.println("Badge ready, advertising OFF state.");
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
        toggleFocus();
      }
    }
  }

  lastReading = reading;
}
