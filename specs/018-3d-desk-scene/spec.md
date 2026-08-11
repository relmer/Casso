# Feature Specification: 3D Desk Scene (Parity Phase)

**Feature Branch**: `desk-scene-models`

**Created**: 2026-08-10

**Status**: Draft

**Input**: User description: "Replace the skeuomorphic theme's 2D monitor frame and drive band with a real-time 3D desk scene rendering the checked-in parametric models (Monitor //c + two Disk II drives for now). The emulator display becomes a texture mapped onto the monitor model's spherical-sag glass mesh so the picture has period-correct curvature. Mouse/touch input over the curved display must inverse-project to the correct emulated pixel. Feature-parity with today's skeuo theme is the bar. Compact/dark-modern/retro themes keep their existing simple 2D widgets, untouched."

## Clarifications

### Session 2026-08-10

- Q: After the 3D scene ships, is the 2D skeuomorphic path deleted, kept as an automatic fallback, or kept as a user-visible choice? → A: Fully superseded — the 3D scene is the skeuomorphic theme; the 2D frame/band code is removed once parity is validated, with no fallback path.
- Q: What surrounds the devices in the scene? → A: Today's skeuomorphic theme backdrop, unchanged — the 3D devices compose over it where the 2D frame/band sit today; no desk surface or modeled environment in this phase.
- Q: What does fullscreen show — today's flat display, the whole desk scene, or the curved glass alone? → A: The curved glass only — the picture fills the screen with CRT curvature and curvature-correct input mapping. Drives are reachable via a temporary overlay strip with full windowed-mode interaction parity: summoned by pushing the pointer to the bottom edge when the host owns the pointer, or by a dedicated hotkey when the guest owns it (mouse/paddle capture) — the hotkey releases capture for the interaction and restores it on dismiss. The strip auto-hides on pointer-leave (never while a tooltip or the disk browser is open). While the strip is hidden, drive activity shows an unobtrusive indicator. The Disk menu keeps working as today.
- Q: How does a device's perspective relate to where it sits in the window? → A: One shared viewpoint centered on the display; every device is truly placed in that one world, so its perspective follows from its position — devices to the right are seen slightly from the left, devices below slightly from above, corners combine both. Per-device cameras and straight-on billboarding are prohibited. This is the mechanism that makes docked positions (follow-on spec) read correctly, including live perspective change while dragging.
- Q: Does undocking drives into separate windows carry into the 3D scene (today's chrome is dockable)? → A: Documented parity exception — in this phase the scene is one composed surface with drives in the default position. The follow-on docking spec implements the agreed model: the user drags a drive as a full-opacity rendering of the drive itself (not a translucent ghost) that follows the cursor and may travel outside the window, and on release it always docks to an edge of the Casso window — drives are never free-floating windows.

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
5. **Given** two drives placed below the monitor, **When** the user views the scene, **Then** each drive's visible faces correspond to its position relative to the scene center (below-center drives show a hint of their top; off-center drives show a hint of their inward flank), consistent with one shared viewpoint.

---

### User Story 3 - Theme switching stays seamless and other themes stay untouched (Priority: P3)

A user can switch between the skeuomorphic theme and the two compact-drive themes (dark-modern and retro) at any time. The other themes look and behave exactly as they do today — simple 2D widgets, no 3D scene — and switching in either direction is quick and leaves the emulated machine undisturbed.

**Why this priority**: Protects existing users who prefer the lightweight themes and guarantees the 3D work is additive, not a regression. It's a gate on shipping, but it has no standalone user value until US1/US2 exist.

**Independent Test**: Can be fully tested by cycling through all themes while a machine runs: each non-skeuomorphic theme renders identically to the previous release, the machine never pauses or glitches during a switch, and returning to skeuomorphic restores the 3D scene.

**Acceptance Scenarios**:

1. **Given** a machine is running in the skeuomorphic theme, **When** the user switches to dark-modern or retro, **Then** those themes render their existing 2D widgets unchanged and the machine keeps running without interruption.
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
- Fullscreen mode: fullscreen presents the curved glass alone (monitor body, drives, and backdrop hidden) with curvature and input accuracy intact; the drive overlay strip provides drive access, and entering/leaving fullscreen transitions cleanly to and from the full scene.
- Fullscreen with guest pointer capture (mouse or paddle mode): pointer-to-edge reveal is disabled so the strip can never be summoned by gameplay motion; the dedicated hotkey is the path in, and dismissing the strip restores capture exactly as it was.
- Overlay strip auto-hide races: the strip must never disappear while a tooltip is showing or the disk browser is open from it.

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
- **FR-009**: The non-skeuomorphic themes (dark-modern and retro, the compact-drive themes — "compact" is a trait they share, not a third theme) MUST be visually and behaviorally unchanged from the current release.
- **FR-010**: Switching between the skeuomorphic theme and any other theme MUST complete without pausing, resetting, or visibly glitching the running emulated machine, and the 3D scene MUST reflect current machine state (picture, mounted disks, activity, write-protect) immediately on arrival.
- **FR-011**: The scene MUST adapt to window resizing, display-scaling (DPI) changes, and fullscreen transitions while preserving proportions, legibility, and input accuracy.
- **FR-012**: Window captures of the emulator MUST contain the composed desk scene as displayed.
- **FR-013**: The scene MUST render from the fixed front-facing viewpoint; user rearrangement, docking, and camera control are explicitly out of scope for this feature.
- **FR-014**: Fullscreen MUST present the curved glass alone — the picture filling the screen with curvature and curvature-correct input, monitor body/drives/backdrop hidden.
- **FR-015**: In fullscreen, the system MUST provide a drive overlay strip with full windowed-mode drive interaction parity (activity, tooltips, slot/body clicks): summoned by pointer-to-bottom-edge when the host owns the pointer, or by a dedicated hotkey when the guest has pointer capture (mouse/paddle) — the hotkey releases capture for the interaction and restores it on dismissal. The strip auto-hides on pointer-leave but never while a tooltip or the disk browser is open. Pointer-to-edge summoning MUST be disabled while the guest has capture. While hidden, drive activity MUST surface via an unobtrusive indicator (its exact form is a design-time decision). The Disk menu remains available as today.
- **FR-016**: Within each composed presentation — the windowed desk scene, and the fullscreen drive strip as its own composition — all devices MUST render from that presentation's single shared viewpoint with their true positions in one scene, so each device's perspective corresponds to where it sits relative to that presentation's center (right of center → seen slightly from the left, below → slightly from above, and combinations). Per-device cameras or straight-on billboarding MUST NOT be used. (Elaborates how the FR-013 viewpoint projects each device.)

### Key Entities

- **Desk scene**: The composed 3D presentation for the skeuomorphic theme — a fixed arrangement of device objects viewed from the front, composed over the theme's existing backdrop (no modeled environment in this phase); owns which devices appear based on the emulated machine's configuration.
- **Device object**: A 3D representation of one physical device (Monitor //c, Disk II drive) built from the checked-in parametric models; carries interactive regions (slot, body) and state indicators (lamps, door/disk state).
- **Curved display surface**: The monitor's glass; presents the live emulated picture with spherical curvature and defines the mapping between pointer positions on the glass and emulated screen coordinates.
- **Drive state**: Per-drive live status feeding the scene — mounted image identity, activity, loaded/empty, and write-protect status with its user-facing explanation (shared with the existing 2D band and menus).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Clicking any target in pointer-driven emulated software succeeds at the same rate on the curved display as on the flat display; in a corner-and-edge click test, 100% of clicks land within one emulated pixel of the aimed location.
- **SC-002**: The emulator maintains its current smoothness bar in the skeuomorphic theme: no user-perceivable drop in display frame rate or emulation speed compared to the current release on the same hardware.
- **SC-003**: All drive interactions available in the 2D band (activity light, loaded/empty state, tooltips including write-protect wording, slot-click eject+browse, body-click browse) are demonstrably present in the 3D scene — 100% parity on a scripted walkthrough checklist.
- **SC-004**: Theme switching completes in under one second in either direction with the emulated machine running throughout.
- **SC-005**: Both non-skeuomorphic themes (dark-modern, retro) produce pixel-identical rendering to the current release in a side-by-side capture comparison.
- **SC-006**: A first-time viewer shown the skeuomorphic theme identifies the on-screen objects as a period Apple monitor and disk drives without prompting (qualitative gestalt check against reference photos).

## Assumptions

- The checked-in parametric models (Monitor //c, Disk II) on this branch are the visual source for the scene; refinements to the models themselves are ordinary asset updates, not spec changes.
- The scene uses a fixed front-facing composition chosen to echo today's skeuomorphic layout (monitor above/behind, drives below/beside); the exact arrangement is a design-time decision, not user-configurable in this phase.
- User rearrangement, docking, and layout persistence are a follow-on feature with its interaction model already agreed (drag the full-opacity drive rendering, cursor-following and free to leave the window during the drag, always resolving to a dock edge of the Casso window on release); undocking drives into separate windows is therefore unavailable in this phase as a documented parity exception. New device types (DuoDisk, //c external drive, ProFile hard disk) arrive in later features paired with their emulation work.
- The existing skeuomorphic 2D drive band and monitor frame are fully superseded within the skeuomorphic theme: their behavior contract (tooltips, click regions, indicator timing) is the parity reference during development, and the 2D frame/band code is removed once parity is validated — no fallback path and no user-visible toggle. The app's existing graphics baseline is sufficient for the scene.
- Print preview, dialogs, menus, and all non-scene chrome are unaffected.
- Machines other than the Apple II family follow later; this phase targets the currently shipping machine set and its drive configurations.
