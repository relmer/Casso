#pragma once

// Build identity for the window caption / diagnostics: version, target
// architecture, build flavor (Debug/Release), and the running exe's link time
// (read from the .exe file at runtime, so it is always the actual build the
// user is running -- never stale after an incremental relink). Shown in the
// caption so a running instance names the binary it is at a glance.

// e.g. "v1.10.0 x64 Debug (Jul 19 2026 08:41:02)"
const wchar_t *  CassoBuildInfo ();
