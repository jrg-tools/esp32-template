#pragma once
#include <WebServer.h>

#include <memory>

// API-first web server. All non-API paths are served from the SvelteKit
// build on LittleFS (web/, deployed with `pnpm deploy` + `pio run -t
// uploadfs`). CORS is enabled so external clients can use the API directly.
//
//   GET  /api/health         {status,version,ip,mode,rssi,freeHeap,uptime}
//
// Add new routes in begin() and document them here.
class ConfigWebServer {
 public:
  void begin(bool apMode);
  void stop();
  void loop();

  // millis() of the last request served, 0 if none yet. Lets main.cpp treat a
  // browser talking to the device as activity that holds off idle sleep.
  uint32_t lastRequestMs() const { return lastRequest; }

 private:
  void sendText(int code, const String& body);
  bool serveFile(String path);
  void handleHealth();
  void handleNotFound();

  std::unique_ptr<WebServer> server;
  bool apMode = false;
  uint32_t lastRequest = 0;
};
