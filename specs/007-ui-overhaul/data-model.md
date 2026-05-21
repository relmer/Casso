# Phase 1 Data Model — Full UI Overhaul

Entities introduced or modified by this feature, their fields, validation, and
state transitions. Pure types — no D3D / Win32 dependencies — to keep the data
model testable per Constitution Principle II.

---

## MachineUserConfig (new, CassoEmuCore/Core/)

Per-machine user override snapshot. Sparse — contains only fields the user has
explicitly modified from the default. Merged at load time onto the read-only
default `MachineConfig`.

| Field | Type | Optional | Notes |
|-------|------|----------|-------|
| `cassoMachineVersion` | `int` | required | Matches embedded version at write time; older values trigger `MachineConfigUpgrade` |
| `speedMode` | `optional<SpeedMode>` | yes | `Authentic` / `Double` / `Maximum` |
| `colorMode` | `optional<ColorMode>` | yes | `Color` / `GreenMono` / `AmberMono` / `WhiteMono` |
| `writeProtect` | `optional<map<int, bool>>` | yes | drive-index → protected flag (or global single entry "all") |
| `floppySoundEnabled` | `optional<bool>` | yes | mirrors current global "Drive Audio" toggle |
| `floppyMechanism` | `optional<string>` | yes | "Alps" / "Shugart" |
| `componentEnabled` | `optional<map<string, bool>>` | yes | key = component `type` or `slotN`; only `optional`-capability components are writable |

**Validation rules**:
- Unknown `componentEnabled` keys → logged warning, skipped (edge case from
  spec: "user JSON references a component type that no longer exists").
- Attempting to disable a `required` or `platform-locked` component → entry
  silently dropped during merge (the capability flag wins).

**Merge rule** (`MergeUserOverDefault`):
For each present optional field, override the default; for `componentEnabled`,
per-key override. Missing fields fall through to default.

---

## CapabilityFlag (new enum, CassoEmuCore/Core/MachineConfig.h)

```cpp
enum class CapabilityFlag
{
    Optional,        // user may disable
    Required,        // checked + disabled, no tooltip
    PlatformLocked   // checked + disabled, tooltip from lockReason
};
```

Added as a field on `InternalDevice` and `SlotConfig`:

| Existing struct | New fields |
|-----------------|------------|
| `InternalDevice` | `CapabilityFlag capability = CapabilityFlag::Required;` `string displayName;` `string lockReason;` |
| `SlotConfig` | `CapabilityFlag capability = CapabilityFlag::Optional;` `string displayName;` `string lockReason;` |

**JSON parsing defaults** (per FR-015):
- Field absent on `internalDevices[*]` → `Required`.
- Field absent on `slots[*]` → `Optional`.
- Unknown string value → parse error, machine rejected, fall back to embedded
  default (consistent with edge case "user JSON references a missing
  component").

---

## ThemeData (new, Casso/Theme/)

In-memory representation of a parsed theme JSON. Plain struct, no methods.

| Field | Type | Notes |
|-------|------|-------|
| `cassoThemeVersion` | `int` | Used by `ThemeUpgrade` |
| `name` | `string` | Display name shown in Settings panel |
| `paletteTokens` | `map<string, ColorRgba>` | named colors referenced by widgets (`"chromeBg"`, `"ledIdleCenter"`, ...) |
| `textureRefs` | `map<string, string>` | named texture slot → relative path |
| `geometry` | `GeometryTokens` | per-widget geometry (drive widget size, eject button rect, LED radius, title-bar height, nav-layer item padding) |
| `animation` | `AnimationTokens` | door-open ms, door-close ms, spin RPM, LED fade ms |
| `crtDefaults` | `CrtParams` | scanline intensity, bloom radius/strength, bleed width, brightness — used when user has no override |
| `ledGlowParams` | `LedGlowParams` | per-state glow color, inner radius, outer radius |

Loaded textures themselves are not part of `ThemeData` — they live in a
GPU-side `ThemeAssetCache` owned by `ThemeManager`.

**State transitions** (managed by `ThemeManager`):
```
[Uninitialized] -- LoadAll() --> [Idle]
[Idle] -- SetActiveTheme(name) --> [Loading] -- success --> [Active]
                                            -- failure --> [Idle, error logged]
[Active] -- SetActiveTheme(other) --> [Loading] -- ... (same as above)
```

A failed `SetActiveTheme` leaves the previously active theme intact (FR-036).

---

## SettingsPanelState (new, Casso/Settings/)

Transient UI state for the open panel. Lives on the UI thread.

| Field | Type | Notes |
|-------|------|-------|
| `selectedMachine` | `wstring` | current machine selector value |
| `controls` | `MachineUserConfig` | working snapshot of edits for the selected machine |
| `crtParams` | `CrtParams` | working snapshot of CRT user overrides (global) |
| `themeName` | `string` | working snapshot of theme selection (global) |
| `dirty` | `bool` | true if any control differs from the persisted state |
| `dirtyRequiresReset` | `bool` | true if any dirty change requires a machine reset |

**Transitions**:
```
Open()       → snapshot current persisted state into controls/crtParams/themeName
OnMachineChanged(newName)
             → if dirty: prompt "discard?" — if confirmed, snapshot newName's state
               if not dirty: snapshot newName's state
OnControlChanged(field, value)
             → controls[field] = value; recompute dirty + dirtyRequiresReset
Apply()      → if dirtyRequiresReset: prompt user (FR-010); if confirmed,
                 UserConfigStore.Save() + post EmulatorCommand for reset
               else: UserConfigStore.Save() + write atomics directly
Cancel()     → discard state, close panel
```

---

## DriveWidgetState (new, Casso/Ui/)

Per-drive runtime state read by the D3D render thread, written by both UI
thread (DnD / click-to-browse) and CPU thread (motor signals).

| Field | Type | Notes |
|-------|------|-------|
| `mountedPath` | `wstring` (atomic-pointer-swap) | empty when no disk |
| `motorRunning` | `atomic<bool>` | written by CPU thread |
| `writeActive` | `atomic<bool>` | written by CPU thread |
| `doorAnimPhase` | `float` (UI-thread only) | 0.0=closed, 1.0=open |
| `spinPhase` | `float` (UI-thread only) | accumulates while motorRunning |
| `ledStateHint` | derived | computed each frame from the above |

**LED state derivation** (per FR-025):
```
ledState = motorRunning            ? Active
         : !mountedPath.empty()    ? Present
         :                            Idle
```

---

## global user.json schema (new, see contracts/)

Persisted to `<assetBaseDir>/user.json`.

| Field | Type | Notes |
|-------|------|-------|
| `cassoUserVersion` | `int` | global-prefs schema version |
| `activeTheme` | `string` | name of active theme |
| `crt` | `CrtParams` | brightness, scanlines on/off + intensity, bloom on/off + radius/strength, bleed on/off + width |

The global `user.json` is created on first explicit change; missing fields
fall through to documented defaults.

---

## CrtParams (shared struct, Casso/Theme/)

| Field | Type | Range |
|-------|------|-------|
| `brightness` | `float` | 0.0 .. 1.5 (1.0 = unscaled) |
| `scanlinesEnabled` | `bool` | |
| `scanlineIntensity` | `float` | 0.0 .. 1.0 |
| `bloomEnabled` | `bool` | |
| `bloomRadius` | `float` | pixels, 0.0 .. 8.0 |
| `bloomStrength` | `float` | 0.0 .. 2.0 |
| `bleedEnabled` | `bool` | |
| `bleedWidth` | `float` | pixels, 0.0 .. 4.0 |

Used both as theme defaults (`ThemeData::crtDefaults`) and as user overrides
(global `user.json::crt`); the panel shows defaults until the user adjusts.

---

## Relationships

```
MachineConfig (default, from Resources/Machines/<N>/<N>.json, read-only)
        │
        │ merged with
        ▼
MachineUserConfig (from <assetBaseDir>/Machines/<N>/<N>_user.json, optional)
        │
        │ produces
        ▼
Effective MachineConfig  ←──── EmulatorShell reads/uses
        │
        │ also consulted by
        ▼
SettingsPanelState (transient working copy of overrides + selected machine)


ThemeData (from <assetBaseDir>/Themes/<N>/<N>.json)
        │
        │ active one is held by
        ▼
ThemeManager ──┐
               │ queried by
               ▼
        D3DUiContext (chrome rendering primitives)
               │
               │ consumed by
               ▼
   ChromeWindow / TitleBar / NavLayer / DriveWidget / LedWidget / SettingsPanel


global user.json (<assetBaseDir>/user.json)
   ├── activeTheme  ──── read by ThemeManager at startup
   └── crt           ──── read by CrtPostFx, edited by SettingsPanelState
```
