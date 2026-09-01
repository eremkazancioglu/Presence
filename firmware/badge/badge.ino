// Bedside Focus Badge — phase 1 firmware
//
// Reads a momentary push button. The button drives BLE connect/disconnect
// against a bonded phone: pressing "on" starts (and keeps) connectable
// advertising so the phone's bonded auto-reconnect completes a
// connection; pressing "off" actively disconnects. A native Shortcuts
// personal automation ("When Bluetooth device connects/disconnects")
// reacts to those two events to toggle Focus mode on the phone.
//
// This supersedes an earlier broadcast-only/iBeacon design -- background
// detection via CoreLocation (region monitoring, then CLMonitor) didn't
// work reliably in practice despite correct setup. See CLAUDE.md's
// Architecture decisions for the full investigation and why a system-level
// Bluetooth connection (this design) sidesteps that problem entirely.
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

bool focusActive = false;

int lastReading = HIGH;
int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

NimBLEServer* server;
NimBLEAdvertising* advertising;

void disconnectAllPeers() {
  for (uint16_t connHandle : server->getPeerDevices()) {
    server->disconnect(connHandle);
  }
}

class BadgeServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
      Serial.println("Connected");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
      Serial.printf("Disconnected (reason %d)\n", reason);
      // Only resume advertising if we're still meant to be ON -- an OFF
      // press already stopped advertising directly, so this path only
      // fires for an unintended drop (interference, brief range loss)
      // while ON, letting it self-heal via bonded auto-reconnect without
      // another button press.
      if (focusActive) {
        advertising->start();
      }
    }
};

void toggleFocus() {
  focusActive = !focusActive;
  Serial.printf("Focus %s\n", focusActive ? "ON" : "OFF");

  if (focusActive) {
    advertising->start();
  } else {
    disconnectAllPeers();
    advertising->stop();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  NimBLEDevice::init(DEVICE_NAME);
  // Bonding, no MITM (no display/keypad on the badge to confirm a
  // passkey -- "Just Works" pairing), LE Secure Connections.
  NimBLEDevice::setSecurityAuth(true, false, true);

  server = NimBLEDevice::createServer();
  server->setCallbacks(new BadgeServerCallbacks());
  // We manage advertising restart ourselves (see onDisconnect) based on
  // whether the badge is still meant to be ON, rather than NimBLE's
  // blanket auto-resume, which can't distinguish a deliberate OFF-press
  // disconnect from an unintended drop.
  server->advertiseOnDisconnect(false);
  server->start();

  advertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advertising->setAdvertisementData(advData);

  Serial.println("Badge ready, OFF (not advertising).");
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
