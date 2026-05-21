# Phase 1 — Data Model

This document defines the entities introduced or extended by the UI overhaul.
Schema contracts (JSON Schema + C++ interface headers) live under
`./contracts/`.

---

## Entity: MachineConfig (extended)

**Location**: `CassoEmuCore/Core/MachineConfig.h`

**Existing fields** (unchanged): machine name, ROM table, CPU type, RAM size,
video standard, slot list, internal-device list, etc.

**Renamed field**:

| Old name | New name | Migration |
|----------|----------|-----------|
| `$cassoDefault` (int) | `$cassoMachineVersion` (int) | `MachineConfigUpgrade` reads either; writes the new name. Old name retained as a read-alias for one upgrade cycle. |

**New fields on every hardware-component entry** (both `internalDevices[]`
and `slots[]`):

| Field | Type | Required | Default if missing | Valid values |
|-------|------|----------|--------------------|--------------|
| `capabilityFlag` | string | no | `"required"` on internal devices, `"optional"` on slots (per FR-015) | `"optional"`, `"required"`, `"platform-locked"` |
| `lockReason` | string | no (must be absent unless `capabilityFlag == "platform-locked"`) | — | free-form, ≤200 chars; displayed verbatim in tooltip |

**Validation rules**:
- `capabilityFlag == "platform-locked"` implies the component is also
  effectively required (UI MUST render checkbox as checked + disabled).
- `lockReason` MUST be omitted (or empty) for non-`platform-locked`
  components; loader emits a warning and ignores it if present.
- `$cassoMachineVersion` MUST be a positive integer.

**State transitions**: None at the schema level. Migration is one-shot at
load via `MachineConfigUpgrade`.

---

## Entity: MachineUserConfig (NEW — shadow file)

**File location**: `<assetBaseDir>/Machines/<MachineName>/<MachineName>_user.json`

**Shape**: A subset of `MachineConfig` containing **only** fields the user
has explicitly changed. Always includes `$cassoMachineVersion`.

**Merge semantics** (FR-014, FR-017):
1. Load read-only default `MachineConfig` from `<MachineName>.json`.
2. If `<MachineName>_user.json` exists:
   a. Read its `$cassoMachineVersion`.
   b. If less than the default's version, run `MachineConfigUpgrade` on the
      user file; write the migrated file back to disk.
   c. For each field present in the user file, override the corresponding
      field in the in-memory `MachineConfig`. Fields absent fall through to
      the default.
3. Return the merged `MachineConfig`. Defaults file on disk is never touched.

**Persistable fields** (initial scope — bounded list to avoid scope creep):
- `speedMode` (Authentic / Double / Maximum)
- `videoColorMode` (Color / Green / Amber / White)
- `writeProtect` (per-drive or global, per FR-003)
- `floppySound.enabled` (bool)
- `floppySound.mechanism` (string)
- `internalDevices[i].enabled` (bool) — only for entries whose
  `capabilityFlag == "optional"`
- `slots[i].enabled` (bool) — same constraint
- `lastMountedImages[<slot>][<drive>]` (string path) — per FR-047,
  auto-remounted on machine load; missing files clear the entry.

Fields **not** persistable from this file (per the "Assumptions" section of
the spec): low-level timing, ROM paths, CPU type. Those remain default-only.

---

## Entity: GlobalUserPrefs (NEW)

**File location**: `<assetBaseDir>/GlobalUserPrefs.json`

**Shape** (see `contracts/global-user-prefs.schema.json` for the binding
schema):

```json
{
  "$cassoGlobalPrefsVersion": 1,
  "activeTheme": "Skeuomorphic",
  "lastSelectedMachine": "apple2e",
  "crt": {
    "brightness": 1.0,
    "scanlines":   { "enabled": false, "intensity": 0.5 },
    "bloom":       { "enabled": true,  "radius": 1.0, "strength": 0.4 },
    "colorBleed":  { "enabled": false, "width": 1.0 }
  },
  "window": {
    "lastBounds": { "x": 100, "y": 100, "w": 1280, "h": 960 },
    "fullscreen": false
  }
}
```

**Validation rules**:
- `brightness ∈ [0, 2]` (1.0 = identity; >1 = boosted).
- `scanlines.intensity ∈ [0, 1]`.
- `bloom.radius ∈ [0, 4]`, `bloom.strength ∈ [0, 1]`.
- `colorBleed.width ∈ [0, 4]`.
- `activeTheme` MUST match an installed theme directory name; if not, the
  loader falls back to `"Skeuomorphic"` and logs a warning.

**Persistence triggers**:
- Theme change (FR-034) → write file immediately.
- CRT param change in Settings panel → write on Apply.
- Window bounds → write on `WM_CLOSE`.

---

## Entity: Theme (NEW)

**Directory layout**:

```text
Themes/<ThemeName>/
├── theme.json           # metadata (see contracts/theme-metadata.schema.json)
├── *.rml                # one or more RmlUi layout documents
├── *.rcss               # one or more RmlUi stylesheets
├── fonts/               # *.ttf, *.otf + accompanying license files (e.g. OFL.txt)
├── images/              # *.png, *.jpg, *.dds
└── sounds/              # *.wav, *.ogg (optional UI chrome SFX)
```

Themes MAY omit asset subdirectories that they don't use. Themes MAY also
omit any `entryDocuments` entry; missing entries fall back to the
corresponding `.rml` in `Themes/_shared/`, which ships built-in defaults
for elements not customized by the theme. Themes MUST NOT reference paths
outside their own directory or `Themes/_shared/`.

**theme.json shape**:

```json
{
  "$cassoThemeVersion": 1,
  "name": "Skeuomorphic",
  "author": "Robert Elmer",
  "description": "Beige/cream Apple II hardware aesthetic.",
  "entryDocuments": {
    "titleBar":     "title_bar.rml",
    "navLayer":     "nav_layer.rml",
    "settings":     "settings_panel.rml",
    "driveWidgets": "drive_widgets.rml"
  },
  "crtDefaults": {
    "brightness": 1.0,
    "scanlines":  { "enabled": false, "intensity": 0.0 },
    "bloom":      { "enabled": true,  "radius": 1.0, "strength": 0.3 },
    "colorBleed": { "enabled": false, "width": 0.0 }
  },
  "useMicaBackdrop": false
}
```

**Validation rules**:
- `$cassoThemeVersion` MUST be a positive integer; if lower than current,
  a theme upgrade path runs analogous to `MachineConfigUpgrade` (FR-045).
- Any `entryDocuments.*` entry present MUST refer to a `.rml` file that
  exists relative to the theme directory; entries omitted from
  `entryDocuments` fall back to `Themes/_shared/<entry>.rml`.
- `crtDefaults` follows the same numeric bounds as `GlobalUserPrefs.crt`.
- A theme failing any rule is excluded from the list (FR-036) with a logged
  warning; it MUST NOT crash the app.

**State transitions** (managed by `ThemeManager`):

```
[Discovered] --activate()--> [Loading] --success--> [Active]
[Loading] --parse/asset error--> [Invalid] (logged, excluded)
[Active] --user selects different theme--> [Loading new theme]
[Active] --ReloadCurrent()--> [Loading] (dev convenience)
```

---

## Entity: SettingsPanelState (NEW — transient)

**Location**: `Casso/Ui/SettingsPanelState.h`, in-memory only.

**Purpose**: Holds the user's *in-flight* edits in the Settings panel. Not
persisted until `Apply()` is called. `Cancel()` discards.

**Fields** (mirror persistable subset of `MachineConfig` for the currently
selected machine, plus a few panel-local fields):

| Field | Type | Notes |
|-------|------|-------|
| `selectedMachine` | string | governs all other field bindings |
| `speedMode` | enum | `SpeedMode` |
| `videoColorMode` | enum | `ColorMode` |
| `writeProtect` | per-drive flags |
| `floppySoundEnabled` | bool |
| `floppySoundMechanism` | string |
| `hardwareTree` | list of `{path, enabled, capabilityFlag, lockReason}` | populated from merged MachineConfig |
| `selectedTheme` | string | from GlobalUserPrefs |
| `crtParams` | struct | live preview values; clamped to schema bounds |
| `dirty` | bool | derived; true if any field differs from loaded snapshot |

**Operations**:
- `LoadFromMachine(name)` — discard current state, rebuild from
  `UserConfigStore` + `GlobalUserPrefs`.
- `Apply()` — commit immediate-effect changes (speed, video, theme, CRT)
  via existing atomics + EmulatorShell command queue; persist via
  `UserConfigStore::SaveDelta` and `GlobalUserPrefs::Save`; if any
  hardware-tree change is present, schedule a machine reset after user
  confirmation.
- `Cancel()` — drop state; restore previous snapshot.
- `IsDirty()` — for Apply/Cancel button enabling and close-with-changes
  confirmation.

---

## Entity: DriveWidgetState (NEW — runtime state)

**Location**: `Casso/Ui/DriveWidgetState.h`, owned per-drive by `EmulatorShell`.

| Field | Type | Writer | Reader |
|-------|------|--------|--------|
| `mountedImagePath` | string | UI thread (insert/eject) | UI thread (display label) |
| `motorOn` | `atomic<bool>` | CPU thread (Disk II controller) | UI thread (spin class) |
| `diskActive` | `atomic<bool>` | CPU thread (track read/write) | UI thread (LED active class) |
| `doorState` | enum {Closed, Opening, Open, Closing} | UI thread | UI thread |
| `animationStartTimeMs` | int64 | UI thread | UI thread |

**Concurrency**: Atomic flags are read once per frame; no locks. The
mounted-path string is only mutated from the UI thread (drag-drop, click,
eject) and only read from the UI thread, so no synchronization is needed
for it.

---

## Entity: HardwareComponentEntry (UI view-model, derived from MachineConfig)

| Field | Source |
|-------|--------|
| `id` (stable path, e.g. `"slots[6]"`) | derived |
| `displayName` | `MachineConfig` |
| `slotPosition` | `MachineConfig` (slot index or "internal") |
| `capabilityFlag` | `MachineConfig` |
| `lockReason` | `MachineConfig` (only if platform-locked) |
| `enabled` | merged from default + user JSON |
| `isInteractive` | derived: `capabilityFlag == "optional"` |
| `tooltip` | derived: lockReason if platform-locked, else empty |

This is a pure view-model — never persisted; rebuilt from the merged
`MachineConfig` each time the Settings panel loads or the machine
selector changes.
