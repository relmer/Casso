#pragma once

// Build identity for the window caption / diagnostics: version, target
// architecture, and the running exe's link time (read from the .exe file at
// runtime, so it is always the actual build the user is running -- never stale
// after an incremental relink). Shown in the caption so a running instance
// names the binary it is at a glance.
//
// This string appears on _DEBUG builds only, but do NOT treat its presence as
// the flavor marker: nobody reads "v1.17.0 x64 (...)" and thinks "debug". The
// caption carries an explicit [Debug] tag for that (EmulatorShell::UpdateTitle).

// e.g. "v1.10.0 x64 (Jul 19 2026 08:41:02)"
const wchar_t *  CassoBuildInfo ();
