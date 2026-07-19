#pragma once

// Build identity for the window caption / diagnostics: version, target
// architecture, build flavor (Debug/Release), and the compile timestamp of
// BuildInfo.cpp (updated whenever it recompiles -- a clean build, or a change
// to it or Version.h). Shown in the caption so a running instance names the
// binary it is at a glance.

// e.g. "v1.10.0 x64 Debug (Jul 19 2026 08:41:02)"
const wchar_t *  CassoBuildInfo ();
