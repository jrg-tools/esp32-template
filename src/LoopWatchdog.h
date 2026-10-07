#pragma once
#include <Arduino.h>

// Subscribes loopTask to the ESP-IDF Task Watchdog, so a loop() that stops
// making progress reboots the device instead of hanging until someone
// notices.
//
// Nothing else covers this gap: the interrupt watchdog catches blocked ISRs
// and the TWDT already watches CPU0's idle task, but loopTask runs on CPU1 —
// whose idle task the Arduino SDK config leaves unwatched — and a loopTask
// blocked on a queue yields normally, so nothing complains. The panic handler
// reboots on timeout (CONFIG_ESP_TASK_WDT_PANIC), so subscribing is the whole
// of the work; it comes back as reset reason 6 with a backtrace naming the
// task that stopped feeding.
//
// Last line of defence. Anything reaching here is a bug.
class LoopWatchdog {
 public:
  // Raised from the SDK's default 5s, so an occasional slow pass (a blocking
  // flash write, say) isn't mistaken for a hang. Sized generously because the
  // failures aren't symmetrical — firing early interrupts real work, firing
  // late costs a few more seconds of a device that is already wedged.
  //
  // Global to the TWDT, so this loosens the CPU0 idle-task check to match.
  static constexpr uint32_t TIMEOUT_S = 15;

  // Call last in setup(): there is no value in policing a one-shot boot
  // sequence whose failure is obvious anyway.
  static void begin();

  // Once per loop() pass, at the top.
  static void feed();

  // Suspends coverage for a call *expected* to block longer than TIMEOUT_S
  // and restores it on scope exit, early returns included. Joining a network
  // tries every configured SSID at 12s each. That isn't a hang, and rebooting
  // through it would leave the device unable to reach the internet at all.
  class Pause {
   public:
    Pause();
    ~Pause();
    Pause(const Pause&) = delete;
    Pause& operator=(const Pause&) = delete;

   private:
    // A task that wasn't subscribed on the way in must not be on the way out.
    bool wasSubscribed = false;
  };

  // Widens the timeout instead of dropping the watch, for work that runs for
  // minutes but produces a steady stream of chances to feed() — e.g. an OTA
  // install, flashed a block at a time. Unlike Pause, the window still
  // measures the gap *between* blocks, so a slow link is tolerated and a dead
  // one reboots.
  //
  // Restores TIMEOUT_S on scope exit. Like begin(), this is the TWDT's global
  // timeout, so it loosens the CPU0 idle-task check for the same window.
  class Stretch {
   public:
    explicit Stretch(uint32_t seconds);
    ~Stretch();
    Stretch(const Stretch&) = delete;
    Stretch& operator=(const Stretch&) = delete;

   private:
    // Nothing to put back if widening it didn't take.
    bool applied = false;
  };
};
