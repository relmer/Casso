# Feature Specification: 3D Desk Scene (Parity Phase)

**Feature Branch**: `desk-scene-models`

**Created**: 2026-08-10

**Status**: Draft

**Input**: User description: "Replace the skeuomorphic theme's 2D monitor frame and drive band with a real-time 3D desk scene rendering the checked-in parametric models (Monitor //c + two Disk II drives for now). The emulator display becomes a texture mapped onto the monitor model's spherical-sag glass mesh so the picture has period-correct curvature. Mouse/touch input over the curved display must inverse-project to the correct emulated pixel. Feature-parity with today's skeuo theme is the bar. Compact/dark-modern/retro themes keep their existing simple 2D widgets, untouched."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Curved-glass monitor with the live picture (Priority: P1)

A user running the skeuomorphic theme sees a period-correct Apple Monitor //c on their desk instead of today's flat 2D monitor frame. The emulated computer's picture appears on the monitor's glass, gently bulging toward the viewer the way a real CRT tube does. Everything the user does with the display today still works: they point, click, and type into the emulated software, and their input lands exactly where they aimed — even near the curved edges of the glass.

**Why this priority**: The monitor is the centerpiece of the scene and the only element the user stares at continuously. If the curved live display isn't convincing and input-accurate, nothing else in the scene matters. It is also the riskiest element, so it must prove out first.

**Independent Test**: Can be fully tested by switching to the skeuomorphic theme with no drives visible: the 3D monitor renders with the live emulated picture on curved glass, software that uses a pointing device responds to clicks at the correct emulated screen locations across the entire display including corners and edges, and the power lamp reflects the machine's running state.

**Acceptance Scenarios**:

1. **Given** the skeuomorphic theme is active and a machine is running, **When** the user looks at the emulator window, **Then** the picture is displayed on the 3D monitor's curved glass with visibly period-correct curvature and no part of the picture cut off or doubled.
2. **Given** a program that tracks a pointing device is running, **When** the user clicks a target near a corner of the curved display, **Then** the click registers at the same emulated location it would have on the flat display (within one emulated pixel).
3. **Given** the emulated machine is running, **When** the user watches the monitor's chin, **Then** the power lamp is lit, and the brand mark appears where the physical monitor carries it.
4. **Given** the user resizes the emulator window or moves it to a display with different scaling, **When** the scene re-renders, **Then** the monitor stays fully visible, correctly proportioned, and input accuracy is unchanged.

---

### User Story 2 - 3D disk drives with full drive interaction (Priority: P2)

The user sees their two disk drives as 3D Disk II units on the desk below/beside the monitor. Everything the 2D drive band does today carries over: the in-use light glows during disk activity, the drive shows whether a disk is in it, hovering explains the drive's state (including why a disk can't be written, exactly as the write-protect messages read today), clicking the slot area ejects and browses for a disk, and clicking the drive body browses while keeping the current disk mounted.

**Why this priority**: Drives are the primary interactive objects and the main reason the desk scene exists (future drive types dock into it). But they only make sense once the monitor scene (US1) is in place to host them.

**Independent Test**: Can be fully tested by booting a disk in the skeuomorphic theme: activity light flashes during boot, hover tooltips report the mounted image and write-protect state with today's exact messaging, slot-click ejects and opens the disk browser, body-click opens the browser with the disk still mounted.

**Acceptance Scenarios**:

1. **Given** a disk is booting, **When** the drive reads, **Then** that drive's in-use light glows in time with activity and goes dark when activity stops.
2. **Given** a drive has a mounted disk, **When** the user hovers over the drive, **Then** the tooltip names the mounted image and, if applicable, the write-protect explanation matches the current 2D band's wording exactly.
3. **Given** a drive has a mounted disk, **When** the user clicks the drive's slot area, **Then** the disk is ejected and the disk browser opens; **When** the user instead clicks the drive body, **Then** the browser opens and the disk stays mounted, exactly matching today's behavior.
4. **Given** a drive is empty versus loaded, **When** the user looks at the drive, **Then** the drive's front visibly distinguishes the two states.

---

### User Story 3 - Theme switching stays seamless and other themes stay untouched (Priority: P3)

A user can switch between the skeuomorphic theme and the compact, dark-modern, and retro themes at any time. The other themes look and behave exactly as they do today — simple 2D widgets, no 3D scene — and switching in either direction is quick and leaves the emulated machine undisturbed.

**Why this priority**: Protects existing users who prefer the lightweight themes and guarantees the 3D work is additive, not a regression. It's a gate on shipping, but it has no standalone user value until US1/US2 exist.

**Independent Test**: Can be fully tested by cycling through all themes while a machine runs: each non-skeuomorphic theme renders identically to the previous release, the machine never pauses or glitches during a switch, and returning to skeuomorphic restores the 3D scene.

**Acceptance Scenarios**:

1. **Given** a machine is running in the skeuomorphic theme, **When** the user switches to compact/dark-modern/retro, **Then** those themes render their existing 2D widgets unchanged and the machine keeps running without interruption.
2. **Given** any non-skeuomorphic theme is active, **When** the user switches to skeuomorphic, **Then** the 3D desk scene appears with the live picture already on the glass and all drive state (mounted disks, activity, write-protect) correctly reflected.

---

### Edge Cases

- Pointer at a glancing angle to the glass: clicks at the extreme curved edge must still map to the outermost emulated pixels, never "miss" the screen while visually over it.
- Pointer between objects (desk background, gaps between monitor and drives): hover/click over dead space does nothing — no phantom drive tooltips or click actions.
- Window made very small or very wide: the scene letterboxes/scales gracefully; the display remains legible and input mapping stays correct at any window size.
- Display scaling (DPI) changes mid-session or differs across monitors: scene proportions and input accuracy are unaffected.
- Drive activity light during sustained fast access: the light must reflect activity without flickering artifacts or lagging noticeably behind the audible drive sounds.
- Machine type with a single drive or no drives configured: the scene shows only the devices the machine actually has.
- Screenshots/window captures of the emulator: the captured image shows the full composed scene as the user sees it.
- Fullscreen mode: entering and leaving fullscreen preserves the scene, its proportions, and input accuracy.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: In the skeuomorphic theme, the system MUST present the emulated display on a 3D Apple Monitor //c model whose screen surface carries period-correct spherical curvature, replacing the current 2D monitor frame.
- **FR-002**: The system MUST map pointing-device input (mouse, touch, pen) over the curved display to emulated screen coordinates such that any visible emulated pixel can be hit, with accuracy within one emulated pixel of the equivalent flat-display mapping, across the entire glass including corners and edges.
- **FR-003**: The system MUST render the machine's disk drives as 3D Disk II models in the scene, showing one drive per configured drive, and no drive objects for machines without them.
- **FR-004**: Each 3D drive MUST show disk activity via its in-use light with no user-perceivable lag relative to today's 2D drive band.
- **FR-005**: Each 3D drive MUST visibly distinguish loaded versus empty state.
- **FR-006**: Hovering a 3D drive MUST show the same tooltip content the 2D drive band shows today, including the mounted image name and the write-protect explanations introduced in the blank-disk-creation feature, with identical wording.
- **FR-007**: Clicking a 3D drive's slot region MUST eject the mounted disk and open the disk browser; clicking the drive body MUST open the disk browser while keeping the current disk mounted — matching the 2D band's click regions in behavior.
- **FR-008**: The 3D monitor MUST show a power lamp reflecting the machine's power/running state and carry the product branding in the position the 2D skeuomorphic frame shows it today.
- **FR-009**: The compact, dark-modern, and retro themes MUST be visually and behaviorally unchanged from the current release.
- **FR-010**: Switching between the skeuomorphic theme and any other theme MUST complete without pausing, resetting, or visibly glitching the running emulated machine, and the 3D scene MUST reflect current machine state (picture, mounted disks, activity, write-protect) immediately on arrival.
- **FR-011**: The scene MUST adapt to window resizing, display-scaling (DPI) changes, and fullscreen transitions while preserving proportions, legibility, and input accuracy.
- **FR-012**: Window captures of the emulator MUST contain the composed desk scene as displayed.
- **FR-013**: The scene MUST render from the fixed front-facing viewpoint; user rearrangement, docking, and camera control are explicitly out of scope for this feature.

### Key Entities

- **Desk scene**: The composed 3D presentation for the skeuomorphic theme — a fixed arrangement of device objects viewed from the front; owns which devices appear based on the emulated machine's configuration.
- **Device object**: A 3D representation of one physical device (Monitor //c, Disk II drive) built from the checked-in parametric models; carries interactive regions (slot, body) and state indicators (lamps, door/disk state).
- **Curved display surface**: The monitor's glass; presents the live emulated picture with spherical curvature and defines the mapping between pointer positions on the glass and emulated screen coordinates.
- **Drive state**: Per-drive live status feeding the scene — mounted image identity, activity, loaded/empty, and write-protect status with its user-facing explanation (shared with the existing 2D band and menus).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Clicking any target in pointer-driven emulated software succeeds at the same rate on the curved display as on the flat display; in a corner-and-edge click test, 100% of clicks land within one emulated pixel of the aimed location.
- **SC-002**: The emulator maintains its current smoothness bar in the skeuomorphic theme: no user-perceivable drop in display frame rate or emulation speed compared to the current release on the same hardware.
- **SC-003**: All drive interactions available in the 2D band (activity light, loaded/empty state, tooltips including write-protect wording, slot-click eject+browse, body-click browse) are demonstrably present in the 3D scene — 100% parity on a scripted walkthrough checklist.
- **SC-004**: Theme switching completes in under one second in either direction with the emulated machine running throughout.
- **SC-005**: The three non-skeuomorphic themes produce pixel-identical rendering to the current release in a side-by-side capture comparison.
- **SC-006**: A first-time viewer shown the skeuomorphic theme identifies the on-screen objects as a period Apple monitor and disk drives without prompting (qualitative gestalt check against reference photos).

## Assumptions

- The checked-in parametric models (Monitor //c, Disk II) on this branch are the visual source for the scene; refinements to the models themselves are ordinary asset updates, not spec changes.
- The scene uses a fixed front-facing composition chosen to echo today's skeuomorphic layout (monitor above/behind, drives below/beside); the exact arrangement is a design-time decision, not user-configurable in this phase.
- User rearrangement, docking, and layout persistence are a follow-on feature; new device types (DuoDisk, //c external drive, ProFile hard disk) arrive in later features paired with their emulation work.
- The existing skeuomorphic 2D drive band and monitor frame are superseded within the skeuomorphic theme only; their behavior contract (tooltips, click regions, indicator timing) is the parity reference and remains available in the codebase until the scene ships.
- Print preview, dialogs, menus, and all non-scene chrome are unaffected.
- Machines other than the Apple II family follow later; this phase targets the currently shipping machine set and its drive configurations.
