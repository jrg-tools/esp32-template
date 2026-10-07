#include "Button.h"

void Button::begin(uint8_t buttonPin, uint32_t settleMs, uint32_t longPressMs) {
  pin = buttonPin;
  settleDelayMs = settleMs;
  longPressDelayMs = longPressMs;
  pinMode(pin, INPUT_PULLUP);
  stableState = digitalRead(pin);
  lastReading = stableState;
}

void Button::reset() {
  stableState = digitalRead(pin);
  lastReading = stableState;
  lastChangeMs = millis();
  releasePending = false;
  longEvent = false;
  // A button already down at resync time gets no hold out of this press —
  // its start time is unknown, and timing it from here would re-fire the
  // hold that the caller has just acted on, if it resyncs straight after.
  longFired = longPressDelayMs != 0 && stableState == LOW;
}

bool Button::update() {
  bool reading = digitalRead(pin);
  if (reading != lastReading) {
    lastChangeMs = millis();
    lastReading = reading;
  }
  if (millis() - lastChangeMs >= DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      pressedAtMs = millis();
      longFired = false;
    } else if (longFired) {
      // The hold already fired for this press; the release that ends it is
      // the end of that gesture, not a press of its own.
      releasePending = false;
    } else {
      // Arm the fire but don't trigger yet: wait settleDelayMs for the
      // press/release vibration to die down. A second press+release before
      // that fires just re-arms it, coalescing rapid double-presses into
      // one.
      releasePending = true;
      releaseAtMs = millis() + settleDelayMs;
    }
  }

  // Fires under the finger rather than on release, so the hold is felt as
  // soon as it is long enough and there is nothing to keep waiting for.
  if (longPressDelayMs && !longFired && stableState == LOW && millis() - pressedAtMs >= longPressDelayMs) {
    longFired = true;
    longEvent = true;
    releasePending = false;
  }

  if (releasePending && static_cast<int32_t>(millis() - releaseAtMs) >= 0) {
    releasePending = false;
    return true;
  }
  return false;
}

bool Button::longPressed() {
  bool fired = longEvent;
  longEvent = false;
  return fired;
}
