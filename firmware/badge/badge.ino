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

#define WAKE_SETTLE_MS 1200
#define REPEAT_ATTEMPTS 4
#define REPEAT_GAP_MS 400

void toggleFocus() {
  if (!keyboard.isPaired()) {
    Serial.println("Not paired yet -- ignoring press. Pair in Settings > Bluetooth first.");
    return;
  }

  focusActive = !focusActive;
  // F13/F14 turned out not to be reliably distinguished by iOS's Full
  // Keyboard Access command recorder (both attempts recorded as the same
  // binding) -- switched to standard letter keys with modifiers, which
  // should be unambiguous.
  uint8_t modifiers = KEY_MOD_LCTRL | KEY_MOD_LALT;
  uint8_t targetKey = focusActive ? KEY_O : KEY_F;
  Serial.printf("Focus %s -- sending Ctrl+Option+%s (repeated, idempotent)\n", focusActive ? "ON" : "OFF", focusActive ? "O" : "F");

  // Real command delivery to a locked/sleeping phone turned out to be
  // unreliable on any single attempt -- observed empirically to
  // eventually "take hold" after a few tries. Since Set Focus is
  // idempotent (applying "on" repeatedly has the same effect as once),
  // it's safe to just resend the same target state several times rather
  // than trying to precisely detect success. F13 (unbound to any Full
  // Keyboard Access command) wakes the phone first each round -- its only
  // job is to wake the screen, harmless even if it does get processed.
  for (int attempt = 1; attempt <= REPEAT_ATTEMPTS; attempt++) {
    Serial.printf("Attempt %d/%d: wake (F13) + real command...\n", attempt, REPEAT_ATTEMPTS);
    keyboard.tap(KEY_F13);
    delay(WAKE_SETTLE_MS);
    keyboard.tap(targetKey, modifiers);
    delay(REPEAT_GAP_MS);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  keyboard.setLogLevel(HIDLogLevel::Normal);
  // A longer hold (900ms) was tried to address a theory about BLE idle
  // timing that later testing ruled out -- the actual issue turned out to
  // be phone-side handling of input while locked/sleeping (see
  // toggleFocus()'s repeat-attempts comment), not report delivery timing.
  // Modest increase over the 25ms default kept as a small safety margin
  // without blowing up total time across REPEAT_ATTEMPTS.
  keyboard.setTapDelay(100);
  keyboard.begin();

  Serial.println("Badge ready. Pair via Settings > Bluetooth, then press to toggle focus.");
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
