#include "Radio.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "../LoopWatchdog.h"
#include "../Settings.h"

namespace {
constexpr uint32_t kConnectPollMs = 50;
}  // namespace

void Radio::begin() {
  // Without this the SDK writes each WiFi.begin()'s credentials into its own
  // NVS and reconnects to them on the next boot, behind Settings' back.
  WiFi.persistent(false);
  off();
}

void Radio::off() {
  if (serving) {
    webServer.stop();
    httpsReject.end();
    MDNS.end();
    serving = false;
  }
  if (current != Mode::OFF) Serial.println("[Radio] off");
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  current = Mode::OFF;
}

bool Radio::joinNetwork(bool withWebServer) {
  // Deliberately blocking, and for far longer than a loop() pass is allowed
  // to take: WifiConnector gives each configured network 12 seconds before
  // moving on to the next.
  LoopWatchdog::Pause unwatched;
  off();
  WiFi.mode(WIFI_STA);
  connector.begin();
  if (connector.status() == WifiConnector::Status::NO_NETWORKS) {
    Serial.println("[Radio] no networks configured");
    off();
    return false;
  }

  while (connector.update() == WifiConnector::Status::CONNECTING) delay(kConnectPollMs);
  if (connector.status() != WifiConnector::Status::CONNECTED) {
    off();
    return false;
  }

  current = Mode::STATION;
  Serial.print("[Radio] station up, IP: ");
  Serial.println(WiFi.localIP());
  if (withWebServer) startWebServer(/*apMode=*/false);
  return true;
}

void Radio::startHotspot() {
  off();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SETTINGS.apSsid.c_str(), SETTINGS.apPassword.c_str());
  current = Mode::HOTSPOT;
  Serial.printf("[Radio] hotspot up: %s\n", SETTINGS.apSsid.c_str());
  startWebServer(/*apMode=*/true);
}

void Radio::startWebServer(bool apMode) {
  webServer.begin(apMode);
  // Distinct from the hotspot's SSID: this is the *.local name.
  if (MDNS.begin(SETTINGS.mdnsHostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[Radio] mDNS responder failed to start");
  }
  httpsReject.begin();
  serving = true;
}

void Radio::loop() {
  if (!serving) return;
  webServer.loop();
  if (httpsReject.hasClient()) httpsReject.available().stop();
}

String Radio::ip() const {
  switch (current) {
    case Mode::STATION:
      return WiFi.localIP().toString();
    case Mode::HOTSPOT:
      return WiFi.softAPIP().toString();
    case Mode::OFF:
      break;
  }
  return String();
}
