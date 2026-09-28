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
| unitId | `std::string` | HID serial when nonempty, else `guidInstance` text; the XInput slot (0-3) for XInput; empty for an XInput unit read from a file written before slots. Narrow text, since it is stored in JSON |
| source | enum `Serial`, `InstanceGuid`, `XInputSlot`, `None` | How `unitId` was obtained |

- Token form `<model token>/serial:<unitId>` or `<model token>/guid:<unitId>` for DirectInput, and `xinput/product:<vvvv>:<pppp>` for XInput, with `:<n>` appended from 2 up for a second unit of the same product; a unit with source `None` (a DirectInput unit with no identity, and an XInput unit from a file written before slots) is just its model token. The model part ends at the first `/`, so a serial containing `/` round trips. A bare `xinput` still loads, as an XInput unit with no slot, meaning whichever Xbox-class controller is connected; a slot past 3, a missing slot, a malformed product or an ordinal below 2, `xinput/serial:` and `dinput:.../slot:` are refused. `xinput/slot:<n>`, written before units were keyed by product, still loads: a player slot naming one adopts the unit in that XInput slot the first time one is there, and is saved with its product key. XInput hands out slots in connection order, so a slot key followed the slot rather than the controller, and powering two controllers on in the other order swapped the players. Two controllers of the same product still cannot be told apart and are numbered in slot order.
- The MODEL token never carries the slot (FR-018a), so profiles, deadzone and calibration stay shared by every Xbox-class controller while two of them are two units.
- Used as the key for calibration and as the per-machine selection.

### ControllerDeviceInfo (enumeration result)

| Field | Type | Notes |
|---|---|---|
| unit | `ControllerUnitKey` | |
| description | `std::wstring` | Product string, or "Xbox Controller (045e:0b13)" for XInput, with ` #<slot+1>` appended while more than one is connected |
| xinputSlot | `int` | 0-3, -1 for DirectInput |

Every connected XInput slot is enumerated as its own device, so two Xbox controllers produce two entries. The appended slot number only keeps the descriptions distinct in a list; which controller is which is settled by moving a stick and watching the Controllers page readout.
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
| maxSpeed | `float` | Rate only: paddle units per second at full deflection, [16, 1024], default 768 (`AxisBinding::kDefaultMaxSpeed`; 2026-09-28, chosen by the owner by feel, one sweep in about a third of a second; it was 256). A binding saved with a speed keeps it |
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
| pdl0-pdl3 | `std::vector<AxisBinding>` each | The controller's own PDL0-PDL3 targets; empty = center. Where they land on the machine is the player's route, set by the two players' modes ([Players](#players-2026-09-27)); before 2026-09-27 it was the player slot's target, or PDL0/PDL1 in single-source mode. `MappingEvaluator::Evaluate` takes an axis count and leaves targets past it absent, so bindings the machine cannot play are ignored without faulting and kept (FR-035) |
| pb0 | `std::vector<ButtonBinding>` | Empty = released |
| pb1 | `std::vector<ButtonBinding>` | Empty = released |
| pb2 | `std::vector<ButtonBinding>` | Empty = released; ignored on the //c (R15) |

Evaluation rules (`MappingEvaluator`, pure, given the elapsed time since the previous sample): buttons OR across bindings; an axis takes the binding whose output is furthest from center; digital pair with both directions held reads center. A rate binding owns an accumulator `float` in [0, 255], advanced by `deflection * maxSpeed * elapsedSeconds` after the deadzone and clamped; the accumulator lives in the evaluator, not the profile, and resets to center on selection, profile or machine change.

Default mapping (`DefaultMapping::For (ControllerModelKey, controls)`): PDL0/PDL1 = axis 0/1 absolute (Xbox: left stick); PB0/PB1 = button 0/1 (Xbox: A/B); PB2 empty. A device lacking a control leaves that target empty.

Paddles template (`DefaultMapping::MakePaddles`): one player's paddle. PDL0 = axis 0 rate (Xbox: left stick X), PB0 = button 0 (Xbox: A); PDL1, PB1 and PB2 unassigned, each bound only when the device reports the control. One controller is one player: a two-player paddle game uses a controller per player, and which paddle each drives is their slot's target (User Story 7), not their profile. (Superseded 2026-09-27: which paddle each drives follows the two players' modes, FR-039; see [Players](#players-2026-09-27).) The template binds PB0 alone, which is also the only button line a player in Paddle mode drives (FR-039). Since 2026-09-27 it is the mapping of the built-in Paddles profile, the Paddle kind's built-in (FR-043), and the Paddle kind's "Paddles mapping" starting point (`ProfileSource::PaddleMapping`). The D-pad is not bound, since a digital pair jumps the axis to either end. (Superseded 2026-09-28: the template is a player's two paddles, for Two paddles mode. `MakePaddles (model, formFactor, controls)` also puts a second X axis on PDL1 at Rate and the default speed, with button 1 (Xbox: B) on PB1: an Xbox-class controller's right stick X, and on another controller the first axis of the second stick `FindSecondStick` finds, which is none on a joystick or a wheel. A controller with no second stick binds PDL0 and PB0 alone. Paddle mode still plays only PDL0 and PB0.)

### ControllerProfile

| Field | Type | Notes |
|---|---|---|
| name | `std::wstring` | 1-40 characters after trimming; unique per model, case-insensitive |
| mapping | `ControlMapping` | |
| isDefault | `bool` | Exactly one per model; cannot be renamed or deleted. (Superseded: replaced by `kind`, below.) |
| kind | `ControllerProfileKind` | `User`, or a built-in: `Default`, `Paddles`, `Joyport`. A built-in cannot be renamed or deleted, and each model always has all three. A user profile saved with the name Paddles is read as the built-in one |
| mode | `ProfileMode` | `Joystick`, `Paddle` or `Joyport` (FR-043, spec 036 FR-020). A built-in's mode comes from its kind: Default is Joystick, Paddles is Paddle, Joyport is Joyport. A profile saved without one is classified on load: `pdl0` bound and `pdl1` not is Paddle, else Joystick; a legacy `joyport` flag stays Joyport |

`ProfileSource`, the starting point of a new profile, is per kind: Joystick takes `DefaultMapping` or `CopyOfProfile`, Paddle takes `PaddleMapping` ("Paddles mapping") or `CopyOfProfile`, Joyport takes `JoyportMapping` or `CopyOfProfile`. `PaddleMapping` replaced `Paddles`, which had been a normal-mode starting point beside the Default mapping. (Superseded 2026-09-27: normal mode split into the Joystick and Paddle kinds.)

### ModelSettings (per `ControllerModelKey`)

| Field | Type | Notes |
|---|---|---|
| deadzone | `float` | |
| profiles | `std::vector<ControllerProfile>` | Always contains the Default profile |

### ControllerProfileStore

Owns `std::map<ModelToken, ModelSettings>`, `std::map<UnitToken, ControllerCalibration>`, and `activeProfiles`, a `std::map<UnitToken, std::string>` giving each controller's active profile; an empty name or no entry is the Default (FR-029). Since 2026-09-27 there is one such map per profile kind: `activeProfiles` (Joystick), `paddleActiveProfiles` (Paddle) and `joyportActiveProfiles` (Joyport); an empty name or no entry is that kind's built-in profile. A controller plays its choice for the kind in effect for its player (its player's mode, Joystick for a controller in no slot, Joyport while the Joyport is attached; since 2026-09-28, Joyport only for a controller whose player is on a jack). On read, a Joystick choice of a profile that is now a Paddle profile moves to the Paddle map unless that controller has a Paddle choice already (`MoveLegacyPaddleChoices`).

| Operation | Rule |
|---|---|
| `GetOrCreateModel (model, controls)` | Creates Default from `DefaultMapping` on first use |
| `CreateProfile (model, name, sourceProfile)` | Refuses empty or duplicate names (FR-027) |
| `RenameProfile`, `DeleteProfile` | Refuse on the Default profile |
| `ResetProfile (model, name)` | Mapping := the built-in mapping of the profile's kind (FR-024, spec 036 FR-020) |
| `FindProfile (model, name)` | Lookup required for GH #78 disk association |
| `ToJson` / `FromJson` | Per-entry failure isolates to that entry (spec edge case) |

## Per-machine state

**Superseded (2026-09-27)**: the per-machine selection, the per-machine `multiplayer` block, the single-source/multiplayer modes and the connect-time policy table below describe the design before GH #156. They are kept as the record of what Phase 4-9 built. What replaces them is under [Players](#players-2026-09-27) at the end of this file; `MultiplayerSlot`, `PlayerAxisTarget`, `Normalize`, `GetTargetAxes` and `GetTargetChoices` carry over. (Superseded later on 2026-09-27: `GetTargetChoices` and `GetAxesForPlayer` are removed with the per-slot target, and `PlayerAxisTarget` survives only as an internal routing value set from the players' modes.) The per-machine keys are now read only by the one-time adoption and the legacy profile move ([research R19](research.md)).

### MachineControllerPrefs (in `MachineInputPrefs`)

| Key | Type | Notes |
|---|---|---|
| `controller` | unit token or absent | Absent = no controller selected |
| `controllerProfile` | profile name or absent | Legacy, read only: moved once to the saved `controller`'s entry in `activeProfiles` when it has none (FR-029) |
| `multiplayer` | `{ "enabled": <bool>, "players": [ <slot>, <slot> ] }`, or absent | Absent or not enabled = single-source mode: the selected controller drives PDL0/PDL1 and PB0-PB2 (FR-037, FR-038). A slot is `{ "controller": <unit token>, "maps": <target token> }`; slots are kept for paddles the machine lacks (FR-035) |

### MultiplayerSetup (per machine, two player slots)

| Field | Type | Notes |
|---|---|---|
| isEnabled | `bool` | False = single-source mode, which is what a machine has always done |
| players | `std::array<MultiplayerSlot, 2>` | Exactly two: the game port reads two buttons a game can tell apart, so a third player has no line to be given |

| MultiplayerSlot field | Type | Notes |
|---|---|---|
| unit | `std::optional<ControllerUnitKey>` | Absent = an empty slot, which plays nothing |
| target | `PlayerAxisTarget` | `Joystick0` (PDL0/PDL1), `Joystick1` (PDL2/PDL3), `Paddle0`-`Paddle3`, or (2026-09-28) `Paddles01` (PDL0/PDL1 as two paddles, PB0 and PB1) and `Paddles23` (PDL2/PDL3 as two paddles, PB2) |

Rules (`ControllerSelectionPolicy`, pure):

- **Normalization.** `Normalize (setup)` empties the SECOND slot when it repeats the first slot's controller or claims a paddle the first already holds (FR-036). The later slot gives way, so the player whose choice was refused sees an empty slot rather than a paddle that quietly does nothing. Every setter and the prefs reader run through it, so a hand-edited file cannot be played as written.
- **Budget.** `GetTargetAxes (target, axisCount)` and `GetAxesForPlayer (setup, player, axisCount)` leave out paddles past the machine's count without changing the slot (FR-034, FR-035). A player with none of their paddles on this machine is not read at all, so neither their paddles nor their button reach a //c.
- **Choices.** `GetTargetChoices (setup, player, axisCount)` is what the settings page offers one slot: every target the machine has all the paddles for, less what the other player holds. A target the machine can play only half of is not offered, because half a joystick is not a choice anyone made.
- **Replacement.** When the selection leaves, a controller no player is holding is preferred; one a player holds takes the selection only when no free controller is attached, and goes on playing its own paddles (SC-012).
- **Xbox-class units.** Xbox controllers share one model key but have a unit key each, the XInput slot (FR-018a), so two of them can fill the two player slots.
- **Adoption covers XInput too.** A saved unit that is absent while exactly one unit of its model is attached is adopted. For an Xbox-class selection that means a controller whose slot changed across a replug, and a selection saved before slots existed, are both picked up again.

**How a slot combines with the profile: the slot remaps, the profile binds.** A player's controller uses its own mapping; the paddles its slot maps to, in ascending order, are where the mapping's `pdl0`.. targets land. With a slot mapped to `Paddle1`, only the controller's `pdl0` bindings are used, on PDL1; with one mapped to `Joystick1`, its `pdl0`/`pdl1` bindings land on PDL2/PDL3. So two players share one Default or Paddles profile with no per-player copy (quickstart 12).

**Buttons follow the player** (FR-039). Player one's `pb0` bindings drive PB0 and player two's drive PB1; a player's `pb1` and `pb2` bindings are kept in the profile and ignored while the mode is on, and PB2 is unused. OR-ing every controller's buttons together, which is what single-source mode still does across the keyboard and mouse sources, made the two lines indistinguishable to a two-player game.

**What drives the game port** (`ControllerInputService`): in single-source mode the selection alone; in multiplayer the two players whose paddles this machine has. Each is read, calibrated and evaluated on its own (its own rate paddles) and merged into the one `Controller` source, with paddles a player does not drive left absent. A controller that stops reading contributes nothing, so only that player's paddles center and only their button line releases; the other is untouched (SC-012). A pick from the command-bar picker turns multiplayer off and keeps both slots.

**Picker while two people play.** Both players' controllers are checked, and the closed picker reads "N controllers" rather than one controller's name. With the keys or the mouse driving, or with the mode off, kept slots check nothing.

### ControllerSelectionPolicy (pure)

Inputs: machine selection, attached devices, connect events, machine has game port. Outputs: selection changes and notices.

| Event | Selection before | Result |
|---|---|---|
| Connect (incl. present at start or machine switch) | none, or saved and absent | Select the lone attached unit of a saved DirectInput model if there is exactly one, else the first device in enumeration order, turn off arrows-to-joystick and mouse-to-paddle, no notice (FR-032) |
| Connect | set (any) | No change (FR-032) |
| Disconnect of selected | set | Rest contribution; the longest-attached controller no player is holding becomes the selection and is persisted, else selection none, which is not persisted; notice naming the controller that left (FR-008a, FR-010, FR-013) |
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

## Players (2026-09-27)

Two player slots, always present and global (FR-037). Research [R16-R25](research.md#2026-09-27-two-always-present-player-slots-gh-156).

### PlayerEntryKind (enum)

`Automatic`, `Controller`, `ArrowKeys`, `MousePaddle`, `Disabled`. `ArrowKeys` and `MousePaddle` are valid for Player 1 only and `Disabled` for Player 2 only (FR-008); a value outside its player's set reads as `Automatic`.

### PlayerEntry (global, one per player)

| Field | Type | Notes |
|---|---|---|
| kind | `PlayerEntryKind` | Default `Automatic` for both players (FR-037) |
| unit | `std::optional<ControllerUnitKey>` | Set only for `Controller`: the picked controller, attached or not (FR-011, FR-041) |
| target | `std::optional<PlayerAxisTarget>` | (Superseded 2026-09-27: removed; the player's `mode` replaced the per-slot target and the rule that it followed the active profile.) Set only when the user chose the target on the Controllers page; absent = follow the active profile (FR-043, R18) |
| mode | `PlayerMode` | `Joystick`, `JoyportLeft`, `JoyportRight`, `Paddle`, `SameAsPlayer1` (Player 2 only) or `TwoPaddles`, listed Joystick, the jacks, Paddle, Two paddles (FR-037, FR-039; 2026-09-28). Player 1 defaults to `Joystick` and Player 2 to `SameAsPlayer1`. Saved globally with the entry. (Superseded 2026-09-28: `Joystick` (default) or `Paddle`.) |

- `PlayerEntries` is `std::array<PlayerEntry, 2>`. Player 2's `Disabled` entry is how two-player play is turned off. (Superseded 2026-09-27: the Multiplayer checkbox, which was `entries[1].kind != Disabled`, is gone.)
- `ApplyPick` keeps the player's mode. `NormalizeEntries` makes the keys play in Joystick mode and the mouse in Paddle mode. `ApplyMode (entries, player, mode)` sets a mode and returns Player 1 to `Automatic` when it holds the keys and the mode becomes Paddle, or holds the mouse and the mode becomes Joystick. `GetEffectiveMode (entry, isJoyport)` is the entry's mode, or Joystick while the Joyport is in effect. (Superseded 2026-09-28: `GetEffectiveMode` is removed; `PlayerModeRules::ResolveMode` gives the mode a player plays, and the keys play in Joystick or a Joyport mode. The paddle modes, `PlayerModeRules::IsPaddleMode`, are Paddle and Two paddles: the keys leave either for Joystick, and the mouse keeps Two paddles mode and otherwise takes Paddle.)
- The two entries may not pick the same controller (FR-036). Picking a controller the other entry holds sets the other entry to `Automatic` (FR-041).

### PlayerSlotState (enum)

| State | Meaning | Counts as playing |
|---|---|---|
| `Empty` | No holder: Automatic waiting, `Disabled`, or keys/mouse (Player 1) | Keys and mouse yes; otherwise no |
| `Provisional` | Automatic's lone Player 1 before any input (FR-032, R16) | No, but drives the port |
| `Waiting` | A picked controller that is not attached while the other slot is not in use. (Before 2026-09-28 also a holder Automatic assigned by connection order before it was used; such a holder is now `Playing` at once.) | No |
| `Playing` | Picked and attached, or assigned by Automatic: by its first input, or by connection order, which also puts a lone `Provisional` Player 1 that connected while Casso runs in use (2026-09-28) | Yes |
| `Held` | Holder left while the other slot was playing; kept for it (FR-040, R17) | No; blocks the one-playing rule |

### PlayerSlot (runtime, one per player, computed by `PlayerSlotPolicy`)

| Field | Type | Notes |
|---|---|---|
| state | `PlayerSlotState` | |
| holder | `std::optional<ControllerUnitKey>` | The controller in the slot, including a held one that is absent |
| isPicked | `bool` | From the entry; false for Automatic |
| target | `PlayerAxisTarget` | Internal routing value, `PlayerTargetRules::GetModeTarget` over the two players' effective modes. (Superseded 2026-09-27: it was the entry's `target` if set, else `GetAutomaticTarget` over the holder's active profile, R18.) |

### PlayerOrderLogs (runtime, held by `ControllerInputService`)

| Field | Type | Notes |
|---|---|---|
| connected | `std::vector<ControllerUnitKey>` | Distinct controllers that arrived after the startup enumeration, in order; a return to a held slot and a repeat are not added (R16) |
| firstInput | `std::vector<ControllerUnitKey>` | Controllers whose first real input came while Casso was active, in order (FR-033, FR-042) |

Both are cleared of every controller that holds no slot when Automatic starts over (FR-040).

### LastHolders (global)

`std::array<std::optional<ControllerUnitKey>, 2>`: the controller that last held each slot, however it got there. Used only to decide whether an Automatic assignment shows a notice (FR-044, R21); never used to assign.

### PlayerSlotPolicy (pure)

Inputs: `PlayerEntries`, attached devices, `PlayerOrderLogs`, the previous slots (for `Held`), the axis count, and whether the Joyport is in effect (`isJoyportInEffect`; both players are Joystick while it is; superseded 2026-09-28: `hasJoyport`, whether the machine has one, and each player plays its resolved mode). Before 2026-09-27 it took each candidate's active profile mapping for targets (superseded: targets come from the modes). Output: `std::array<PlayerSlot, 2>`, plus the assignment notices to show and whether `LastHolders` changed.

| Operation | Rule |
|---|---|
| `Evaluate` | R16 ordering and states; R17 held slots and start over; targets from the modes (`GetModeTarget`; per R18 before 2026-09-27, superseded); `Normalize` over the result (FR-036) |
| `GetPlayerRoute (slots, entries, player, ...)` | The route a player's controller takes, whether or not it is driving: the lone route while it plays alone, else its target's route (FR-039) |
| `GetDriverRoute (slots, entries, ...)` | The route of the one player driving the port alone, if there is one |
| `IsRealInput (sample, calibration, deadzone)` | A button or D-pad press, a trigger past its threshold, or any axis outside its deadzone after calibration (R16) |
| `IsOnePlaying (slots)` | Exactly one slot `Playing` or `Provisional` (or Player 1 on keys/mouse) and the other `Empty` or `Waiting`, not `Held` |
| `DescribeAssignment (player, description)` | "Player 1: description"; spec 036 substitutes its Joyport labels here. (Superseded 2026-09-28: `PlayerModeRules::DescribeAssignment (player, description, jacks)` adds "(Joyport left)", "(Joyport right)" or "(Joyport left and right)" after the player.) |
| `NeedsIdleWatch (entries, slots)` | Whether any Automatic player is `Empty`, `Waiting` or `Provisional`, which turns on R23's watch |

### PlayerTargetRules (pure)

| Operation | Rule |
|---|---|
| `GetAutomaticTarget (player, mapping, otherTarget)` | (Superseded 2026-09-27: removed with `IsPaddleMapping`; replaced by `GetModeTarget`.) Paddle when `pdl0` has a binding and `pdl1` has none, else joystick; Player 1 gets Joystick 0 / Paddle 0; Player 2 gets Joystick 1, or the lowest paddle Player 1 does not hold |
| `GetModeTarget (player, isPaddle, isTwoPaddles, isPlayerOnePaddle)` | Player 1: `Joystick0`, `Paddle0` in Paddle mode, or `Paddles01` in Two paddles mode. Player 2: `Joystick1`, `Paddles23` in Two paddles mode, or in Paddle mode `Paddle1` beside a Paddle Player 1 and `Paddle2` beside a Joystick or Two paddles Player 1 |
| `GetButtonRoute (target)` | Which of `pb0`/`pb1` reaches which of PB0-PB2 (table below) |
| `GetSingleRoute ()` | The one-playing route: `pdl0`/`pdl1` to PDL0/PDL1, `pb0`-`pb2` to PB0-PB2 |
| `GetLoneRoute (target, axisCount)` | A player playing alone: Joystick mode drives PDL0, PDL1 and PB0-PB2 (`GetSingleRoute`); Paddle mode drives PDL0 and PB0; Two paddles mode drives PDL0, PDL1, PB0 and PB1 |

Routing by mode (FR-039). Only the modes decide; the profile does not. A player's controller's `pdl0`.. bindings land on its paddles in ascending order.

| Player 1 mode | Player 2 mode | Player 1 drives | Player 2 drives |
|---|---|---|---|
| Joystick | Joystick | PDL0, PDL1, PB0, PB1 | PDL2, PDL3, PB2 (from its `pb0`) |
| Joystick | Paddle | PDL0, PDL1, PB0, PB1 | PDL2, PB2 |
| Paddle | Joystick | PDL0, PB0 | PDL2, PDL3, PB2 |
| Paddle | Paddle | PDL0, PB0 | PDL1, PB1 |
| Joystick, alone | -- | PDL0, PDL1, PB0-PB2 | -- |
| Paddle, alone | -- | PDL0, PB0 | -- |
| Joystick | Two paddles | PDL0, PDL1, PB0, PB1 | PDL2, PDL3, PB2 (2026-09-28) |
| Two paddles | Joystick | PDL0, PDL1, PB0, PB1 | PDL2, PDL3, PB2 |
| Two paddles | Paddle | PDL0, PDL1, PB0, PB1 | PDL2, PB2 |
| Paddle | Two paddles | PDL0, PB0 | PDL2, PDL3, PB2 (PDL1 unused) |
| Two paddles | Two paddles | PDL0, PDL1, PB0, PB1 | PDL2, PDL3, PB2 |
| Two paddles, alone | -- | PDL0, PDL1, PB0, PB1 | -- |

What a machine lacks plays nothing (FR-035): on the //c, with no PDL2/PDL3, a second player in Joystick mode drives nothing, and neither does a second player beside Player 1's two paddles (2026-09-28). While the Joyport is in effect both players route as Joystick. (Superseded 2026-09-28: see "Joyport modes" below.)

### State transitions (one slot on Automatic)

```text
Empty --lone candidate attached, no input yet--> Provisional
Provisional --its first real input--> Playing
Provisional --another candidate's first real input--> Empty (that candidate fills the slot per R16)
Empty --R16 picks a holder--> Playing (2026-09-28; before, Waiting until its first input)
Provisional --a second controller connects after it, both connected while Casso runs--> Playing (2026-09-28)
Playing --holder leaves, other slot Playing--> Held
Playing --holder leaves, other slot not Playing--> Empty (start over)
Held --holder returns--> Playing
Held --other slot stops playing--> Empty (start over)
```

A picked slot is `Playing` while its controller is attached. While it is not attached it is `Held` if the other slot is in use, so the remaining player is not given its lines (FR-040), and `Waiting` otherwise; a disconnect never rewrites the entry. Player 1 on keys or the mouse is `Empty` with those sources driving through the mixer as before.

### What the game port gets (`ControllerInputService::BuildMergedLocked`)

| Slots | Player 1 drives | Player 2 drives |
|---|---|---|
| One playing (`IsOnePlaying`) | If it is the one: `GetLoneRoute` for its mode (before 2026-09-27, `GetSingleRoute` whatever its target) | If it is the one: the same |
| Both playing | Its target's paddles and `GetButtonRoute` | Its target's paddles and `GetButtonRoute` |
| One playing, one `Held` | Its own target only | Its own target only; the held slot drives nothing |

Paddles and lines a player does not drive are absent from the contribution, so they rest at center or released.

## Notice stack (2026-09-27)

`Dxui/Widgets/DxuiNoticeStack.h/.cpp` ([contracts/notice-stack.md](contracts/notice-stack.md), research R20).

| Entity | Fields | Notes |
|---|---|---|
| Notice | `DxuiTimedInfoBanner` (text, `untilMs`), `slide` (`DxuiSlide`) | Own full duration; `slide` is the offset still to travel after a notice above expired |
| DxuiNoticeStack | ordered notices, bounds, `nowMs` of the last tick | Arrival order top to bottom |
| DxuiSlide | `startMs`, `distancePx`, `isAnimated`, duration `DxuiPopupMenu::kRevealMs`, `DxuiTweenEase::EaseOut` | Returns the end at once when `isAnimated` is false; the caller passes the menu animation setting in |

```text
(push) --> Showing[below the last]
Showing --its untilMs passes--> removed; each notice below it starts a slide up by the removed notice's height
Sliding --duration passes--> Showing at its new place
```

## Saved state added (2026-09-27)

In the global `controllers` section ([contracts/prefs-schema.md](contracts/prefs-schema.md)): `players` (two `PlayerEntry` records, each with its `mode`; its presence marks the one-time adoption as done) and `lastHolders` (two unit tokens or null). Later the same day: `paddleActiveProfiles` and `joyportActiveProfiles` beside `activeProfiles`, and each profile's `profileMode`. Per machine: `controller` and `multiplayer` are no longer written and are read only by the adoption and the legacy profile move; `arrowsToJoystick` and a `pointerMapping` of `paddle` are read only by the adoption; a `pointerMapping` of `mouse` (the //c IOU mouse) stays per machine.

2026-09-28: each player's `mode` may also be `joyportLeft`, `joyportRight`, `twoPaddles` or (Player 2) `sameAsPlayer1`. The global `gamePortAdapter` is read by the migration only while no player has a saved `mode`, and is then removed from the global prefs.

## Joyport modes (2026-09-28)

Research R27; spec 036 data-model holds `PlayerModeRules`' full table and the jack assignment.

`PlayerModeRules` (`CassoEmuCore/Controllers/PlayerModeRules.h/.cpp`, pure) resolves each player's mode (`ResolveMode`: `SameAsPlayer1` is Player 1's mode, or the other jack while Player 1 is on one; a jack is `Joystick` on a machine without a Joyport) and provides `IsJoyportOn`, `GetJack`, `IsModeTaken`, `AreButtonsCut`, `ArePaddlesConnected`, `IsPaddleMode`, `AreKeysOffered`, `IsMouseOffered` (Player 1 in Paddle or Two paddles mode), `BuildModeChoices`, `GetModeLabel`, `GetPlayerLabel`, `DescribeAssignment` and the migration.

Routing with a player on a jack (FR-039):

| Player 1 resolved | Player 2 resolved | Player 1 drives | Player 2 drives |
|---|---|---|---|
| a jack | a jack | its jack (both while Player 2 is idle) | its jack (both while Player 1 is idle) |
| a jack | Joystick | its jack and, the other being free, both | PDL0, PDL1; no button line |
| a jack | Paddle | as above | PDL0; no button line |
| Joystick | a jack | PDL0, PDL1; no button line | its jack and, the other being free, both |
| Paddle | a jack | PDL0; no button line | as above |
| a jack, alone | -- | both jacks | -- |

Player 1 on the keys in a jack mode drives its jack or jacks from the arrow and fire keys and no paddle; Player 1 on the mouse (Paddle) drives PDL0 alone while Player 2 plays. The paddle inputs read as no paddle connected only while every playing player is on a jack.

Each controller plays the profile kind of its own player's resolved mode: Joyport for a jack, otherwise Joystick or Paddle.
