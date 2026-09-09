# Validation Guide: Thin Executable, Testable Core

**Feature**: 031-thin-exe-shim | **Spec**: [spec.md](spec.md)

How to prove a slice is done. Run the per-slice gate at every slice boundary and
the final gate once, before asking for the merge to master.

## Prerequisites

- Visual Studio 2026 / MSBuild, Windows SDK, PowerShell 7 (`pwsh`).
- Build through `Casso.sln`, never the `.vcxproj` directly, or the executable
  lands in `Casso\Casso\x64\Debug` and a stale solution-directory binary is what
  gets tested.
- x64 Debug and Release are the acceptance bar for test execution. ARM64 must
  compile; it is not run, and waiting on it is not part of any gate.

## The per-slice gate

Run all six checks at the end of every slice. All six must pass before the slice
is committed and pushed.

### 1. Build both configurations

```powershell
pwsh scripts/Build.ps1 -Configuration Debug -Platform x64
pwsh scripts/Build.ps1 -Configuration Release -Platform x64
```

If the slice added or removed a virtual function, use `-Target Rebuild`.
Incremental builds leave stale objects after a vtable change, which surfaces as
Release-only deterministic test failures with empty error messages.

### 2. Confirm the binaries are actually new

`RunTests.ps1` does not build. Check that `UnitTest.dll` and `Casso.exe` have a
`LastWriteTime` newer than the build before trusting any result — a stale DLL
reports a confident green.

### 3. Run the suite

```powershell
pwsh scripts/RunTests.ps1
```

Green in Debug and Release (SC-006). Release additionally verifies that no
`EhmAssert` fired.

### 4. Style

```powershell
pwsh scripts/CheckStyle.ps1 -Mode Tree
```

The pre-push hook only checks lines added in the pushed range, so it cannot see
a `////` banner orphaned by an edit to its surroundings. Slices that relocate
functions are exactly the edits that orphan banners, so run the tree sweep
rather than relying on the hook.

### 5. Record the measurement

Every slice records, in its commit message (FR-012, FR-013, SC-007):

- `Casso` project lines before and after.
- Receiving library lines before and after.
- Dual-compile entries remaining in `UnitTest.vcxproj` (38 at branch point,
  zero at the end).
- What Principle VI verification found: which project now holds the moved code,
  what stayed in the exe, and why. A bare PASS is not a check.

Counting the remaining dual-compile entries:

```powershell
(Select-String -Path UnitTest/UnitTest.vcxproj -Pattern 'ClCompile Include="\.\.\\Casso\\').Count
```

### 6. Behavior is unchanged

The suite does not cover what a Settings page looks like. For slices 1, 4 and 5,
launch the emulator and confirm the affected surface behaves as before.

```powershell
./x64/Debug/Casso.exe --title 031-thin-exe-shim
```

Always pass `--title` so the caption says which session the window came from.
Launch in the background; an unrequested run must not steal focus.

## Per-story checks

| Story | What proves it |
|---|---|
| 0 | Debug and Release green; diff shows no content change beyond include paths and header guards; no file under `Devices/`, `Video/` or `Audio/` assumes one machine; every model directory matches one under `Resources/Machines/` |
| 1 | Preference merge, corrupt-input recovery and placement restore assert against a synthetic in-memory file system; no disk, no registry, no window |
| 2 | Each moved stray has a test file constructing it from synthetic data |
| 3 | Viewport-to-client round trip returns the original size at every supported DPI; the render gate declines an unchanged screen and re-rasterizes when any one input changes; a single connected drive lays out centered |
| 4 | A debug panel re-attaches to the new controller and audio source across a machine switch; MRU de-duplicates without evicting; an encoded screenshot decodes to the same pixels |
| 5 | A dependent control's enablement follows its governing value with no painter; every desk-scene rectangle falls inside the scene bounds at every supported DPI |
| 6 | A machine builds headlessly; soft reset preserves user RAM and takes the reset vector; power cycle re-seeds every DRAM-owning device first; a single step retires exactly one instruction |
| 7 | A synthetic framebuffer through the full pass chain matches its golden pixel-exact; a parameter change moves the image in the documented direction; a mixed span matches expected samples including at the clipping boundary; two runs are identical |
| 8 | Both executable projects define zero functions; both link and start in Debug and Release |

## The final gate

Run once, when every slice is done, before asking for the merge.

1. The per-slice gate, clean.
2. Both executables define **zero** functions, entry point included (SC-002).
   Each project holds only a resource script, a resource header, and a
   comment-only translation unit.
3. `EntryPointSymbol` is set in every configuration of both projects, and both
   executables start.
4. `UnitTest.vcxproj` has no `ProjectReference` to `Casso.vcxproj`, no `..\Casso`
   include directory, and zero `..\Casso\*` `ClCompile` entries.
5. `CassoCore` still has no include reaching into `CassoEmuCore`, `Dxui` or an
   executable (FR-005b).
6. ARM64 compiles in Debug and Release.
7. `CHANGELOG.md` carries entries for defects fixed and user-visible changes
   only, never for the extraction itself (FR-015).
8. Final measurements are recorded so issue #85 can be closed against them
   (FR-016).

## Known hazards

- **Precompiled headers.** The 38 dual-compiled sources build against
  `..\Casso\CassoPch.cpp` today. Moving a file changes which PCH it sees; this
  is the likeliest per-slice build breakage.
- **Sweeping renames on master.** Four have landed since August 2026, and they
  merge cleanly and then fail to compile, so the conflict count badly
  understates the work. Merge master into this branch at every slice boundary at
  least, so a rename is absorbed one slice at a time.
- **Shader build.** `Shaders.targets` is imported at `Casso.vcxproj:473` and its
  paths are relative to the importing project. Moving `Shaders/` moves the
  import and probably needs the targets file adjusted.
- **CI code analysis.** CI builds with `EnableCppCoreCheck` and
  `TreatWarningsAsErrors`. `Build.ps1 -RunCodeAnalysis` does not reproduce it, so
  a local run is not evidence that the CI gate passes.
