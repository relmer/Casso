# Implementation Plan: 007 UI Overhaul (Native Reset)

**Branch**: `007-ui-overhaul`  
**Date**: 2026-05-22  
**Spec**: `specs/007-ui-overhaul/spec.md`

## Summary

Reset 007 to a native-owned UI pipeline for all end-user-visible chrome and
settings surfaces. Remove all legacy alternate ownership and keep one
deterministic render/input path.

The user-visible target remains unchanged:

1. Cohesive custom window/chrome appearance.
2. Realistic Disk ][ drive icons for drive widgets.
3. Visible door-open/door-close animations synchronized with drive sounds.
4. Theme variants for Apple II / II+ / IIe / //c now.
5. Extensible theme-family model for future non-Apple systems.

## Technical Context

- Language/Version: C++ (MSVC v145, `/std:c++latest`)
- Rendering: D3D11 geometry + DirectWrite text
- Input/Windowing: Win32 + existing NC hit-test model
- Storage: existing machine/global JSON + theme metadata JSON
- Testing: CppUnitTest + runtime screenshot verification
- Platforms: Windows 10/11 x64 + ARM64

## Ownership Model

Single ownership only:

- Native renderer owns title bar, nav/menu, settings panel chrome, and drive widgets.
- Native input routing owns chrome hit-testing and command dispatch.
- No alternate runtime ownership path is allowed.

## Phases

### P0 - Contract reset

1. Remove legacy alternate-contract assumptions from spec artifacts.
2. Publish native theme metadata contract updates.
3. Lock acceptance criteria around drive realism and sync behavior.

### P1 - Native chrome/nav stabilization

1. Title/nav rendering and command routing on one path.
2. Win11-style font policy for title/nav text.
3. NC behavior parity (drag/min/max/close/snap).
4. Remove previously added legacy UI runtime plumbing from `Casso` code.
5. Remove legacy UI dependency/build wiring from solution/projects.
6. Delete obsolete legacy UI classes/resources/tests added in the prior attempt.
7. Retire remaining Win32 settings dialogs so D3D settings ownership is complete.
8. Implement keyboard-only settings interaction + visible focus behavior.

### P2 - Drive realism and synchronized animation

1. Disk ][ visual profile for drive icon geometry and texture.
2. Door open/close state machine tied to user actions.
3. Sound-animation synchronization contract (shared event + bounded skew).

### P3 - Theme breadth (Apple variants)

1. First-class variants for Apple II, II+, IIe, //c.
2. //c variant includes distinct color treatment and distinct drive style.
3. Visual differences may be color/texture-first initially, but drive shape
   must still read as Disk ][ where required.
4. Ensure runtime theme discovery, malformed-theme exclusion, and built-in re-extract behavior.

### P4 - Future family extensibility

1. Theme family/variant schema (`familyId`, `variantId`) finalized.
2. Non-Apple families can be added without schema redesign.

### P5 - Validation hardening

1. Add/expand UT coverage for chrome/nav ownership, NC hit-testing, drive
   animation state transitions, and sync-event timing constraints.
2. Enforce UT isolation: use only mocks/fakes/in-memory adapters for
   filesystem/registry/environment dependencies; no host-state side effects.
3. Launch app and capture screenshots for a fixed validation matrix
   (startup, open menu/dropdown, NC controls visible, settings open, drive door
   open/closed states, and per-variant theme snapshots).
4. Review screenshots against spec acceptance behavior to catch visual
   regressions (missing menus, missing NC components, broken chrome geometry).
5. Validate persistence paths: mounted-disk auto-remount and per-monitor window placement.
6. Run required Code Analysis gate before implementation signoff.

## Acceptance Criteria

1. Title/nav text uses Windows-system-identical UI font definition.
2. Drive icons visibly read as Disk ][ (not generic placeholders).
3. Insert/eject actions show matching door-close/door-open animations.
4. Door animation and drive sound stay synchronized (<= 1 frame skew).
5. Apple II / II+ / IIe / //c theme variants all render correctly.
6. //c variant uses distinct palette and drive style.
7. No chrome surface has dual ownership at runtime.
8. No legacy UI runtime initialization/render/input path remains in code.
9. No legacy UI dependency or build reference remains in solution wiring.
10. UT suite covers critical native UI behavior and sync constraints.
11. Screenshot validation matrix passes with no missing menu or NC components.
12. UT execution is side-effect free with no real registry/filesystem mutation.
13. CRT brightness/effect controls and per-theme defaults/user overrides work end-to-end.
14. Mounted-image auto-remount and per-monitor window-placement persistence are verified.

## Risks

1. Legacy docs/tasks drift from reset direction.
   - Mitigation: reset core artifacts and remove obsolete contracts/tasks.
2. Visual fidelity disagreements for "realistic Disk ][".
   - Mitigation: define concrete visual profile + screenshot baselines.
3. Animation/sound drift under load.
   - Mitigation: central sync event model and timing assertions.
