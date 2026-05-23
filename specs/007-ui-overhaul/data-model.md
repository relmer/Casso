# Phase 1 — Data Model (Native UI Reset)

This document defines the entities introduced or extended by the native reset.
Schema and interface contracts live under `./contracts/`.

## Entity: Theme (updated)

**Directory layout**

```text
Themes/<ThemeName>/
├── theme.json
├── images/
├── sounds/
└── fonts/               # optional; title/nav still use Windows system UI font
```

**theme.json shape**

```json
{
  "$cassoThemeVersion": 1,
  "name": "Apple IIe Classic",
  "familyId": "apple2",
  "variantId": "iie",
  "author": "Casso Team",
  "description": "Apple IIe visual treatment.",
  "uiTokens": {
    "chrome": {
      "backgroundGradient": ["#efe7d6", "#d8ccb7"],
      "edgeShadow": "#6b5f4f",
      "titleTextColor": "#1f1f1f"
    },
    "menus": {
      "background": "#f2ead9",
      "hover": "#d5c4a6",
      "separator": "#8f7d63"
    },
    "timingsMs": {
      "menuOpen": 80,
      "driveDoor": 120
    }
  },
  "driveVisualProfile": {
    "style": "disk2",
    "colorway": "beige",
    "doorAnimation": "mechanicalSwing",
    "syncChannel": "drive-door"
  },
  "crtDefaults": {
    "brightness": 1.0,
    "scanlines": { "enabled": false, "intensity": 0.0 },
    "bloom": { "enabled": true, "radius": 1.0, "strength": 0.3 },
    "colorBleed": { "enabled": false, "width": 0.0 }
  },
  "useMicaBackdrop": false
}
```

**Validation rules**

- `$cassoThemeVersion` must be positive.
- `familyId` and `variantId` are required and non-empty.
- `driveVisualProfile.style` must support `disk2` for Apple II-family variants.
- `crtDefaults` numeric bounds match global CRT prefs bounds.
- Invalid themes are excluded with a warning and never crash runtime.

## Entity: ChromeMetrics (new, transient)

| Field | Type | Notes |
|-------|------|-------|
| `dpi` | `UINT` | Per-window DPI |
| `titleHeightPx` | `int` | Title strip height |
| `navHeightPx` | `int` | Menu/nav strip height |
| `buttonWidthPx` | `int` | NC button width |
| `buttonHeightPx` | `int` | NC button height |
| `paddingPx` | `int` | Horizontal chrome padding |

## Entity: ChromeRects (new, transient)

| Field | Type | Notes |
|-------|------|-------|
| `titleRect` | `RECT` | Full title strip |
| `dragRect` | `RECT` | Draggable caption region |
| `minRect/maxRect/closeRect` | `RECT` | NC button regions |
| `navStripRect` | `RECT` | Menu/nav strip frame |
| `navItemRects[]` | `vector<RECT>` | Top-level menu items |
| `dropdownRects[]` | `vector<RECT>` | Active dropdown items |

## Entity: ChromeVisualState (new, transient)

| Field | Type | Notes |
|-------|------|-------|
| `hoveredButton` | enum/optional | Min/Max/Close/None |
| `hoveredNavItem` | `int` | `-1` when none |
| `activeMenu` | enum/optional | Open dropdown source |
| `pressed` | bool | Pointer-down state |
| `windowActive` | bool | Active/inactive coloring |

## Entity: DriveWidgetState (new runtime state)

| Field | Type | Writer | Reader |
|-------|------|--------|--------|
| `mountedImagePath` | `string` | UI thread | UI thread |
| `motorOn` | `atomic<bool>` | Emulation thread | UI thread |
| `diskActive` | `atomic<bool>` | Emulation thread | UI thread |
| `doorState` | enum | UI thread | UI thread |
| `animationStartTimeMs` | `int64` | UI thread | UI thread |
| `lastSyncEventId` | `uint64` | Drive sync broker | UI thread/audio |

## Entity: DriveSyncEvent (new)

Single synchronization event payload used by both animation and audio:

```text
{ eventId, driveId, action(open|close|spinStart|spinStop), timestampTicks }
```

Contract: animation and sound consumers must start from the same event id and
remain within one rendered frame of each other.

## Entity: HardwareComponentEntry (new persisted/view state)

| Field | Type | Notes |
|-------|------|-------|
| `type` | `string` | Component registry type key |
| `displayName` | `string` | Human-readable tree label |
| `capabilityFlag` | enum | `optional`, `required`, or `platform-locked` |
| `lockReason` | `string` | Optional tooltip text for platform-locked entries |
| `enabled` | bool | Current staged enable-state |

## Entity: SettingsPanelState (new transient state)

Transient in-memory snapshot of unapplied Settings selections. It is discarded
on Cancel and persisted only on Apply.

| Field | Type | Notes |
|-------|------|-------|
| `selectedMachine` | `string` | Machine currently staged in the selector |
| `speedMode` | enum | Emulation speed selection |
| `colorMode` | enum | Video color mode selection |
| `writeProtect[2]` | bool array | Per-drive write-protect toggles |
| `floppySoundEnabled` | bool | Disk audio enable-state |
| `floppyMechanism` | enum/string | Selected drive mechanism sound profile |
| `hardwareTreeState` | collection | `HardwareComponentEntry` nodes and expansion/selection state |
| `activeThemeName` | `string` | Staged theme selection |
| `crtParams` | object | Brightness and CRT effect toggle/parameter values |

## Entity: WindowPlacementProfile (existing registry-backed profile)

Per-monitor-configuration window bounds record keyed by a hash of current
monitor topology plus active monitor. Stored under
`HKCU\Software\relmer\Casso\WindowPlacement\v1\<hash>`.

| Field | Type | Notes |
|-------|------|-------|
| `profileHash` | `string` | Monitor topology + active monitor key |
| `x` / `y` | int | Restored top-left window position |
| `w` / `h` | int | Restored window size |

## Entity: GlobalUserPrefs (existing, extended semantics)

- `activeTheme` now resolves via `(familyId, variantId)` + theme name.
- `window.lastBounds` behavior remains monitor-profile aware as already defined.

## Entity: UiDrawList (new transient command stream)

Per-frame batched geometry, texture, and text command stream consumed by
`DxUiPainter` and `DwriteTextRenderer`. It contains resolved theme tokens,
DPI-scaled rects, texture handles, text runs, clipping, z-order, and blend mode
for the current frame only.

## Entity: MachineConfig / MachineUserConfig (unchanged core + persisted settings)

No reset-driven structural change required beyond existing machine/user merge
model and persistable settings set. UI reset only changes ownership of rendering
and interaction layers, not machine hardware schema.
