#pragma once

// Build identity for the window caption / diagnostics: version, target
// architecture, build flavor (Debug/Release), and the compile timestamp of
// THIS build. The backing translation unit (BuildInfo.cpp) is force-touched by
// a pre-build step so its __DATE__/__TIME__ reflect the actual link, not a
// stale incremental compile -- a running instance's caption then tells you at a
// glance exactly which binary it is.

// e.g. "v1.10.0 x64 Debug (Jul 19 2026 08:41:02)"
const wchar_t *  CassoBuildInfo ();
