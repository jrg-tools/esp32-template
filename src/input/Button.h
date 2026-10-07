#pragma once
#include <Arduino.h>

// A tactile button, one leg to the pin and the other to GND, so a press
// pulls the pin low (see the wiring table in README.md).
//
// Fires on release rather than on press, deferred until settleMs past the
// debounced release (pass settleMs=0 to fire as soon as it debounces).
// Optionally reports a hold gesture via longPressed(). Poll-driven; never
// blocks.
class Button {
 public:
  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t DEFAULT_SETTLE_MS = 50;

  // `longPressMs` of 0 leaves the hold gesture off entirely, which is what
  // every button that only has one meaning wants.
  void begin(uint8_t pin, uint32_t settleMs = DEFAULT_SETTLE_MS, uint32_t longPressMs = 0);

  // Call every loop(). Returns true exactly once per press+release cycle,
  // settleMs after the debounced release. A press that ran long enough for
  // longPressed() returns nothing here: one gesture is one event.
  bool update();

  // True once per press, the moment it has been held for longPressMs — while
  // still down, so the hold is felt rather than waited out. Read right after
  // update(), which is what detects it.
  bool longPressed();

  // Re-syncs to the pin's current level and drops any armed fire. Call after
  // anything that blocks loop() for much longer than DEBOUNCE_MS. Debouncing
  // is measured against the last *observed* change, so while nothing is
  // polling that timestamp goes stale and the window is already satisfied when
  // polling resumes — the next differing sample would fire with none of the
  // required stability.
  void reset();

 private:
  uint8_t pin = 0;
  uint32_t settleDelayMs = DEFAULT_SETTLE_MS;
  uint32_t longPressDelayMs = 0;
  bool stableState = HIGH;
  bool lastReading = HIGH;
  uint32_t lastChangeMs = 0;
  bool releasePending = false;
  uint32_t releaseAtMs = 0;
  uint32_t pressedAtMs = 0;
  // Set for the whole of a press once its hold has fired, so it can't repeat
  // while the finger is down and the release is swallowed rather than read as
  // a second, short press.
  bool longFired = false;
  bool longEvent = false;
};
