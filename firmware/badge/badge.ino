// Bedside Focus Badge — phase 1 firmware
//
// Reads a momentary push button. The badge is a real BLE HID keyboard
// (via the HijelHID_BLEKeyboard library) -- but only to get iOS's
// HID-specific treatment: proper Settings > Bluetooth pairing visibility,
// and genuine system-level auto-reconnect with no app running. It never
// actually sends keystrokes. Instead, the button toggles the BLE
// *connection* itself: "on" resumes advertising (bonded auto-reconnect
// completes a connection), "off" actively disconnects. A native Shortcuts
// personal automation ("When Bluetooth device connects/disconnects")
// reacts to those two events to toggle Focus mode.
//
// This supersedes three earlier designs -- broadcast-only/iBeacon,
// bonded connect/disconnect on a plain (non-HID) peripheral, and HID
// keyboard actually sending keystrokes. The keystroke-sending version
// worked at the BLE level but iOS silently discards (redirects to the
// unlock screen instead of processing) any external keyboard input while
// the phone is locked -- a hard security boundary, not fixable from
// firmware. Bluetooth connect/disconnect events aren't gated by lock
// state the same way (verified empirically with a real Bluetooth
// accessory), which is why this design goes back to connect/disconnect
// as the actual signal, just using genuine HID classification (unlike
// the second design's Heart Rate Service masquerade) to get real
// app-free auto-reconnect. See CLAUDE.md's Architecture decisions for
// the full investigation.
//
// Board: Seeed Studio XIAO ESP32C6
// Libraries: NimBLE-Arduino (h2zero), HijelHID_BLEKeyboard (Hijel) —
// both via Arduino Library Manager.

#include <HijelHID_BLEKeyboard.h>

// TEMP: no external tactile button wired yet. Until it arrives, jumper a
// wire from the header pin labeled "D0" to a GND pin to simulate a press
// (D0 = GPIO0 in the XIAO ESP32C6's pin mapping, active LOW via the
// internal pull-up below). Once the real button is wired, change
// BUTTON_PIN to that pin — the debounce/toggle logic doesn't need to
// change. Use the D-number macro (not a raw GPIO number) so the pin in
// code always matches the pin printed on the board's silkscreen.
#define BUTTON_PIN D0

#define DEBOUNCE_MS 50

HijelHID_BLEKeyboard keyboard("PresenceBadge", "Presence", 100);

bool focusActive = false;

int lastReading = HIGH;
int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

void toggleFocus() {
  focusActive = !focusActive;
  Serial.printf("Focus %s\n", focusActive ? "ON" : "OFF");

  if (focusActive) {
    // Resumes advertising; since the phone already trusts this bonded
    // HID device, iOS reconnects automatically -- that reconnection is
    // the "device connects" event the Shortcuts automation reacts to.
    keyboard.begin();
  } else {
    // Actively disconnects (a live connection isn't advertising anyway,
    // so merely stopping advertising wouldn't drop it) and stops
    // advertising, so nothing reconnects until the next "on" press.
    keyboard.end();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  keyboard.setLogLevel(HIDLogLevel::Normal);
  keyboard.begin();

  Serial.println("Badge ready, OFF. Pair via Settings > Bluetooth, then press to toggle.");
  keyboard.end();
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

  // Self-heal an unintended drop (RF interference, brief range loss)
  // while still meant to be ON: re-arm advertising so the phone's bonded
  // auto-reconnect can complete again, without waiting for another
  // button press. begin() is a documented no-op if already
  // advertising/connected, so this is safe to check repeatedly.
  static unsigned long lastReconnectCheck = 0;
  if (focusActive && !keyboard.isPaired() && millis() - lastReconnectCheck > 1000) {
    lastReconnectCheck = millis();
    Serial.println("Still meant to be ON but not connected -- re-arming advertising.");
    keyboard.begin();
  }
}
