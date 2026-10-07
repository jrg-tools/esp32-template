#include "SleepPins.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include <array>

namespace {

struct PinLevel {
  int pin;
  int level;
};

// Each entry is the level that leaves its peripheral quiet with the CPU
// stopped — chip selects deselected, shared bus lines driven low, LEDs off.
// Empty until the project wires up peripherals. For example:
//
//   constexpr std::array<PinLevel, 2> kParked{{
//       {PIN_SD_CS, HIGH},
//       {PIN_LED, LOW},
//   }};
constexpr std::array<PinLevel, 0> kParked{};

constexpr gpio_num_t asGpio(int pin) { return static_cast<gpio_num_t>(pin); }

}  // namespace

void sleep_pins::hold() {
  for (const PinLevel& parked : kParked) {
    pinMode(parked.pin, OUTPUT);
    digitalWrite(parked.pin, parked.level);
    gpio_hold_en(asGpio(parked.pin));
  }

  // RTC-range pads (GPIO0-21) latch on their own. Pads outside that range
  // only keep their latch through deep sleep with the digital-pad hold
  // enabled.
  gpio_deep_sleep_hold_en();
}

void sleep_pins::release() {
  gpio_deep_sleep_hold_dis();
  for (const PinLevel& parked : kParked) gpio_hold_dis(asGpio(parked.pin));
}
