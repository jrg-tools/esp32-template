#pragma once
#include <Arduino.h>

#include <vector>

#include "../Settings.h"

// Tries each configured WiFi network in order until one connects or all have
// been tried. Poll-driven via update(); never blocks.
class WifiConnector {
 public:
  enum class Status { NO_NETWORKS, CONNECTING, CONNECTED, FAILED };

  static constexpr uint32_t PER_NETWORK_TIMEOUT_MS = 12000;

  // Starts attempting the first configured network. Status is NO_NETWORKS
  // if none are configured.
  void begin();

  // Advances the current attempt; call every loop(). Returns the new status.
  Status update();

  Status status() const { return currentStatus; }
  // SSID of the network currently (or last) being attempted.
  const String& ssid() const { return currentSsid; }

 private:
  void startAttempt(size_t idx);

  std::vector<Settings::WifiNetwork> networks;
  size_t index = 0;
  uint32_t attemptStartMs = 0;
  String currentSsid;
  Status currentStatus = Status::NO_NETWORKS;
};
