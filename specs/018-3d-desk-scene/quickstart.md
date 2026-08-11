# Quickstart: Validating the 3D Desk Scene

Prerequisites: Windows 11, VS 2026, repo at this branch. Build **via the solution** (never
the vcxproj directly) and confirm `Casso.exe` LastWriteTime is newer than the build start
before trusting any run.

## Build + unit gates

```powershell
# x64 Debug build + full test suite (also run Release before merge)
# VS Code task: "Build + Test Debug"   (or the scripts under scripts/)
```

Expected: all tests green, including the new
`CurvedDisplayMathTests` / `SceneCameraTests` / `DeskSceneLayoutTests` /
`DeskSceneModelTests` / `DeskSceneHitTesterTests` / `FullscreenStripStateTests`.

## US1 — curved display (P1)

1. Launch Casso (default //e), Skeuomorphic theme: 3D Monitor //c renders with the live
   picture on curved glass; power lamp lit; brand mark on chin.
2. Boot MousePaint (or any mouse-mode title): clicks/drags land where aimed across the
   whole display — specifically test all four corners and edge extremes.
3. Resize the window through extreme aspect ratios; move across monitors with different
   scaling: scene contained, proportions correct, input accuracy unchanged.
4. Ctrl+0: window sizes so the picture is native scale.
5. Capture validation: `scripts/CaptureScreenshotMatrix.ps1` (PrintWindow →
   CopyFromScreen fallback) — judge the gestalt against the Monitor //c reference photos.

## US2 — drive parity (P2)

1. Boot a DOS 3.3 disk: drive 1 activity LED flashes during boot, dark when idle.
2. Hover each drive: tooltip names the mounted image; write-protect a disk
   (Disk menu) and confirm the tooltip wording matches the 017 strings verbatim.
3. Click drive slot region: disk ejects + browser opens. Click drive body: browser opens,
   disk stays mounted. Click dead space between devices: nothing.
4. Eject/insert: door animates (~350 ms); loaded vs empty visibly distinct.
5. Machine with one drive configured: only one drive object appears.

## US3 — themes (P3)

1. Cycle Skeuomorphic → DarkModern → RetroTerminal → Skeuomorphic with a machine running:
   each switch < 1 s, emulation never pauses, scene state correct on return.
2. Capture DarkModern/RetroTerminal before+after this feature: pixel-identical.

## Fullscreen (FR-014/FR-015)

1. Alt+Enter in skeuo: glass fills the screen with curvature; no chrome.
2. Host-pointer mode: push pointer to bottom edge → strip slides in; interact per US2;
   move away → auto-hides. Hover a tooltip: strip stays while tooltip shows.
3. Boot a mouse-mode title, capture active: edge push does NOT reveal; strip hotkey does
   (capture released); dismiss strip → capture restored (game input resumes cleanly).
   Repeat in paddle mode.
4. While strip hidden, trigger disk activity: unobtrusive indicator visible.
5. Alt+Enter back: windowed desk scene restored, emulation uninterrupted.

## Perf sanity (SC-002)

1. Idle at the BASIC prompt (static screen): presents are skipped (no continuous GPU work
   — check with PresentMon or GPU busy% roughly matching the current release).
2. During boot animation + door animation: smooth, no hitching vs current release.

## Merge gates (project standard)

x64 Debug + Release full suite green · ARM64 builds · Code Analysis zero warnings ·
CheckStyle clean · non-skeuo capture comparison recorded.
