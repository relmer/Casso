# Data Model: Physical Game Controllers

**Feature**: `034-game-controllers` | **Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

All types live in `CassoEmuCore/Controllers/` unless stated. All are plain data or pure logic, reachable from `UnitTest` with synthetic samples; only `Win32ControllerBackend` touches devices.

## Identity

### ControllerKind (enum)

`XInput`, `DirectInput`.

### ControllerModelKey

| Field | Type | Notes |
|---|---|---|
| kind | `ControllerKind` | |
| vendorId | `Word` | 0 when unknown |
| productId | `Word` | 0 when unknown |

- Token form `xinput:045e:0b13`, `dinput:044f:b10a`; unknown Xbox model is `xinput:generic` (R6).
- Equality is field-wise. Used as the key for profiles and deadzone.

### ControllerUnitKey

| Field | Type | Notes |
|---|---|---|
| model | `ControllerModelKey` | |
| unitId | `std::wstring` | HID serial when nonempty, else `guidInstance` text; empty for XInput |
| source | enum `Serial`, `InstanceGuid`, `None` | How `unitId` was obtained |

- Token form `<model token>/<unitId>`; XInput units have no `/` part (FR-018a).
- Used as the key for calibration and as the per-machine selection.

### ControllerDeviceInfo (enumeration result)

| Field | Type | Notes |
|---|---|---|
| unit | `ControllerUnitKey` | |
| description | `std::wstring` | Product string, or "Xbox Controller" for XInput |
| xinputSlot | `int` | 0-3, -1 for DirectInput |
| controls | `std::vector<ControlId>` | Controls the device actually reports (R7) |

## Samples and controls

### ControlKind (enum)

`Axis`, `Trigger`, `Button`, `DpadUp`, `DpadDown`, `DpadLeft`, `DpadRight`.

### ControlId

| Field | Type | Notes |
|---|---|---|
| kind | `ControlKind` | |
| index | `int` | Axis 0-7 (X, Y, Z, Rx, Ry, Rz, slider 0, slider 1), trigger 0-1, button 0-127, hat 0-3 for D-pad kinds |

- Token form `axis:1`, `button:3`, `dpad-left:0`, `trigger:1`.
- Xbox mapping of indexes is fixed by the backend (R7): left stick = axis 0/1, right stick = axis 3/4, LT/RT = trigger 0/1, A/B/X/Y = button 0-3, LB/RB = 4/5, Back/Start = 6/7, LS/RS = 8/9, D-pad = hat 0.
- Each `ControlId` has a display label supplied by `ControlLabels` (Xbox: "A", "LT", "D-pad Left"; DirectInput: "Button 3", "Z Axis", "Hat 1 Left").

### ControllerSample

| Field | Type | Notes |
|---|---|---|
| axes | `std::array<float, 8>` | [-1, 1], raw (before calibration) |
| triggers | `std::array<float, 2>` | [0, 1] |
| buttons | `std::bitset<128>` | |
| hats | `std::array<Byte, 4>` | Bit set of up/down/left/right |
| connected | `bool` | false produces a rest sample |

## Calibration and deadzone

### AxisCalibration

| Field | Type | Notes |
|---|---|---|
| center | `float` | Raw value read as center |
| minimum | `float` | Raw value read as full negative |
| maximum | `float` | Raw value read as full positive |

- Invariant: `minimum < center < maximum`; a violation is rejected on load (spec edge case: unreadable data falls back to automatic and is reported).

### ControllerCalibration (per DirectInput unit)

| Field | Type | Notes |
|---|---|---|
| mode | enum `Automatic`, `User` | |
| axes | `std::array<AxisCalibration, 8>` | |

State transitions:

```text
(new unit) --connect--> Automatic[center := sample at connect, limits := center]
Automatic --sample beyond limits--> Automatic[limits widened]
Automatic --Calibrate action applied--> User
User --"Use automatic" applied--> Automatic[recaptured at next connect]
User --connect--> User (no recapture, FR-007a)
```

Only `User` calibration and the automatic limits persist; the automatic center is recaptured each connect.

### Deadzone

Per model: `float` fraction of travel, [0, 0.9]. Defaults and radial/axial rule in R8.

## Mapping and profiles

### AxisBinding

| Field | Type | Notes |
|---|---|---|
| kind | enum `Analog`, `DigitalPair` | |
| analog | `ControlId` | `Axis` or `Trigger` |
| inverted | `bool` | Analog only |
| negative | `ControlId` | DigitalPair: drives 0 while held |
| positive | `ControlId` | DigitalPair: drives 255 while held |

### ButtonBinding

| Field | Type | Notes |
|---|---|---|
| control | `ControlId` | Any kind |
| threshold | `float` | Used when `control` is `Axis` or `Trigger` (R8 defaults); for an axis, deflection in the configured direction |
| negativeDirection | `bool` | Axis only: which side of center counts |

### ControlMapping

| Field | Type | Notes |
|---|---|---|
| pdl0 | `std::vector<AxisBinding>` | Empty = center |
| pdl1 | `std::vector<AxisBinding>` | Empty = center |
| pb0 | `std::vector<ButtonBinding>` | Empty = released |
| pb1 | `std::vector<ButtonBinding>` | Empty = released |

Evaluation rules (`MappingEvaluator`, pure): buttons OR across bindings; an axis takes the binding whose output is furthest from center; digital pair with both directions held reads center.

Default mapping (`DefaultMapping::For (ControllerModelKey, controls)`): PDL0/PDL1 = axis 0/1 (Xbox: left stick); PB0/PB1 = button 0/1 (Xbox: A/B). A device lacking a control leaves that target empty.

### ControllerProfile

| Field | Type | Notes |
|---|---|---|
| name | `std::wstring` | 1-40 characters after trimming; unique per model, case-insensitive |
| mapping | `ControlMapping` | |
| isDefault | `bool` | Exactly one per model; cannot be renamed or deleted |

### ModelSettings (per `ControllerModelKey`)

| Field | Type | Notes |
|---|---|---|
| deadzone | `float` | |
| profiles | `std::vector<ControllerProfile>` | Always contains the Default profile |

### ControllerProfileStore

Owns `std::map<ModelToken, ModelSettings>` and `std::map<UnitToken, ControllerCalibration>`.

| Operation | Rule |
|---|---|
| `GetOrCreateModel (model, controls)` | Creates Default from `DefaultMapping` on first use |
| `CreateProfile (model, name, sourceProfile)` | Refuses empty or duplicate names (FR-027) |
| `RenameProfile`, `DeleteProfile` | Refuse on the Default profile |
| `ResetProfile (model, name)` | Mapping := default mapping (FR-024) |
| `FindProfile (model, name)` | Lookup required for GH #78 disk association |
| `ToJson` / `FromJson` | Per-entry failure isolates to that entry (spec edge case) |

## Per-machine state

### MachineControllerPrefs (in `MachineInputPrefs`)

| Key | Type | Notes |
|---|---|---|
| `controller` | unit token or absent | Absent = no controller selected |
| `controllerProfile` | profile name or absent | Absent or missing profile = Default (FR-029) |

### ControllerSelectionPolicy (pure)

Inputs: machine selection, attached devices, connect events, machine has game port. Outputs: selection changes and notices.

| Event | Selection before | Result |
|---|---|---|
| Connect (incl. present at start or machine switch) | none | Select first device in enumeration order, turn off arrows-to-joystick and mouse-to-paddle, notice (FR-032) |
| Connect | set (any) | No change (FR-032) |
| Connect matching selection | set, disconnected | Controller resumes; arrow fallback ends (FR-008a, FR-010) |
| Disconnect of selected | set, connected | Rest contribution, arrow fallback begins (FR-008a, FR-010) |
| User selects arrows or paddle | set | Selection cleared for the machine |
| Machine has no game port | any | Policy inert (FR-017) |

Xbox-class selection matches any unit of the selected model; with several connected, lowest slot wins.

## Game port mixing

### GamePortContribution

| Field | Type | Notes |
|---|---|---|
| paddle | `std::optional<std::array<Byte, 2>>` | Absent = this source does not drive axes |
| buttons | `std::bitset<2>` | PB0, PB1 |

### GamePortInputMixer (pure, mutex-guarded)

Sources: `FireKeys`, `AppleModifierKeys`, `MousePaddle`, `Controller`. Axis owner: `ArrowKeys`, `MousePaddle`, `Controller`, `None`, chosen from input mode plus the arrow fallback state. Final buttons = OR of all sources; final axes = owner's contribution or center. Writes through `IGamePortSink` only when a final value changes.
