# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A template for ESP32-S3 firmware with a web UI (Seeed XIAO ESP32-S3, Arduino framework on PlatformIO, single env `xiao_esp32s3`). It has no application logic of its own: boot → join WiFi or start a hotspot → serve `/api/health` and a static SvelteKit UI from LittleFS → deep sleep when idle. Projects are built on top of it. README.md has the placeholder-renaming checklist, the wiring table and the API table.

## Commands

The tooling comes from the Nix dev shell (`nix develop ./nix`). It bootstraps PlatformIO into `.venv/` and sets `PLATFORMIO_CORE_DIR=.cache/platformio`. Its quick commands run from the repo root wherever you call them:

| Command | Does |
|---|---|
| `build-ui` | `pnpm install && pnpm run deploy` in `web/`: builds and copies `web/build/` → `data/` |
| `flash-ui` | `build-ui` + `pio run -t uploadfs` |
| `flash` / `flash-monitor` | `pio run -t upload` (+ `pio device monitor`) |
| `flash-all` | UI + filesystem + firmware + monitor. Use this on a new board |

Other useful commands:

- Compile the firmware without flashing: `pio run` (in CI, `pio run -e xiao_esp32s3`). Build the filesystem image: `pio run -t buildfs`.
- Type-check the web UI: `cd web && pnpm check`.
- Run the web dev server: `cd web && pnpm dev`. `/api` is proxied to `http://$DEVICE_HOST`, which defaults to `esp32.local`.
- Format C++ with `clang-format -i src/**/*.{h,cpp}` (config in `.clang-format`).

There is no test suite for the firmware or the UI. Verify a change by compiling (`pio run`, `pnpm check`) and, when a device is attached, by flashing it and reading the serial log.

`data/` is generated and gitignored. The firmware's static files only change on the device after `flash-ui` / `uploadfs`, not after `flash`.

## Architecture

**Control flow (`src/main.cpp`).** `setup()` runs in a fixed order, and the order matters:
1. `sleep_pins::release()` comes first, because pad latches from the last sleep survive the wake reset.
2. Then `SETTINGS.load()`, then generating the hotspot password on first boot.
3. On an ext0 wake, wait for the button to be released, so that release isn't read as a fresh press.
4. Then `radio.begin()` → `joinNetwork()` or `startHotspot()`.
5. `LoopWatchdog::begin()` comes last.

`loop()` feeds the watchdog, polls the button (press = activity, hold = sleep), services the radio, and calls `enforceIdleSleep()`.

**Idle sleep and activity.** The idle timer is moved forward by button presses and by `ConfigWebServer::lastRequestMs()`. A new route handler must set `lastRequest = millis()`, as `handleHealth`/`handleNotFound` do, or requests to it won't count as activity and the device can fall asleep while it is in use.

**Deep sleep (`enterDeepSleep()`).** It stops every driver (currently only `radio.off()`), then calls `sleep_pins::hold()`, re-arms the button's pull-up through the RTC domain, and arms ext0 wake. Each new peripheral needs two changes: shut it down here before `hold()`, and add its control/CS lines to `kParked` in `src/power/SleepPins.cpp`. The wake button must be an RTC GPIO (0–21).

**LoopWatchdog.** `loopTask` is subscribed to the ESP-IDF TWDT with `TIMEOUT_S = 15`. Each `loop()` pass must finish within that. Code that is meant to block longer takes a scoped guard:
- `LoopWatchdog::Pause` turns coverage off. Examples are `Radio::joinNetwork()`, which allows 12 s per SSID, and `ConfigWebServer::loop()`, which is bounded by TCP timeouts.
- `LoopWatchdog::Stretch(seconds)` widens the timeout for long work that still feeds regularly, such as an OTA install.
A watchdog reboot shows up as reset reason 6 in the boot log.

**Networking (`src/network/`).**
- `Radio` owns the WiFi mode and everything that only exists while the radio is up: the `ConfigWebServer`, mDNS, and a port-443 listener that closes HTTPS attempts at once, so browsers don't stall. `WiFi.persistent(false)` is set on purpose, so that `Settings` is the only store of credentials.
- `WifiConnector` tries the configured networks in order.

**Web server (`ConfigWebServer`).** API routes are registered in `begin()`; document new ones in the header comment and in the README's API table. For any non-`/api/` path, the server tries `<path>` (a trailing `/` maps to `index.html`), then `<path>.html`. If none exists, it redirects to `/` in hotspot mode (captive-portal style) and otherwise serves the SPA fallback `200.html`. Unknown `/api/*` paths return an honest 404. CORS is enabled. `/_app/immutable/*` is served with long cache headers. Add new file types to `contentTypeFor()`.

**Settings.** `SETTINGS` is an NVS-backed singleton (`Preferences`, namespace `settings`). WiFi networks are stored as a JSON string (`[{"ssid","password"}]`) and parsed by `wifis()`. To add a field, add it to the class and to both `load()` and `save()`.

**Web UI (`web/`).** The UI uses SvelteKit with `adapter-static` (`fallback: '200.html'`), `prerender = true`, and runes mode forced on. Nothing runs server-side on the device, so everything must prerender or render on the client. It is served from LittleFS, whose flash space is limited.

**Versioning and releases.** `FIRMWARE_VERSION` in `src/Version.h` is a default for dev builds. Pushing a `v*.*.*` tag runs `.github/workflows/release.yml`, which injects the tag through `PLATFORMIO_BUILD_FLAGS`, builds the firmware and `littlefs.bin`, and publishes a GitHub Release. Dependabot does not cover `platformio.ini`; update its `lib_deps` by hand.

## Conventions

- C++20 (`-std=gnu++2a`), no exceptions in app code, 2-space indent, 120 columns.
- One `PascalCase` class per file under `src/<area>/`, `camelCase` methods and members, `UPPER_SNAKE_CASE` constants. Constants local to a file are `kCamelCase` inside an anonymous namespace.
- Serial log lines are prefixed with a `[Tag]` (`[Main]`, `[Radio]`, `[Web]`, `[Power]`, …).
- Comments explain *why*: hardware quirks and invariants. Keep that density when changing nearby code.
- `platformio.ini` sets `monitor_rts = 0` / `monitor_dtr = 0` so the XIAO doesn't stay held in reset while the monitor is attached. Don't remove them.
