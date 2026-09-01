// Bedside Focus Badge — phase 1 firmware
//
// Reads a momentary push button. Each press toggles an on/off state and
// updates a continuous BLE advertisement reflecting that state. No pairing,
// no GATT service — see docs/ble-protocol.md for the advertising format.
//
// Board: Seeed Studio XIAO ESP32C6
// Library: NimBLE-Arduino (h2zero) — install via Arduino Library Manager.

#include <NimBLEDevice.h>

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
#define MANUFACTURER_ID 0xFFFF
#define PROTOCOL_MAGIC 0x50

bool focusActive = false;

int lastReading = HIGH;
int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

NimBLEAdvertising *advertising;

void updateAdvertisement() {
  std::string mfgData;
  mfgData += (char)(MANUFACTURER_ID & 0xFF);
  mfgData += (char)((MANUFACTURER_ID >> 8) & 0xFF);
  mfgData += (char)PROTOCOL_MAGIC;
  mfgData += (char)(focusActive ? 0x01 : 0x00);

  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setManufacturerData(mfgData);

  advertising->stop();
  advertising->setAdvertisementData(advData);
  advertising->start();
}

void toggleFocus() {
  focusActive = !focusActive;
  Serial.printf("Focus %s\n", focusActive ? "ON" : "OFF");
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
