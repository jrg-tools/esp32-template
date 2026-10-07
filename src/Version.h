#pragma once

// Single source of truth for the firmware version. Overridden at build time
// via -D FIRMWARE_VERSION=\"vx.y.z\" (see .github/workflows/release.yml)
// with the pushed tag verbatim; this default covers local/dev builds.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v0.1.0"
#endif
