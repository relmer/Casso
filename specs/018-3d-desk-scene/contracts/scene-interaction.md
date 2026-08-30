# Contract: Scene Interaction Parity (UX)

The behavioral contract the 3D scene must honor. The reference implementation is the
current 2D skeuo chrome; these are binding regardless of how rendering changes.

## Drive interactions (FR-004..FR-007)

| Event | Behavior (identical to 2D band) |
|---|---|
| Click slot/eject region | `Eject(6, drive)` then `BrowseForDisk(drive)` — existing routing at `EmulatorShell::OnLButtonUp` |
| Click drive body | `BrowseForDisk(drive)` — disk stays mounted |
| Hover drive | Tooltip: mounted image name; write-protect text from `ComposeWriteProtectTooltip` **verbatim** |
| Disk activity | Activity LED lit while `motorOn`/`diskActive`; no perceivable lag vs today |
| Mount/eject | Door animation (350 ms FSM), loaded/empty visibly distinct |
| Write-protected disk | Same padlock-style visual cue concept + tooltip as today |
| Dead space between devices | No tooltip, no click action |

Command routing stays on `IDriveCommandSink` / existing `EmulatorShell` handlers; the
scene only changes *how hits are found*, never what they do.

## Display input (FR-002)

- Mouse mode (non-capturing, absolute): pointer over glass → inverse-projected emulated
  pixel → `AppleMouse::SetHostTargetFraction`; leaving glass → `ClearHostTarget()`.
  Cursor hidden over glass only while guest mouse live (parity with `OnSetCursor`).
- Letterbox fix: only actual picture area maps; this *intentionally replaces* the current
  viewport-rect mapping (bars no longer count as picture).
- Paddle mode: capture/relative behavior untouched (chrome never sees moves while
  captured — existing bail in `OnMouseMove`).
- Button gating: press requires pointer on glass (parity with viewport gating); release
  never gated.

## Monitor (FR-008)

- Power lamp reflects machine running state; brand mark in the chin position (both are
  model features — lamp tint driven by state).

## Theme switching (FR-009, FR-010, SC-004, SC-005)

- Non-skeuo themes: pixel-identical to current release (capture comparison).
- Switch either direction: < 1 s, emulation uninterrupted, scene state (picture, disks,
  activity, WP) correct on arrival. Window-size delta behavior follows the existing
  band-thickness rules until the band is retired, then the scene's own layout owns it.

## Fullscreen (FR-014, FR-015)

| State | Behavior |
|---|---|
| Enter fullscreen (skeuo) | Glass-only camera; all chrome bands hidden; curvature + input mapping intact |
| Host owns pointer | Bottom-edge dwell reveals drive strip; leave → auto-hide after grace |
| Guest owns pointer (mouse/paddle) | Edge-reveal disabled; strip hotkey releases capture, shows strip; dismiss restores capture exactly as it was |
| Strip shown | Full drive interaction parity (table above) |
| Tooltip or browse open from strip | Strip cannot auto-hide |
| Strip hidden + drive active | Unobtrusive activity indicator |
| Disk menu | Unchanged, available as today |
| Leave fullscreen | Windowed desk scene restored; emulation uninterrupted |

## Commands & prefs

- Ctrl+0 / reset-to-100%: window sizes so the glass is native scale (SceneCamera inverse).
- `skeuoMonitorFrame` pref + Settings toggle retired at supersession; stale key ignored.
- Strip hotkey: new command through the standard accelerator path; shown in menu with
  accelerator text (discoverability).
