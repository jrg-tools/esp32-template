# ESP32 Template

A template repository for ESP32-S3 firmware projects that come with a web UI. It targets the Seeed XIAO ESP32-S3 and uses the Arduino framework on PlatformIO. It has no application logic: the device boots, gets on a network, serves a health-check page, and goes to sleep when nobody is using it. Your project is built on top of that.

It ships with:

- **Networking**: on boot the device joins the first configured WiFi network that answers. If none does, it starts its own hotspot. It is reachable at `http://esp32.local/` over mDNS.
- **Web server**: a JSON API (`GET /api/health`) plus a static SvelteKit UI served from LittleFS.
- **Sleep mode**: deep sleep after 5 minutes with no button press and no web request, or right away when the button is held. The same button wakes the device.
- **Loop watchdog**: reboots the device if `loop()` stops making progress.
- **Nix dev shell**: PlatformIO, Node 24, pnpm and clang-format, plus one-word flash commands.
- **Release workflow**: pushing a `v*.*.*` tag builds the firmware and filesystem images and publishes a GitHub Release.
- **Dependabot**: weekly update PRs for the Nix flake inputs, the web UI's npm packages and GitHub Actions. PlatformIO isn't supported by Dependabot, so update `platformio.ini` by hand.

## Starting a new project

1. Create a repository from this template: click **Use this template** on GitHub, or copy the files.
2. Rename the placeholders. They all default to `esp32`:

   | Where | What |
   |---|---|
   | `src/Settings.h` | `apSsid` (hotspot name) and `DEFAULT_HOSTNAME` (the `.local` name) |
   | `src/Settings.cpp` | `NVS_NAMESPACE`, if several projects might share a board |
   | `web/vite.config.ts` | default `DEVICE_HOST` for the dev proxy |
   | `web/src/routes/+page.svelte` | page title and heading |
   | `nix/flake.nix` | `description` and the `esp32-shell` FHS env name |
   | `README.md` | this file |

3. If you use a different board, change `board` and `board_build.arduino.memory_type` in `platformio.ini`.
4. Enter the dev shell, run `flash-all`, and check that `http://esp32.local/` (or the hotspot at `192.168.4.1`) shows the health page.
5. Build your project on top:
   - **Firmware features:** add them as classes under `src/<area>/`, call them from `setup()` and `loop()`, and shut them down in `enterDeepSleep()`.
   - **API endpoints:** add routes in `ConfigWebServer::begin()`.
   - **UI pages:** add them under `web/src/routes/`.

## Wiring

| Pin | Use |
|---|---|
| D1 (GPIO2) | Button to GND. A press counts as activity; a hold (1.5s) puts the device to sleep. It also wakes the device from deep sleep |

Any pin in the RTC range (GPIO0-21) can be the wake button. Change `kButtonPin` in `src/main.cpp`.

## Repository layout

```
src/
  main.cpp                 boot, loop, idle/deep sleep
  Settings.{h,cpp}         NVS-backed settings (WiFi networks, hotspot, mDNS name)
  Version.h                firmware version (overridden from the tag in CI)
  LoopWatchdog.{h,cpp}     task watchdog on loop()
  input/Button             debounced button with hold gesture
  network/WifiConnector    tries each configured network in turn
  network/Radio            station/hotspot, mDNS, web server lifecycle
  network/ConfigWebServer  /api/* routes + static UI from LittleFS
  power/SleepPins          parks peripheral pins during deep sleep
web/                       SvelteKit UI (static build -> data/ -> LittleFS)
nix/                       dev shell (flake + shell.nix)
.github/workflows/         tagged release build
```

## Development

### Environment

The Nix dev shell provides everything: PlatformIO, Node 24, pnpm and clang-format.

```sh
nix develop ./nix
```

The first time you enter the shell, it creates a `.venv` with PlatformIO Core. It works on Linux (through an FHS env) and on macOS. Without Nix you need PlatformIO Core, Node.js >= 24 and pnpm.

### Quick commands (inside the shell)

| Command | What it does |
|---|---|
| `build-ui` | Build the SvelteKit UI and stage it into `data/` |
| `flash-ui` | `build-ui` + flash the LittleFS partition |
| `flash` | Flash the firmware |
| `flash-monitor` | Flash the firmware + attach the serial monitor |
| `flash-all` | UI + filesystem + firmware + monitor (full deploy) |

On a new board, run `flash-all` first.

### Configuring WiFi

Networks live in `Settings::wifiNetworks` as a JSON array (`[{"ssid":"...","password":"..."}]`), and the list starts empty. With no networks configured, the device starts the `esp32` hotspot. Its password is generated on first boot and printed to the serial log. To get it onto your network, either set the default in `src/Settings.h` or add an API route that writes it.

### Working on the web UI

```sh
cd web
pnpm dev
```

`/api/*` calls are proxied to a real device. The default host is `esp32.local`; override it with `DEVICE_HOST=192.168.4.1 pnpm dev`.

### Web API

| Route | Response |
|---|---|
| `GET /api/health` | `{status,version,ip,mode,rssi,freeHeap,uptime}` |

Add routes in `ConfigWebServer::begin()`. Any non-API path is served from LittleFS. A path with no matching file gets the SPA fallback (`200.html`); in hotspot mode it is redirected to `/` instead.

### Sleep

`enterDeepSleep()` in `src/main.cpp` stops every driver, then parks pins (`power/SleepPins.cpp`), then arms the ext0 wake on the button. When you add a peripheral:

1. Shut it down in `enterDeepSleep()` before `sleep_pins::hold()`.
2. Add its chip select / control lines to `kParked` in `power/SleepPins.cpp`, so they don't float against a powered rail.

### Releasing

```sh
git tag v0.2.0 && git push --tags
```

The tag is compiled in as `FIRMWARE_VERSION` and reported by `/api/health`.

### Conventions

- C++20, no exceptions in app code, 2-space indent, 120 columns (`.clang-format`)
- `PascalCase` classes (one per file), `camelCase` methods/members, `UPPER_SNAKE_CASE` constants
- `loop()` must never block longer than `LoopWatchdog::TIMEOUT_S`. Calls that are meant to block take a `LoopWatchdog::Pause`.
