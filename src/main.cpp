// ESP32-S3 firmware template.
//
// On boot the device joins the first configured WiFi network that answers
// (see Settings::wifiNetworks), falling back to its own hotspot, and serves
// the SvelteKit web UI from LittleFS plus a JSON API (network/ConfigWebServer.h).
//
// One button on D1 (see the wiring table in README.md):
//   press   counts as activity, holding off idle sleep
//   hold    puts the device to sleep straight away
// After kIdleSleepMs with no button press and no web request the device goes
// into deep sleep; the same button wakes it.
//
// If loop() stops making progress at all, LoopWatchdog reboots the device.

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "LoopWatchdog.h"
#include "Settings.h"
#include "Version.h"
#include "input/Button.h"
#include "network/Radio.h"
#include "power/SleepPins.h"

Radio radio;
Button button;

namespace {
// D1/GPIO2 is inside the RTC GPIO range (0-21), which is what lets it wake the
// chip from deep sleep. Any other RTC-range pin works the same way.
constexpr uint8_t kButtonPin = D1;

constexpr uint32_t kLoopDelayMs = 10;

// How long the device sits untouched before going to sleep.
constexpr uint32_t kIdleSleepMs = 5 * 60 * 1000;

// How long the button has to be held to put the device to sleep on demand.
constexpr uint32_t kSleepHoldMs = 1500;

// How long to wait, after a wake, for the press that did the waking to be
// released — long enough for a deliberate press, short enough that a stuck
// button costs a moment rather than the boot.
constexpr uint32_t kWakeReleaseMs = 3000;

// When the device was last used for anything. Button presses and web requests
// push it forward; enforceIdleSleep() puts the device to sleep once it stops
// moving.
uint32_t lastActivityMs = 0;
void noteActivity() { lastActivityMs = millis(); }

// Shuts the device down as far as an ESP32-S3 can go. There is no true off:
// the XIAO has no software-controlled load switch, so its 3V3 rail stays up
// whatever the firmware does. Deep sleep is the next thing to it — CPU and RAM
// powered down, every peripheral stopped and its pins parked first (see
// power/SleepPins.h), leaving the RTC domain holding one pin awake to bring it
// all back. Does not return.
void enterDeepSleep() {
  // The shutdown is one long loop() pass with no feed() in it. Never
  // restored, and it doesn't need to be: this function does not return.
  LoopWatchdog::Pause unwatched;

  Serial.println("[Power] going to sleep; the button wakes it");
  Serial.flush();

  // Stop every driver before its pins are parked. Add new peripherals here.
  radio.off();

  sleep_pins::hold();

  // The button pulls to ground, hence a wake level of 0, and the pull-up has
  // to be re-armed through the RTC domain: pinMode(INPUT_PULLUP) is a
  // digital-domain setting that does not survive into sleep, so without this
  // the pin floats and wakes on noise.
  const gpio_num_t wakePin = static_cast<gpio_num_t>(kButtonPin);
  rtc_gpio_pullup_en(wakePin);
  rtc_gpio_pulldown_dis(wakePin);
  esp_sleep_enable_ext0_wakeup(wakePin, 0);

  // Every power domain defaults to ESP_PD_OPTION_AUTO, which already powers
  // down whatever the configured wake sources don't need.
  esp_deep_sleep_start();
}

// Invariant: an untouched device doesn't stay awake.
void enforceIdleSleep() {
  const uint32_t lastRequestMs = radio.lastRequestMs();
  if (lastRequestMs && static_cast<int32_t>(lastRequestMs - lastActivityMs) > 0) {
    lastActivityMs = lastRequestMs;
  }
  if (millis() - lastActivityMs < kIdleSleepMs) return;
  Serial.println("[Power] idle");
  enterDeepSleep();
}

}  // namespace

void setup() {
  // First, before anything claims a pin: the pad latches enterDeepSleep()
  // applied survive the wake reset, and latched pins cannot be driven.
  sleep_pins::release();

  Serial.begin(115200);
  SETTINGS.load();
  Serial.print("[Main] firmware ");
  Serial.println(FIRMWARE_VERSION);
  // Printed every boot so a reset *loop* is unmistakable rather than inferred
  // from symptoms. Reset reasons: 1 power-on, 3 software restart, 4 panic,
  // 5 interrupt watchdog, 6 task watchdog, 8 wake from deep sleep, 9 brownout.
  Serial.printf("[Main] boot: reset reason %d, heap %u, psram %u\n", static_cast<int>(esp_reset_reason()),
                ESP.getFreeHeap(), ESP.getFreePsram());

  // WPA2's 8-character floor: anything shorter would make softAP() fall back
  // to an open network, so a random one is generated on first boot.
  if (SETTINGS.apPassword.length() < 8) {
    SETTINGS.apPassword = String(random(10000000, 99999999));
    SETTINGS.save();
  }
  Serial.printf("[Main] hotspot: %s (password: %s)\n", SETTINGS.apSsid.c_str(), SETTINGS.apPassword.c_str());

  // The press that woke the device is likely still down, and buttons fire on
  // *release* — so starting the button now would read that release as a
  // fresh press.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[Power] woken by the button");
    pinMode(kButtonPin, INPUT_PULLUP);
    const uint32_t startedMs = millis();
    while (digitalRead(kButtonPin) == LOW && millis() - startedMs < kWakeReleaseMs) delay(10);
  }
  button.begin(kButtonPin, /*settleMs=*/0, kSleepHoldMs);

  radio.begin();
  if (!radio.joinNetwork()) radio.startHotspot();
  Serial.printf("[Main] web UI at http://%s.local/ (%s)\n", SETTINGS.mdnsHostname.c_str(), radio.ip().c_str());

  // So the idle timeout measures time since boot finished, not since
  // power-on.
  noteActivity();

  // Last, so the one-shot boot sequence above isn't policed by a timeout
  // meant for the steady state.
  LoopWatchdog::begin();
}

void loop() {
  // Everything below has LoopWatchdog::TIMEOUT_S to finish, except the few
  // calls that take a LoopWatchdog::Pause because they block for longer.
  LoopWatchdog::feed();

  const bool pressed = button.update();
  // Read straight after update(), which is what detects it.
  const bool held = button.longPressed();
  if (pressed) {
    Serial.println("[Button] press");
    noteActivity();
  }
  if (held) enterDeepSleep();

  radio.loop();

  enforceIdleSleep();

  delay(kLoopDelayMs);
}
