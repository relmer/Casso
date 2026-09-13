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

- Token form: `xinput` for every Xbox-class controller (FR-018a), `dinput:044f:b10a` for the rest.
- An XInput key always carries vendor and product 0; the real IDs live in `ControllerDeviceInfo::description` for display. Measured reason: one controller reported 045E:02FF over USB and 045E:0B13 over Bluetooth.
- Equality is field-wise. Used as the key for profiles and deadzone.

### ControllerUnitKey

| Field | Type | Notes |
|---|---|---|
| model | `ControllerModelKey` | |
| unitId | `std::string` | HID serial when nonempty, else `guidInstance` text; empty for XInput. Narrow text, since it is stored in JSON |
| source | enum `Serial`, `InstanceGuid`, `None` | How `unitId` was obtained |

- Token form `<model token>/serial:<unitId>` or `<model token>/guid:<unitId>`; a unit with source `None` (every XInput unit, and a DirectInput unit with no identity) is just its model token (FR-018a). The model part ends at the first `/`, so a serial containing `/` round trips.
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
| response | enum `Absolute`, `Rate` | Analog only (R14) |
| maxSpeed | `float` | Rate only: paddle units per second at full deflection, [16, 1024], default 256 |
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
| pdl | `std::array<std::vector<AxisBinding>, 4>` | PDL0-PDL3; empty = center. Indices 2 and 3 are never evaluated on a machine with two axes (FR-035) |
| pb0 | `std::vector<ButtonBinding>` | Empty = released |
| pb1 | `std::vector<ButtonBinding>` | Empty = released |
| pb2 | `std::vector<ButtonBinding>` | Empty = released; ignored on the //c (R15) |

Evaluation rules (`MappingEvaluator`, pure, given the elapsed time since the previous sample): buttons OR across bindings; an axis takes the binding whose output is furthest from center; digital pair with both directions held reads center. A rate binding owns an accumulator `float` in [0, 255], advanced by `deflection * maxSpeed * elapsedSeconds` after the deadzone and clamped; the accumulator lives in the evaluator, not the profile, and resets to center on selection, profile or machine change.

Default mapping (`DefaultMapping::For (ControllerModelKey, controls)`): PDL0/PDL1 = axis 0/1 absolute (Xbox: left stick); PB0/PB1 = button 0/1 (Xbox: A/B); PB2 empty. A device lacking a control leaves that target empty.

Paddles template (`DefaultMapping::MakePaddles`): PDL0 = axis 0 rate, PDL1 = axis 3 rate (Xbox: right stick X; DirectInput: Rx when reported, else empty), PB0/PB1 = button 0/1.

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
| Connect (incl. present at start or machine switch) | none, or saved and absent | Select the lone attached unit of a saved DirectInput model if there is exactly one, else the first device in enumeration order, turn off arrows-to-joystick and mouse-to-paddle, no notice (FR-032) |
| Connect | set (any) | No change (FR-032) |
| Disconnect of selected | set | Rest contribution; the longest-attached unassigned controller becomes the selection for the session (the saved controller is unchanged), else selection none; notice naming the controller that left (FR-008a, FR-010, FR-013) |
| User selects arrows or paddle | set | Selection cleared for the machine |
| Machine has no game port | any | Policy inert (FR-017) |

Xbox-class selection matches any unit of the selected model; with several connected, lowest slot wins.

## Game port mixing

### GamePortContribution

| Field | Type | Notes |
|---|---|---|
| paddle | `std::array<std::optional<Byte>, 4>` | Per axis: absent = this source does not drive that axis. Per-axis rather than all-or-nothing, so two controllers can hold PDL0 and PDL1 separately (FR-036) |
| buttons | `std::bitset<3>` | PB0, PB1, PB2 |

### GamePortInputMixer (pure, mutex-guarded)

Sources: `FireKeys`, `AppleModifierKeys` (Open-Apple, Solid-Apple, and on the //e Shift as PB2), `MousePaddle`, `Controller`. Axis owner is held **per axis** (`ArrowKeys`, `MousePaddle`, `Controller`, `None`), chosen from input mode and the selection, so PDL0 and PDL1 can belong to different controllers (FR-036). Final buttons = OR of all sources; each final axis = that axis's owner's contribution or center. Writes through `IGamePortSink` only when a final value changes.
