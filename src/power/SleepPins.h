#pragma once

// Deep sleep releases every pad to high impedance, but peripherals on the
// board's 3V3 rail stay powered: the XIAO has no software-controlled load
// switch. A floating input on a powered CMOS chip draws current through its
// own input buffer, and a chip select left floating can keep a peripheral
// awake — together they can cost milliamps for as long as the device is
// "off".
//
// hold() parks the pins listed in SleepPins.cpp at their inactive levels and
// latches them there for the duration of the sleep; release() undoes it. The
// latch lives in the RTC domain and survives the wake reset, so release() has
// to run before anything tries to drive those pins again.
namespace sleep_pins {

// Call immediately before esp_deep_sleep_start(), after every driver that
// owns one of these pins has been shut down.
void hold();

// Call at the top of setup(), before any driver claims its pins. Safe on a
// cold boot, where there is nothing latched to release.
void release();

}  // namespace sleep_pins
