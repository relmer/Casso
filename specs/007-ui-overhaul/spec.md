# Feature Specification: Full UI Overhaul

**Feature Branch**: `007-ui-overhaul`  
**Created**: 2026-05-20  
**Status**: Draft  
**Input**: User description: Full UI overhaul — Uber Settings Dialog, Full Custom D3D Chrome, JSON/File-Based Theme System

---

## Overview

This feature replaces Casso's current Win32-native chrome (title bar, menu bar, status bar, option dialogs) with a fully custom Direct3D 11–rendered user interface, while simultaneously consolidating scattered per-feature settings dialogs into a single machine-aware settings panel and introducing a hot-swappable CSS-based theme system. The result is an application whose every visible pixel — save the core emulated video output — is rendered through the existing D3D11 pipeline, delivering a cohesive, skeuomorphic-ready aesthetic.

The UI layer is built on **RmlUi** (MIT-licensed HTML/CSS-style UI library with a custom D3D11 backend). The emulated viewport's CRT post-processing uses **MIT/public-domain shader sources** (e.g., CRT-Lottes, CRT-Geom-Mod) shipped in-tree as HLSL ports. These two dependencies are the only third-party libraries introduced and are documented in the project root.

The overhaul spans three interlocked deliverables:

1. **Uber Settings Dialog** — one consolidated settings panel whose top-level control is machine selection; all other settings (speed, video mode, write protect, drive audio, hardware component tree) immediately reflect the selected machine's saved configuration when the machine selection changes.
2. **Full Custom D3D Chrome** — borderless window with custom D3D-rendered title bar, drive widgets (physical disk drive look with spinning animation and eject slot), realistic LED indicators, and a D3D-rendered navigation/menu layer.
3. **CSS-Based Theme System** — themes expressed as RML (layout) + RCSS (styles) + asset files living in a `Themes/` directory (bootstrapped on first launch like `Machines/`), hot-swappable at runtime, with several built-in themes shipped with the application.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Change emulation speed without hunting through menus (Priority: P1)

A user who frequently switches between authentic 1 MHz and maximum-speed disk imaging runs opens the single Settings panel from the custom toolbar, changes the machine from Apple //e to Apple II+, and immediately sees all settings (speed, video mode, hardware tree) update to that machine's last-saved values. They then set speed to Maximum. On closing, those settings are committed as the Apple II+'s user override and the menu items (which no longer exist as Win32 menus) are no longer needed.

**Why this priority**: Consolidating scattered menu items into a single machine-aware dialog is the core productivity gain of the settings work. If this story fails, users still need to find multiple menu items across multiple menus and the overhaul has not achieved its goal.

**Independent Test**: With at least two machine profiles installed, open the Settings dialog, switch between machines, and verify that every visible setting control reflects the newly-selected machine's persisted values within one frame of the selection change. Changing speed mode for one machine MUST NOT alter another machine's saved speed mode.

**Acceptance Scenarios**:

1. **Given** the Settings dialog is open with machine Apple //e selected, **When** the user changes the machine selector to Apple II+, **Then** all setting controls (speed, video mode, write protect, drive audio, hardware tree) immediately display the Apple II+'s last-saved values without closing and reopening the dialog.
2. **Given** a user has previously set Apple //e to "Maximum" speed and Apple II+ to "Authentic", **When** the user opens the Settings dialog and switches machines between them, **Then** each machine's speed setting is shown correctly for that machine, and changes to one machine do not alter the other.
3. **Given** the user changes speed mode to "Double" for the currently running machine, **When** the dialog is confirmed/applied, **Then** emulation speed changes immediately and the per-machine user JSON is updated with the new value.

---

### User Story 2 — Enable or disable a hardware component for a machine (Priority: P1)

A power user opens the Settings dialog, navigates the hardware component tree, and unchecks the "Disk II Controller (Slot 6)" component to test booting without a disk controller. Optional components show a normal enabled checkbox; required components (e.g., Apple II speaker) show a checked, grayed-out checkbox; platform-locked components (e.g., 80-column card on Apple //c) show a checked, grayed-out checkbox with a tooltip explaining the lock. The user re-enables Disk II, applies, and the change takes effect on the next machine reset without restarting the application.

**Why this priority**: The hardware capability tree is a new first-class concept enabling users to model stripped-down machine configurations without editing raw JSON. Without it, the only way to remove a component is a manual JSON edit.

**Independent Test**: Open the Settings dialog, locate the Disk II controller row in the hardware tree, uncheck it, apply, and reset the machine. Verify the emulator boots without a disk controller. Then re-enable it, apply, and reset — the disk controller must be active again.

**Acceptance Scenarios**:

1. **Given** the hardware component tree is displayed, **When** a component has capability `optional`, **Then** it renders with a fully interactive checkbox that can be checked or unchecked.
2. **Given** a component has capability `required`, **When** the tree renders it, **Then** its checkbox is checked and disabled (not interactive), and no tooltip is required.
3. **Given** a component has capability `platform-locked`, **When** the tree renders it, **Then** its checkbox is checked, disabled, and a hover tooltip explains why the component cannot be removed (e.g., "Built into the Apple //c motherboard").
4. **Given** the user unchecks an optional component and confirms the dialog, **When** the machine resets, **Then** that component is absent from the active hardware configuration.

---

### User Story 3 — Insert a disk using the custom drive widget (Priority: P1)

A user sees two physical-drive-style widgets in the custom D3D chrome. Drive 1 shows an idle drive with a visible eject slot and a softly glowing LED. They drag a `.woz` file from Windows Explorer onto the Drive 1 widget; the drive widget plays a brief door-close animation, the LED brightens to indicate a disk is present, and the disk label appears next to the widget. When the emulated machine reads the disk, a spinning disk animation plays inside the drive face.

**Why this priority**: Drive interaction is the most frequent user action in a disk-based Apple II emulator. The custom chrome replaces the current Win32 status-bar drive indicators and drag-drop behavior with a more discoverable, expressive equivalent.

**Independent Test**: With no disks mounted, drag a `.dsk` image file onto each drive widget and verify: (a) door-close animation plays, (b) LED changes to "disk present" state, (c) disk label is visible, (d) during a disk read the spinning animation runs, (e) the eject button ejects the disk and plays a door-open animation.

**Acceptance Scenarios**:

1. **Given** a drive widget is in the empty state, **When** the user drags a valid disk image file onto it, **Then** a door-close animation plays, the LED transitions to "disk present" brightness, and the disk label is shown adjacent to the widget.
2. **Given** a disk is mounted and the emulated machine is executing a disk read or write, **When** the D3D frame renders, **Then** an animated spinning disk is visible within the drive widget face.
3. **Given** a disk is mounted, **When** the user clicks the eject affordance on the widget, **Then** a door-open animation plays, the LED returns to idle glow, and the disk is unmounted.
4. **Given** a drive widget is in the empty or mounted state, **When** the user clicks the drive widget body (not the eject affordance), **Then** a file-open dialog appears filtered to `.dsk`, `.nib`, `.woz`, and `.po`; on confirmation the selected image is mounted and the door-close animation plays.
5. **Given** the user drags an unrecognized file type onto a drive widget, **When** the drop occurs, **Then** no animation plays, the widget state is unchanged, and a brief error indication is shown (e.g., LED flickers or a tooltip-style message appears).

---

### User Story 4 — Apply a different theme at runtime (Priority: P2)

A user navigates to the Theme section of the Settings dialog and selects "Retro Terminal" from the list of installed themes. Without closing the application or resetting the machine, all chrome (drive widgets, LEDs, title bar, menu bar, settings dialog chrome) instantly re-renders with the phosphor-green/scanline aesthetic. The emulated video output is unaffected. The user then switches back to "Skeuomorphic" to confirm the beige/cream look returns.

**Why this priority**: The theme system is what allows the chrome overhaul to serve users with different aesthetic preferences, and hot-swap is the differentiating quality that makes theme selection exploratory rather than a restart-and-hope workflow.

**Independent Test**: Ship at least three built-in themes. Open Settings, cycle through all three, and verify that (a) each theme change is visually complete within the same frame or the next frame, (b) the emulated screen content does not change, (c) the theme selection persists across application restarts.

**Acceptance Scenarios**:

1. **Given** the Settings dialog is open on the Theme page, **When** the user selects a different theme from the list, **Then** all custom-chrome visual elements update to that theme's colors, textures, and layout geometry within one rendered frame (no restart required).
2. **Given** a theme change is applied, **When** the emulated video output frame is rendered, **Then** the emulated screen content is pixel-identical to what it would have been with any other theme active (themes affect only chrome, not emulation).
3. **Given** the user selects "Dark Modern", closes the application, and relaunches, **When** the application fully initializes, **Then** the chrome renders with the "Dark Modern" theme without requiring the user to re-select it.
4. **Given** a user has authored a custom theme directory (RML + RCSS + `theme.json`) and placed it in the `Themes/` directory while the application is running, **When** the user opens the Settings Theme page, **Then** the new theme appears in the list and can be selected immediately.

---

### User Story 5 — Interact with the custom title bar and navigation layer (Priority: P2)

A user double-clicks the custom D3D-rendered title bar to toggle fullscreen, drags it to reposition the window, and right-clicks it to access a system-menu equivalent. The custom menu/navigation layer (replacing the Win32 menu bar) responds to hover and click with animated highlights consistent with the active theme.

**Why this priority**: The custom chrome is only usable if its fundamental windowing behaviors (move, resize, fullscreen, close) are at least as reliable as the Win32 chrome they replace.

**Independent Test**: Confirm drag-to-move, double-click-to-fullscreen, close button, minimize, and maximize all work correctly with the borderless window. Confirm the nav layer opens sub-menus on click and can be dismissed by clicking outside.

**Acceptance Scenarios**:

1. **Given** the application is in windowed mode, **When** the user clicks and drags the custom title bar region, **Then** the window repositions following the drag exactly as a native title bar would.
2. **Given** the user double-clicks the custom title bar, **When** the action completes, **Then** the window toggles between windowed and fullscreen mode, consistent with the existing D3DRenderer fullscreen toggle behavior.
3. **Given** the user clicks the close button in the custom title bar, **When** the click is processed, **Then** the application initiates a clean shutdown (saving state, flushing dirty disk images) exactly as closing via Alt+F4 currently does.
4. **Given** the navigation layer is visible, **When** the user clicks a top-level nav item (e.g., "Machine"), **Then** a drop-down panel appears with the items that were formerly in the corresponding Win32 menu, styled per the active theme.

---

### User Story 6 — Per-machine JSON settings survive an upgrade (Priority: P3)

A user who ran Casso v1.x has a `Machines/apple2e/apple2e_user.json` file on disk that predates a new `$cassoDefault` version. On launching the new version, Casso detects the version mismatch, automatically runs `MachineConfigUpgrade` to bring the user file forward, and writes the migrated file back. The user's customizations (speed, video mode, disabled components) are preserved; new fields introduced in the new version fall through to the read-only default machine JSON.

**Why this priority**: Settings persistence is only trustworthy if upgrades are silent and lossless. Losing a user's hardware configuration on update is a serious regression. Prioritized P3 because the upgrade path is infrastructure that only activates during the version transition, not the daily-use path.

**Independent Test**: Create a user JSON with a `$cassoDefault` version behind the current default, launch the application, and verify the user file is silently upgraded and the resulting in-memory config merges user overrides with new default fields correctly.

**Acceptance Scenarios**:

1. **Given** a user JSON file exists with a `$cassoDefault` version lower than the current default, **When** the application loads that machine, **Then** `MachineConfigUpgrade` is invoked before the config is used, and the migrated file is written back to disk in the same location.
2. **Given** the migrated user JSON is missing a field introduced in the new version (e.g., `capabilityFlag` on a new internal device), **When** the config is resolved, **Then** the missing field's value is silently taken from the read-only default JSON rather than causing an error.
3. **Given** the user JSON specifies a non-default value for a field that also exists in the default (e.g., speed mode), **When** the config is resolved, **Then** the user JSON's value takes precedence over the default.

---

### Edge Cases

- What happens when the `Themes/` directory is missing or empty on first launch? → Bootstrap must extract built-in theme files from embedded resources, mirroring the `Machines/` bootstrap pattern in `AssetBootstrap`.
- What happens when a user-authored theme is malformed (invalid RCSS, missing `theme.json`, missing asset file)? → The theme is excluded from the selection list, a warning is logged, and the previously active theme remains active. Application does not crash.
- What happens when the Settings dialog is open and the user triggers a machine reset? → If the dialog has unapplied changes, present a brief confirmation; if confirmed, dismiss the dialog without applying and proceed with the reset.
- What happens when the user JSON for a machine references a component `type` that no longer exists in the current codebase? → The unknown component is skipped (logged as a warning) and the machine loads with the remaining valid components.
- What happens when the user drags an image file onto a drive widget while the emulator is running at Maximum speed? → The disk insert must be safe regardless of emulation speed; the command is posted to the CPU thread command queue (same as current behavior) and the UI widget updates optimistically.
- What happens when D3D device is lost (e.g., screen lock, GPU driver update)? → The existing `D3DRenderer` device-lost recovery path must cover the chrome layer as well; all chrome geometry and textures must be re-created on device restore.
- What happens when there is no user JSON yet for a machine (first launch or fresh install)? → No migration is run; the read-only default JSON is used as the sole config source; a user JSON is written only on the first explicit settings change.

---

## Requirements *(mandatory)*

### Functional Requirements — Area 1: Uber Settings Dialog

- **FR-001**: The Settings panel MUST be the single entry point for all emulation and machine configuration; no setting currently accessible via Win32 menu items SHALL remain exclusively in a menu item after this feature ships.
- **FR-002**: Machine selection MUST be the outermost/governing control in the Settings panel; all other controls MUST reflect the selected machine's saved values and MUST update immediately when the machine selection changes, without closing and reopening the dialog.
- **FR-003**: The Settings panel MUST expose the following controls, each bound to the selected machine's saved values: machine selector, emulation speed (Authentic / Double / Maximum), write protect mode (per-drive or global), floppy sound toggle and mechanism selector, video mode (Color / Green / Amber / White Mono).
- **FR-004**: The Settings panel MUST include a hardware component tree (listview or treeview layout) that reflects the selected machine's hardware configuration, structured to match the JSON hierarchy (internal devices, slots).
- **FR-005**: Each hardware component row in the tree MUST display the component's human-readable name, its slot or position (where applicable), and a checkbox indicating enabled/disabled state.
- **FR-006**: Components with capability `optional` MUST render an interactive checkbox; the user CAN check or uncheck them.
- **FR-007**: Components with capability `required` MUST render a checked, non-interactive (disabled) checkbox; no tooltip is required.
- **FR-008**: Components with capability `platform-locked` MUST render a checked, non-interactive checkbox AND display a tooltip on hover explaining why the component is locked (e.g., "Built into the Apple //c motherboard — cannot be removed").
- **FR-009**: Settings changes in the panel MUST NOT take effect in the running emulation until the user explicitly applies or confirms them; a Cancel action MUST discard all unapplied changes.
- **FR-010**: On confirmation, settings that require a machine reset to take effect (e.g., hardware component changes) MUST prompt the user before applying, clearly stating that a reset will occur.
- **FR-011**: Settings that take effect immediately without a reset (e.g., emulation speed, video mode, floppy sound toggle) MUST be applied at dialog confirmation without requiring a machine reset.

### Functional Requirements — Area 1: Settings Persistence

- **FR-012**: Per-machine user-override settings MUST be stored as JSON files at `<assetBaseDir>/Machines/<MachineName>/<MachineName>_user.json`, following the same directory convention as the existing default machine JSONs.
- **FR-013**: On loading a machine, if a user JSON file exists and its `$cassoDefault` version is lower than the current default JSON's version, `MachineConfigUpgrade` MUST be invoked to migrate the user file before the config is used, and the migrated result MUST be written back to the user JSON path.
- **FR-014**: Fields present in the user JSON MUST shadow (take precedence over) the corresponding fields in the read-only default JSON. Fields absent from the user JSON MUST fall through to the default JSON value.
- **FR-015**: The machine configuration JSON schema MUST be extended to include a `capabilityFlag` field on each hardware component entry (both `internalDevices` and `slots`). Valid values are `"optional"`, `"required"`, and `"platform-locked"`. Absence of the field in legacy JSONs MUST be treated as `"optional"` for slots and `"required"` for internal devices.
- **FR-016**: Registry-based settings (`RegistrySettings`) for machine-specific values (speed, video mode, etc.) MUST be superseded by the per-machine user JSON; the registry path MAY be retained as a one-time migration source on first upgrade but MUST NOT be the primary storage after this feature ships.
- **FR-017**: User JSON files MUST NOT overwrite the read-only embedded default JSONs; the shadow/fallthrough merge MUST be performed in memory at load time.

### Functional Requirements — Area 2: Full Custom D3D Chrome

- **FR-018**: The application window MUST use a borderless style; all window chrome (title bar, borders, resize handles, system buttons) MUST be implemented in the D3D rendering layer rather than by the Win32 window manager.
- **FR-019**: The custom title bar region MUST support: drag to move the window, double-click to toggle fullscreen (delegating to the existing `D3DRenderer::ToggleFullscreen`), close button (triggering clean shutdown), minimize button, and maximize/restore button.
- **FR-020**: The application MUST maintain the existing Win32 window handle (`HWND`) as the D3D render surface anchor; the emulated video output sub-region MUST remain pixel-identical in position and scale to a user-specified aspect ratio setting.
- **FR-021**: Drive widgets MUST be rendered in D3D and MUST visually represent physical disk drive hardware at a fidelity determined by the active theme.
- **FR-022**: Drive widgets MUST support drag-and-drop of disk image files from Windows Explorer (`.dsk`, `.nib`, `.woz`, `.po`); on successful drop the existing disk-mount logic MUST be invoked.
- **FR-022b**: Drive widgets MUST also support click-to-browse; clicking the drive widget body (not the eject affordance) MUST open a file-open dialog filtered to supported disk image types and invoke the existing disk-mount logic on confirmation.
- **FR-023**: Drive widgets MUST display a spinning disk animation during active disk read or write operations, driven by the same motor-on/off signals currently used by the audio and debug systems.
- **FR-024**: Drive widgets MUST display an eject affordance; clicking it MUST invoke the existing disk-eject logic.
- **FR-025**: LED indicators for each drive MUST be rendered in D3D with a soft-glow appearance (not a flat colored circle); LED states MUST include at minimum: idle (disk absent), disk present, and active (motor running). Exact glow parameters are defined by the active theme.
- **FR-026**: A D3D-rendered navigation/menu layer MUST replace the Win32 menu bar; it MUST provide access to all commands currently in the Win32 menus (File, Machine, View, Disk, Edit, Help groups).
- **FR-027**: The Settings dialog (FR-001 through FR-017) MUST be rendered in D3D within the application window rather than as a Win32 dialog (`DialogBoxIndirectParam`). The existing `OptionsDialog` and `MachinePickerDialog` Win32 dialogs MUST be retired by this feature.
- **FR-028**: The D3D chrome layer MUST correctly handle WM_NCHITTEST returns for borderless window behavior, ensuring OS-level window management (snap, Aero Shake, Task View) continues to function correctly.

### Functional Requirements — Area 3: CSS-Based Theme System

- **FR-029**: Themes MUST be defined as a directory under the asset base directory (same base as `Machines/` and `Devices/`) containing RML layout files, RCSS stylesheets, image/font assets, and a small `theme.json` metadata file (name, version, author, description).
- **FR-030**: On first launch (or when `Themes/` is absent), `AssetBootstrap` MUST extract the built-in themes from embedded resources to the `Themes/` directory, following the same pattern as `EnsureMachineConfigs`.
- **FR-031**: The application MUST ship at least three built-in themes: **Skeuomorphic** (beige/cream palette, physical Apple II hardware appearance), **Dark Modern** (dark palette with glowing LED accents and neon highlights), and **Retro Terminal** (phosphor green/amber, scanlines overlay, CRT aesthetic).
- **FR-032**: A theme's RML/RCSS MUST define: color palette tokens (RCSS custom properties), widget visual treatment (drive widgets, LED indicators, title bar, nav layer, Settings panel), layout geometry, animation parameters (RCSS transitions/keyframes), and CRT effect defaults (in the `theme.json` metadata).
- **FR-033**: Themes MUST be hot-swappable: switching the active theme in the Settings panel MUST cause all RmlUi-rendered chrome to re-render with the new theme's styles within the current or immediately following rendered frame, without restarting the application or resetting the emulator.
- **FR-034**: The active theme selection MUST persist across application restarts, stored in global user settings (not machine-specific, as theme is a UI preference not a machine property).
- **FR-035**: User-authored theme directories placed in `Themes/` while the application is running MUST appear in the theme list the next time the Theme section of the Settings panel is opened (no restart required to discover new themes).
- **FR-036**: A malformed theme (missing `theme.json`, invalid RCSS that fails RmlUi parsing, or referencing missing assets) MUST be excluded from the theme list with a warning logged; it MUST NOT crash the application or corrupt the active theme.
- **FR-037**: Built-in theme files in `Themes/` that have been overwritten or deleted by the user MUST be re-extracted from embedded resources on next launch (same behavior as machine config defaults).
- **FR-038**: The Settings panel MUST include a CRT brightness control (e.g., a knob or slider) that adjusts the luminance of the emulated display output. The brightness value MUST be persisted globally (display preference, not machine-specific) and applied in real time as the control is adjusted.
- **FR-039**: The D3D renderer MUST support optional CRT post-processing effects applied to the emulated screen sub-region: scanlines (horizontal darkening bands matching the Apple II display refresh geometry), phosphor bloom (soft luminance bloom on bright pixels), and color bleed (lateral spread of chroma between adjacent pixels, approximating NTSC phosphor persistence). Each effect MUST be individually toggleable. Implementation MAY adapt MIT/public-domain HLSL ports of community CRT shaders (e.g., CRT-Lottes, CRT-Geom-Mod) shipped in-tree; GPL-licensed shaders MUST NOT be used.
- **FR-040**: CRT effect parameters (scanline intensity, bloom radius/strength, bleed width) MUST be configurable per-theme as defaults in `theme.json`, and MUST also be overridable per-user in the Settings panel. User overrides are persisted globally (display preference, not machine-specific).
- **FR-041**: The emulated machine MUST continue running while the Settings panel is open; the panel operates on a transient `SettingsPanelState` snapshot and changes are only applied on explicit user confirmation. No pause/resume API is required.
- **FR-042**: The application MUST target Windows 11 as the minimum supported version; Windows 11-only DWM APIs (e.g., `DwmSetWindowAttribute` with `DWMWA_WINDOW_CORNER_PREFERENCE`, Mica backdrop) MAY be used in the chrome implementation.
- **FR-043**: The emulated display viewport MUST maintain the Apple II's native aspect ratio (4:3) at all window sizes; when the window aspect ratio differs from 4:3, the viewport MUST be letterboxed or pillarboxed with the remaining area used by chrome widgets or rendered black.
- **FR-044**: All interactive elements in the RmlUi-rendered Settings panel MUST be fully operable via keyboard alone (Tab/Shift-Tab to navigate, Space/Enter to activate, Escape to dismiss); keyboard focus MUST be visually indicated in all active themes (via RCSS `:focus` styles).
- **FR-045**: The `theme.json` metadata file MUST include a `$cassoThemeVersion` integer field. On load, if the version is lower than the current expected version, a theme upgrade path MUST be applied (analogous to `MachineConfigUpgrade`) before the theme is used. Machine config JSON files MUST use `$cassoMachineVersion` (renaming the existing `$cassoDefault` field).
- **FR-046**: The application MUST use **RmlUi** (MIT-licensed) as the UI framework for all custom chrome and the Settings panel, integrated via a custom D3D11 render backend that composites RmlUi output onto the existing swap chain. Direct D3D drawing (outside RmlUi) is reserved for the emulated viewport and CRT post-processing pass.

---

### Key Entities

- **MachineUserConfig**: Per-machine user override JSON file (`<MachineName>_user.json`). Contains a `$cassoMachineVersion` version field and only the fields the user has explicitly changed from the default. Merged at load time with the read-only default JSON. Managed by a new `UserConfigStore` or equivalent.
- **HardwareComponentEntry**: A node in the hardware configuration tree. Has a `type` (string matching the component registry), a human-readable `displayName`, a `capabilityFlag` (`optional` / `required` / `platform-locked`), an optional `lockReason` string (for platform-locked tooltip), and an `enabled` boolean state.
- **Theme**: A subdirectory of `Themes/` containing `theme.json` metadata (name, `$cassoThemeVersion`, CRT effect defaults), one or more `.rml` layout files, one or more `.rcss` stylesheet files, and asset files (fonts, images, sounds). Loaded by `ThemeManager` and applied to RmlUi at selection time.
- **SettingsPanelState**: Transient in-memory snapshot of the Settings dialog's current selections; not persisted until the user confirms. Allows cancel-without-effect semantics.
- **DriveWidgetState**: Runtime state for each drive widget: mounted disk path (or empty), motor running flag, write-active flag, animation frame index. Updated by the CPU thread's motor/drive signals and read by the D3D render thread.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can change machine, adjust all settings, and confirm in under 60 seconds — half the time currently required by navigating three separate menus and dialogs.
- **SC-002**: Switching the active theme causes all chrome to visually update within a single displayed frame on a mid-range GPU (no perceptible flash or partial-redraw artifact).
- **SC-003**: Per-machine user settings survive at least three consecutive application upgrades (each introducing at least one new config field) without data loss or user-visible error.
- **SC-004**: Drive widget drag-and-drop succeeds for all supported image formats (`.dsk`, `.nib`, `.woz`, `.po`) with the same reliability as the current menu-based disk mount path.
- **SC-005**: The full chrome layer (title bar, drive widgets, nav menu, settings panel) adds no more than 1 ms to the total frame time on a mid-range GPU at the application's native rendering resolution.
- **SC-006**: All commands reachable via the current Win32 menu bar remain reachable through the D3D navigation layer; no command is lost in the migration.
- **SC-007**: A user with a pre-existing `_user.json` from an earlier version experiences no settings loss or application error after upgrading; the migration is silent.
- **SC-008**: At least 90% of user-study participants can locate and change the machine's emulation speed without instruction, compared to a baseline of under 60% with the current menu system.

---

## Assumptions

- The existing `D3DRenderer::UploadAndPresent` pipeline remains the sole path for presenting the emulated framebuffer; the chrome layer is composited as an additional D3D render pass on the same swap chain, not a separate window.
- `WM_NCHITTEST` customization is sufficient to achieve borderless-window drag/resize behavior on Windows 11; Windows 11-only DWM APIs are permitted and preferred for rounded corners and backdrop effects.
- The `AssetBootstrap` directory-scanning and embedded-resource-extraction pattern is directly reusable for the `Themes/` bootstrap without architectural changes.
- The `MachineConfigUpgrade` system already handles versioned config migration; extending it to cover the `capabilityFlag` field and other new schema fields is an incremental change, not a redesign. The existing `$cassoDefault` version field in machine JSONs is renamed to `$cassoMachineVersion`; theme `theme.json` files use `$cassoThemeVersion` following the same per-type convention.
- **RmlUi** (MIT) is the chosen UI framework; integrated via a custom D3D11 render backend that draws into the existing swap chain. Source is vendored in-tree under `External/RmlUi/` or built as a static library project. No package manager or vcpkg integration.
- The CRT post-processing shaders are HLSL ports of MIT/public-domain community shaders (e.g., CRT-Lottes, CRT-Geom-Mod, libretro common shader collection). GPL-licensed shaders (e.g., CRT-Royale) are explicitly excluded to keep Casso MIT-licensed.
- Per-machine `_user.json` files contain only settings reachable from the Settings panel (speed, video mode, write protect, drive audio, component enable/disable); low-level timing and ROM configuration remain in the default machine JSONs and are not user-overridable from the UI.
- Theme textures and images are loaded at theme-selection time; they are retained in GPU memory until a different theme is selected or the application exits. Themes do not hot-reload individual assets independently.
- The three built-in theme names (Skeuomorphic, Dark Modern, Retro Terminal) are final for the initial release; additional built-in themes may be added in subsequent releases without schema changes.
- The `Casso` project's existing `atomic<ColorMode>` and `atomic<SpeedMode>` fields in `EmulatorShell` are the authoritative runtime state; the Settings panel reads from and writes to these atomics (through the existing command-queue mechanism where a reset is needed).
- Mobile or remote-desktop scenarios with software renderers are out of scope for the chrome performance target (SC-005); the 1 ms budget applies to hardware D3D11 feature level 11.0 or higher.
- The Win32 `OptionsDialog` and `MachinePickerDialog` are fully retired (no "legacy mode" fallback) once the D3D Settings panel ships; backward compatibility is achieved via the user JSON migration path (FR-013), not by keeping old dialogs alive.
