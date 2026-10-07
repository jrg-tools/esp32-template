#pragma once
#include <Arduino.h>

#include <vector>

// Persisted device settings singleton. Backed by NVS (Preferences).
class Settings {
 public:
  static Settings& getInstance() {
    static Settings instance;
    return instance;
  }

  // A WiFi network to try. Tried in order (see wifis()) until one connects.
  struct WifiNetwork {
    String ssid;
    String password;
  };
  // JSON array of WifiNetwork, e.g. [{"ssid":"home","password":"..."}].
  String wifiNetworks = "[]";

  // Hotspot credentials, used when no configured network answers. The
  // password is generated on first boot (see main.cpp) and persisted.
  String apSsid = "esp32";
  String apPassword;

  // The device answers at http://<mdnsHostname>.local/ on whichever
  // interface is up. Must only contain [a-z0-9-].
  static constexpr const char* DEFAULT_HOSTNAME = "esp32";
  String mdnsHostname = DEFAULT_HOSTNAME;

  void load();
  void save() const;

  std::vector<WifiNetwork> wifis() const;

 private:
  Settings() = default;
};

#define SETTINGS Settings::getInstance()
