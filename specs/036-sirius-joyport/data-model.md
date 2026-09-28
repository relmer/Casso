# Data Model: Sirius Joyport Emulation

**Feature**: [spec.md](spec.md) | **Research**: [research.md](research.md)

## GamePortAdapter (enum, API type)

`CassoEmuCore/Controllers/ControllerTypes.h`, beside the other input types.

| Value | Token | Meaning |
|---|---|---|
| `None` | `"none"` | The game port as it has always been emulated. Default. |
| `SiriusJoyport` | `"siriusJoyport"` | The Joyport in Atari mode, Controller Select at Center. |

- Token conversion in `ControllerTokens` (`GamePortAdapterToToken`,
  `GamePortAdapterFromToken`). An unknown or missing token reads as `None`.
- Total over its implementations: a test sweeps the enum and requires a token
  for every value.

## JoystickSwitches (value, API type)

`ControllerTypes.h`. One Atari joystick's five switches: the spec's "Atari
joystick state" entity, one per jack. An alias,
`using JoystickSwitches = std::bitset<5>`, indexed by `JoystickSwitch`: `Up`,
`Down`, `Left`, `Right`, `Fire`. Set = closed.

- Invariant: `Up` and `Down` are never both set; neither are `Left` and
  `Right` (FR-007). The evaluator guarantees it for controllers; the keyboard
  path cannot violate it because the arrow source never reports both ends.
- Default: all open.

## JoyportJacks (value, API type)

`ControllerTypes.h`.

| Field | Type | Notes |
|---|---|---|
| `jack` | `std::array<JoystickSwitches, 2>` | `[0]` left jack (player 1, AN0 off), `[1]` right jack (player 2, AN0 on). |

## Changes to existing types

| Type | File | Change |
|---|---|---|
| `GamePortContribution` | `ControllerTypes.h` | + `JoystickSwitches switches` (the evaluator's per-controller result), + `std::optional<JoyportJacks> jacks` (set only on the Controller source's merged contribution). |
| `GamePortState` | `GamePortInputMixer.h` | + `JoyportJacks jacks` (what the sink writes). |
| `MachineDefinition` | `Machines/MachineDefinition.h` | + `bool hasAnnunciators` (true for the ][, ][+, //e, enhanced //e). |
| `GamePortTargets` | `Shell/MachineGamePortSink.h` | + `SiriusJoyport * joyport` (null on the //c). |
| `MachineHost` | `Shell/MachineHost.h` | + owns `std::unique_ptr<SiriusJoyport>`, `GetJoyport` / `SetJoyport`, beside the mouse. (`MachineRefs` holds only pointers into the owned device list, so it gains nothing.) |
| `IMachine` | `Machines/IMachine.h` | + `HasAnnunciators()`, the source of `MachineDefinition::hasAnnunciators`. |
| `SettingsUiPrefs` | `Ui/Settings/SettingsPanelState.h` | + `GamePortAdapter gamePortAdapter = None`. (Removed 2026-09-27, R15.) |
| `ISettingsApplySink` | `SettingsPanelState.h` | + `ApplyGamePortAdapter (GamePortAdapter)`. (Removed 2026-09-27, R15.) |

## Annunciator state

Held by `AppleSoftSwitchBank` (and so by `Apple2eSoftSwitchBank`).

| Field | Type | Notes |
|---|---|---|
| `m_annunciators` | `std::atomic<Byte>` | Bit n = ANn on, n = 0-2. Written on the CPU thread, read on the CPU thread by the Joyport; atomic only so the Controllers page or a test can read it without a race. |

Transitions: an access to `$C058 + 2n` clears bit n; `$C059 + 2n` sets it
(n = 0-2). On the //c, not while IOU access is on. Cleared by `PowerCycle`, kept
by `SoftReset`.

## SiriusJoyport (device model)

`CassoEmuCore/Machines/Apple2/Common/SiriusJoyport.{h,cpp}`. Owned by
`MachineHost`; built only when `hasAnnunciators`.

| Field | Type | Notes |
|---|---|---|
| `m_annunciatorSource` | `const AppleSoftSwitchBank *` | Wired by `MachineBuilder`. |
| `m_cycleSource` | `const uint64_t *` | The CPU's total-cycle counter. |
| `m_isAttached` | `std::atomic<bool>` | Set from the UI thread. |
| `m_jacks` | `std::array<std::atomic<unsigned long>, 2>` | Each jack's `JoystickSwitches` bits, written by the sink. |
| `m_resetCycle` | `uint64_t` | Stamp from the last `OnMachineReset`. |
| `m_hasResetStamp` | `bool` | Set by the first `OnMachineReset`. The window is `now - m_resetCycle < kReleaseCycles`, computed on each read, so nothing has to clear it. |

State machine:

```text
            SetAttached(true)                      OnMachineReset()
Detached  ------------------->  Active  ------------------------------->  Released
    ^                             |  ^                                        |
    |       SetAttached(false)    |  |   kReleaseCycles elapsed (computed     |
    +-----------------------------+  +---- on each read) ---------------------+
    ^                                                                         |
    +------------------------------ SetAttached(false) -----------------------+
```

- **Detached**: `TryReadButton` false, `IsDrivingPaddles` false.
- **Active**: `TryReadButton` true with the switch the annunciators select;
  `IsDrivingPaddles` true.
- **Released** (the reset window): `TryReadButton` false; `IsDrivingPaddles`
  true, since the paddles have no reset-time meaning.
- `OnMachineReset` while Detached still stamps the cycle, so attaching during a
  window does not bypass it.

## Settings pref

> **Superseded (2026-09-27)** by the global setting below. Kept as the source
> the one-time adoption reads.

`$cassoUiPrefs.gamePortAdapter`, per machine, token as above. Omitted when
`"none"`. See [contracts/prefs-and-ui.md](contracts/prefs-and-ui.md).

---

## 2026-09-27 additions (GH #156)

Research R14-R23. The spec's entities map as follows.

| Spec entity | Where it lives |
|---|---|
| Joyport setting | `GlobalUserPrefs::gamePortAdapter` (token), read through `JoyportSetting` |
| Profile mode | `ControllerProfile::mode` |
| Joyport profile | the `ControllerProfileKind::Joyport` built-in (already built), now Joyport-mode |
| Chosen profile per mode | `ControllerProfileStore::activeProfiles` / `joyportActiveProfiles` (already built), now restricted to the mode's own profiles. (Superseded 2026-09-27: one map per kind, `activeProfiles` for Joystick, `paddleActiveProfiles` for Paddle and `joyportActiveProfiles` for Joyport.) |
| Paddles profile (2026-09-27) | the `ControllerProfileKind::Paddles` built-in, Paddle kind, from `DefaultMapping::MakePaddles` |
| Controller Select position | not modeled; FR-016 is undecided (R24) |

### Joyport setting (global)

| Field | Type | Notes |
|---|---|---|
| `GlobalUserPrefs::gamePortAdapter` | `std::string` | `"none"` (Apple mode) or `"siriusJoyport"` (Atari mode). Empty means never set, which triggers the one-time adoption (R14). Written on every change. |

`JoyportSetting` (`CassoEmuCore/Controllers/JoyportSetting.h/.cpp`), static
members only:

| Function | Returns |
|---|---|
| `IsInEffect (GamePortAdapter setting, bool hasAnnunciators)` | whether the running machine reads the Joyport; false on the //c whatever the setting (R16) |
| `IsMousePaddleOffered (bool isJoyportInEffect)` | whether the picker lists mouse-as-paddle: false while the Joyport is in effect (FR-009, R20) |
| `ResolveAtLaunch (const std::string & globalToken, const JsonValue * launchedUiPrefs, bool launchedHasAnnunciators)` | a `JoyportLaunchSetting { GamePortAdapter setting; bool isAdopted; }`: the global value when set, otherwise the launched machine's saved per-machine value (`None` on a machine without annunciators), with `isAdopted` true so the caller saves it |

State transitions of the setting:

```text
           picker row checked / page switch to Atari
 Apple  --------------------------------------------->  Atari
 mode   <---------------------------------------------  mode
           picker row unchecked / page switch to Apple

 A machine switch changes neither; IsInEffect decides what the machine reads.
```

### ControllerProfile (changed)

| Field | Type | Notes |
|---|---|---|
| `mode` | `ProfileMode` | `Normal` or `Joyport`, fixed at creation. Built-ins: Default `Normal`, Joyport `Joyport`, forced from the kind. User profiles: saved as `"profileMode": "joyport"`, omitted for `Normal`; absent or unknown reads `Normal`. (Superseded 2026-09-27: see the three kinds below.) |

**Current (2026-09-27, R25)**: `mode` is `ProfileMode { Joystick, Paddle,
Joyport }`, fixed at creation. Built-ins take their kind: Default `Joystick`,
Paddles `Paddle` (saved with `"paddles": true`), Joyport `Joyport`. Every
profile is saved with `"profileMode"` set to `"joystick"`, `"paddle"` or
`"joyport"`. A profile saved without one is classified: PDL0 bound and PDL1
not is `Paddle`, anything else `Joystick`; `"joyport"` stays `Joyport`. A
legacy `activeProfiles` choice of a profile that now classifies as `Paddle`
moves to `paddleActiveProfiles` on read.

Validation and rules (read "mode" as "kind" since 2026-09-27):

- Names are unique per model across both modes, ignoring case (unchanged
  `CheckProfileName`).
- `ControllerModelSettings::GetProfileNames (ProfileMode mode)`: the mode's
  built-in profile first, then the mode's user profiles in stored order.
- Reset restores the built-in mapping of the profile's own mode.
- An active-profile entry pointing at a profile of the other mode is treated as
  unset.

### ProfileSource (changed)

| Value | Starting mapping |
|---|---|
| `DefaultMapping` | the Default built-in mapping |
| `JoyportMapping` | new: the Joyport built-in mapping |
| `CopyOfProfile` | a copy of any profile of either mode (superseded 2026-09-27: a copy of a profile of the same kind) |
| `Paddles` | the Paddles template (superseded 2026-09-27: replaced by `PaddleMapping`, below) |
| `PaddleMapping` | 2026-09-27: the Paddle kind's built-in mapping, shown as "Paddles mapping" in the New profile dialog |

The new profile's mode is the mode in effect, whatever the source.
(Superseded 2026-09-27: each kind has its own starting points. Joystick: the
Default mapping or a copy of a Joystick profile. Paddle: the Paddles mapping
or a copy of a Paddle profile. Joyport: the Joyport mapping or a copy of a
Joyport profile.)

### Built-in mapping inputs (changed)

`DefaultMapping::MakeJoyport`, `ControllerModelSettings::MakeBuiltInMapping`,
`EnsureBuiltInProfiles`, `ResetProfile`, and `ControllerProfileStore`'s
`GetBuiltInSettings`, `GetOrCreateModel`, `CreateProfile` and `ResetProfile`
gain `ControllerFormFactor formFactor`. `DefaultMapping::FindSecondStick
(ControllerFormFactor, const std::vector<ControlId> &)` returns
`std::optional<std::pair<ControlId, ControlId>>`: Z/Rz, else Rx/Ry, on a
DirectInput gamepad; none on a joystick or wheel (R18).

### Jack assignment

`JoyportJackRules` (`CassoEmuCore/Controllers/JoyportJackRules.h/.cpp`):

| Type | Fields |
|---|---|
| `JoyportPlayerState` (enum) | `Idle`, `Driving`, `Held`: spec 034's `PlayerSlotState` reduced per R21 |
| `JoyportPlayers` (input) | `std::array<JoyportPlayerState, 2> players`; `bool isPlayer2Disabled` |
| `JoyportJackSource` (enum) | `None`, `Player1`, `Player2` |
| `AssignJacks` result | `std::array<JoyportJackSource, 2>`, `[0]` left, `[1]` right |

The table is research R21. Invariant: a jack's source is always a Driving
player; a Held player's jack is `None` and is never handed to the other
player.

### Labels

`JoyportLabels` (`CassoEmuCore/Controllers/JoyportLabels.h/.cpp`) returns the
strings of FR-019 from a player index, `IsInEffect`, and whether the notice is
for a controller playing alone (R22). Player 2's Disabled entry is "Same as
left" in the submenu and "same as left" in lower case after a colon (the row
"Joyport right: same as left" and the Controllers page's Player 2 drop-down).

### Player mode with the Joyport (2026-09-27)

`PlayerSlotPolicy::GetEffectiveMode (const PlayerEntry &, bool
isJoyportInEffect)` returns Joystick while the Joyport is in effect and the
player's own mode otherwise. The saved mode is not changed; every controller
plays its Joyport-kind choice while the Joyport is in effect (R26).

### DxuiToggle (changed)

| Member | Notes |
|---|---|
| `enum class OnDirection { Right, Up, Down }` | which end the thumb travels to when checked; `Right` is today's pill |
| `SetOnDirection` / `GetOnDirection` | default `Right` |
| `static ComputeTrackAndThumb (const RECT & pill, OnDirection, bool checked)` | pure geometry, tested without a painter |

### Removed

`SettingsUiPrefs::gamePortAdapter`, `ISettingsApplySink::ApplyGamePortAdapter`,
`SettingsPanelState::SetGamePortAdapter` / `ObserveLiveGamePortAdapter`, the
`HardwarePage` Game port group and its helpers (R15). The per-machine key is
no longer written (R14).
