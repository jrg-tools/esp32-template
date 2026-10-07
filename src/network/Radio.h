#pragma once
#include <Arduino.h>
#include <WiFiServer.h>

#include "ConfigWebServer.h"
#include "WifiConnector.h"

// The device's radio, and everything that only makes sense while it is up:
// the web server, the mDNS responder, and the HTTPS rejecter.
class Radio {
 public:
  enum class Mode { OFF, STATION, HOTSPOT };

  // Parks the radio off.
  void begin();

  Mode mode() const { return current; }
  bool on() const { return current != Mode::OFF; }

  // Tries each configured network in turn, blocking until one answers or all
  // time out.
  bool joinNetwork(bool withWebServer = true);

  void startHotspot();

  void off();

  // Services the web server. No-op while the radio is off.
  void loop();

  // Empty while the radio is off.
  String ip() const;

  // See ConfigWebServer::lastRequestMs().
  uint32_t lastRequestMs() const { return webServer.lastRequestMs(); }

 private:
  void startWebServer(bool apMode);

  WifiConnector connector;
  ConfigWebServer webServer;
  // Nothing is meant to be listening here: a browser that tries HTTPS first
  // (Chrome's "Always use secure connections") gets an immediate close instead
  // of a multi-second timeout on every load.
  WiFiServer httpsReject{443};
  Mode current = Mode::OFF;
  bool serving = false;
};
