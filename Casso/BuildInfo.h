#pragma once

// Build identity for the window caption / diagnostics: version, target
// architecture, and the running exe's link time (read from the .exe file at
// runtime, so it is always the actual build the user is running -- never stale
// after an incremental relink). Shown in the caption so a running instance
// names the binary it is at a glance. No flavor tag: the caption carries this
// only on a _DEBUG build, so its presence already says Debug.

// e.g. "v1.10.0 x64 (Jul 19 2026 08:41:02)"
const wchar_t *  CassoBuildInfo ();
