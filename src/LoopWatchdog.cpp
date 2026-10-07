#include "LoopWatchdog.h"

#include <esp_task_wdt.h>

void LoopWatchdog::begin() {
  // The SDK initialized the TWDT at boot, so this updates the existing
  // timer's timeout rather than creating a second one.
  esp_err_t err = esp_task_wdt_init(TIMEOUT_S, /*panic=*/true);
  if (err != ESP_OK) {
    Serial.printf("[Watchdog] init failed: 0x%x\n", err);
    return;
  }
  // setup() and loop() are the same task, so nullptr is loopTask.
  err = esp_task_wdt_add(nullptr);
  if (err != ESP_OK) {
    Serial.printf("[Watchdog] subscribe failed: 0x%x\n", err);
    return;
  }
  Serial.printf("[Watchdog] loop watched, %lus timeout\n", static_cast<unsigned long>(TIMEOUT_S));
}

void LoopWatchdog::feed() { esp_task_wdt_reset(); }

LoopWatchdog::Pause::Pause() : wasSubscribed(esp_task_wdt_status(nullptr) == ESP_OK) {
  if (wasSubscribed) esp_task_wdt_delete(nullptr);
}

LoopWatchdog::Pause::~Pause() {
  if (wasSubscribed) esp_task_wdt_add(nullptr);
}

LoopWatchdog::Stretch::Stretch(uint32_t seconds)
    : applied(esp_task_wdt_init(seconds, /*panic=*/true) == ESP_OK) {
  // The old timeout may already be most of the way spent.
  if (applied) esp_task_wdt_reset();
}

LoopWatchdog::Stretch::~Stretch() {
  if (!applied) return;
  esp_task_wdt_init(TIMEOUT_S, /*panic=*/true);
  // Same reason as above, in the other direction: the next feed() is a whole
  // loop() pass away and the widened window is gone.
  esp_task_wdt_reset();
}
