// Bedside Focus Badge — phase 1 firmware
//
// Reads a momentary push button. The badge is a BLE HID keyboard (via the
// HijelHID_BLEKeyboard library), staying continuously connected once
// paired -- like a real Bluetooth keyboard, not toggling connect/
// disconnect per press. Each press sends one distinct keystroke: F13 for
// "on", F14 for "off". On the phone, Settings -> Accessibility ->
// Keyboards & Typing -> Full Keyboard Access -> Commands binds each key to
// a Shortcut (Badge Focus On / Badge Focus Off) natively, with no app
// needed at all -- this fires reliably even while the phone is locked
// (verified empirically with a real Bluetooth accessory before committing
// to this design).
//
// This supersedes two earlier designs: broadcast-only/iBeacon (background
// detection via CoreLocation didn't work reliably), and bonded connect/
// disconnect toggling (regular BLE peripherals don't get iOS's app-free
// auto-reconnect treatment -- only recognized classes like HID and audio
// devices do, which is exactly why this design uses real HID). See
// CLAUDE.md's Architecture decisions for the full investigation.
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
  if (!keyboard.isPaired()) {
    Serial.println("Not paired yet -- ignoring press. Pair in Settings > Bluetooth first.");
    return;
  }

  focusActive = !focusActive;
  Serial.printf("Focus %s -- sending %s\n", focusActive ? "ON" : "OFF", focusActive ? "F13" : "F14");
  keyboard.tap(focusActive ? KEY_F13 : KEY_F14);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  keyboard.setLogLevel(HIDLogLevel::Normal);
  keyboard.begin();

  Serial.println("Badge ready. Pair via Settings > Bluetooth, then press to send F13/F14.");
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
