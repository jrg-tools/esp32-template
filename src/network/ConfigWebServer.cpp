#include "ConfigWebServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "../LoopWatchdog.h"
#include "../Version.h"

namespace {
const char* contentTypeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".txt")) return "text/plain";
  if (path.endsWith(".woff2")) return "font/woff2";
  return "application/octet-stream";
}
}  // namespace

void ConfigWebServer::begin(bool ap) {
  apMode = ap;
  if (!LittleFS.begin()) {
    Serial.println("[Web] LittleFS mount failed — run `pio run -t uploadfs`");
  }

  server = std::make_unique<WebServer>(80);
  server->enableCORS(true);

  server->on("/api/health", HTTP_GET, [this] { handleHealth(); });
  // Everything else falls through to the SvelteKit build on LittleFS.
  server->onNotFound([this] { handleNotFound(); });

  server->begin();
  Serial.println("[Web] server on port 80");
}

void ConfigWebServer::stop() {
  if (!server) return;
  server->stop();
  server.reset();
  LittleFS.end();
}

void ConfigWebServer::loop() {
  // Serving a client is bounded by TCP timeouts, not by anything here: a
  // write to a peer that has stopped acknowledging blocks until the stack
  // gives up retransmitting. That's not a hang, so it isn't watched.
  LoopWatchdog::Pause unwatched;
  if (server) server->handleClient();
}

void ConfigWebServer::sendText(int code, const String& body) {
  server->send(code, "text/plain; charset=utf-8", body);
}

bool ConfigWebServer::serveFile(String path) {
  if (path.endsWith("/")) path += "index.html";
  if (!LittleFS.exists(path)) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  // Immutable hashed assets under /_app/immutable can be cached hard.
  if (path.startsWith("/_app/immutable/")) {
    server->sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  }
  server->streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

void ConfigWebServer::handleHealth() {
  lastRequest = millis();
  JsonDocument doc;
  doc["status"] = "ok";
  doc["version"] = FIRMWARE_VERSION;
  doc["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["mode"] = apMode ? "AP" : "STA";
  doc["rssi"] = apMode ? 0 : WiFi.RSSI();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  String response;
  serializeJson(doc, response);
  server->send(200, "application/json", response);
}

void ConfigWebServer::handleNotFound() {
  lastRequest = millis();
  // Answer CORS preflight; keep /api/* 404s honest so XHR errors surface.
  if (server->method() == HTTP_OPTIONS) {
    server->send(204);
    return;
  }
  if (!server->uri().startsWith("/api/")) {
    if (serveFile(server->uri())) return;
    // Each route prerenders to a flat file (e.g. /about.html) —
    // adapter-static's convention for a path with no trailing slash.
    if (serveFile(server->uri() + ".html")) return;
    if (apMode) {
      // Captive-portal style redirect for unknown non-API paths.
      server->sendHeader("Location", "/", true);
      server->send(302, "text/plain", "");
      return;
    }
    // A client-side route with no matching static file: hand it the SPA
    // fallback shell (adapter-static's `fallback: '200.html'`) so SvelteKit's
    // router can render the right page from the URL.
    if (serveFile("/200.html")) return;
  }
  sendText(404, "Not found");
}
