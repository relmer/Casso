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

- Automatic travel on each side is taken as at least `kMinimumTravel` (0.5 of the half range), capped by how far the reading can physically go, until the stick has shown more. Without that floor, limits set to the rest position at connect would read full deflection for the first hair's width of movement.
- A reading at connect is taken as center only within `kMaxRestOffset` (0.25) of zero; further out the stick is being held over, and the center stays 0.
- An automatic axis reads center until it moves more than `kMovedThreshold` (0.02) from its reading at connect. That covers an enumerated axis with no hardware behind it, pinned at a rail (research R3), and a stick held over while it connects.
- Calibrations are held by the controller service and written to the global prefs when Casso exits, and only when they changed. An automatic axis is written only once it has shown travel.

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
| pdl0-pdl3 | `std::vector<AxisBinding>` each | The controller's own PDL0-PDL3 targets; empty = center. Where they land on the machine is the controller's axis assignment (below). `MappingEvaluator::Evaluate` takes an axis count and leaves targets past it absent, so bindings the machine cannot play are ignored without faulting and kept (FR-035) |
| pb0 | `std::vector<ButtonBinding>` | Empty = released |
| pb1 | `std::vector<ButtonBinding>` | Empty = released |
| pb2 | `std::vector<ButtonBinding>` | Empty = released; ignored on the //c (R15) |

Evaluation rules (`MappingEvaluator`, pure, given the elapsed time since the previous sample): buttons OR across bindings; an axis takes the binding whose output is furthest from center; digital pair with both directions held reads center. A rate binding owns an accumulator `float` in [0, 255], advanced by `deflection * maxSpeed * elapsedSeconds` after the deadzone and clamped; the accumulator lives in the evaluator, not the profile, and resets to center on selection, profile or machine change.

Default mapping (`DefaultMapping::For (ControllerModelKey, controls)`): PDL0/PDL1 = axis 0/1 absolute (Xbox: left stick); PB0/PB1 = button 0/1 (Xbox: A/B); PB2 empty. A device lacking a control leaves that target empty.

Paddles template (`DefaultMapping::MakePaddles`): one player's paddle. PDL0 = axis 0 rate (Xbox: left stick X), PB0 = button 0 (Xbox: A); PDL1, PB1 and PB2 unassigned, each bound only when the device reports the control. One controller is one player: a two-player paddle game uses a controller per player, and which paddle each drives is its axis assignment (User Story 7), not its profile. The D-pad is not bound, since a digital pair jumps the axis to either end.

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
| `controllerAxes` | array of `{ "controller": <unit token>, "axes": [<index>...] }`, or absent | Absent or empty = the selected controller holds PDL0/PDL1 (FR-038). Kept for axes the machine lacks (FR-035) |

### ControllerAxisAssignment (per machine, controller-to-axes)

| Field | Type | Notes |
|---|---|---|
| unit | `ControllerUnitKey` | |
| axes | `std::bitset<4>` | The machine axes this controller holds |

Rules (`ControllerSelectionPolicy`, pure):

- **The selection needs no entry.** Without one it holds PDL0 and PDL1 less any axis another entry holds, which is exactly what a machine with only `controller` saved has always done. An entry for it replaces that default.
- **Displacement.** `AssignAxes (unit, axes)` gives the unit exactly those axes and removes each of them from every other entry; the displaced controller keeps its other axes. An entry left holding nothing is kept, so a displaced controller stays off rather than falling back to a default (FR-036).
- **Budget.** `GetAxesFor (assignments, unit, selection, axisCount)` leaves out axes past the machine's count without changing the assignment (FR-034, FR-035). A controller whose axes are all past the count is not read at all, so neither its axes nor its buttons reach a //c.
- **Replacement.** When the selection leaves, a controller with no entry of its own is preferred; an assigned one takes over only when no unassigned one is attached, and keeps its own axes rather than moving onto the freed ones (SC-012).
- **XInput limit.** Xbox-class controllers share one unit key (FR-018a), so two of them cannot hold different axes. Two players need at least one DirectInput controller, or two DirectInput controllers.

**How the assignment combines with the profile: the assignment remaps, the profile binds.** A controller's mapping drives its own PDL0-PDL3 targets; the machine axes it holds, in ascending order, are where those land. A controller holding PDL1 alone plays its `pdl0` bindings on PDL1, and one holding PDL2 and PDL3 plays its `pdl0`/`pdl1` there. So two players on the same Default or Paddles profile need no per-player profile (quickstart 12), and one controller holding all four axes plays a mapping that binds all four as written (quickstart 14, US7 #7).

**What drives the game port** (`ControllerInputService`): the selection, and every controller holding an axis the machine has. Each is read, calibrated and evaluated on its own (its own rate paddles); the service merges them by assignment into the one `Controller` source, with axes a controller does not hold left absent and buttons ORed. A controller that stops reading contributes nothing, so only its axes center and only its buttons release; the others are untouched (SC-012). A pick from the command-bar picker clears the assignments, because the picker chooses the one controller that drives the paddles.

**Picker with several controllers driving.** Every attached controller holding an axis the machine has is checked, and the closed picker reads "N controllers" rather than one controller's name. With the keys or the mouse driving, kept assignments check nothing.

### ControllerSelectionPolicy (pure)

Inputs: machine selection, attached devices, connect events, machine has game port. Outputs: selection changes and notices.

| Event | Selection before | Result |
|---|---|---|
| Connect (incl. present at start or machine switch) | none, or saved and absent | Select the lone attached unit of a saved DirectInput model if there is exactly one, else the first device in enumeration order, turn off arrows-to-joystick and mouse-to-paddle, no notice (FR-032) |
| Connect | set (any) | No change (FR-032) |
| Disconnect of selected | set | Rest contribution; the longest-attached unassigned controller becomes the selection and is persisted, else selection none, which is not persisted; notice naming the controller that left (FR-008a, FR-010, FR-013) |
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

Sources: `FireKeys`, `AppleModifierKeys` (Open-Apple, Solid-Apple, and on the //e Shift as PB2), `MousePaddle`, `Controller`. Axis owner is held **per axis** (`ArrowKeys`, `MousePaddle`, `Controller`, `None`); `SetAxisOwner (owner)` sets all four and `SetAxisOwner (axis, owner)` displaces one axis's owner only (FR-036). Several controllers reach the mixer as the single `Controller` source, merged by the service with each axis held separately, so PDL0 and PDL1 can belong to different controllers. Final buttons = OR of all sources; each final axis = that axis's owner's contribution or center. Writes through `IGamePortSink` only when a final value changes.
