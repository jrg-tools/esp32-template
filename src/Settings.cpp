#include "Settings.h"

#include <ArduinoJson.h>
#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "settings";
}  // namespace

void Settings::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  wifiNetworks = prefs.getString("wifiNetworks", wifiNetworks);
  apSsid = prefs.getString("apSsid", apSsid);
  apPassword = prefs.getString("apPassword", "");
  mdnsHostname = prefs.getString("mdnsHostname", mdnsHostname);
  prefs.end();
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putString("wifiNetworks", wifiNetworks);
  prefs.putString("apSsid", apSsid);
  prefs.putString("apPassword", apPassword);
  prefs.putString("mdnsHostname", mdnsHostname);
  prefs.end();
  Serial.println("[Settings] saved");
}

std::vector<Settings::WifiNetwork> Settings::wifis() const {
  std::vector<WifiNetwork> out;
  JsonDocument doc;
  if (deserializeJson(doc, wifiNetworks) != DeserializationError::Ok) return out;

  for (JsonObjectConst entry : doc.as<JsonArrayConst>()) {
    WifiNetwork n;
    n.ssid = entry["ssid"] | "";
    n.password = entry["password"] | "";
    if (n.ssid.length()) out.push_back(n);
  }
  return out;
}
