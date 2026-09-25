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
joystick state" entity, one per jack.

| Field | Type | Notes |
|---|---|---|
| `bits` | `std::bitset<5>` | Indexed by `JoystickSwitch`: `Up`, `Down`, `Left`, `Right`, `Fire`. Set = closed. |

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
| `MachineRefs` | `Shell/MachineRefs.h` | + `SiriusJoyport * joyport`. |
| `SettingsUiPrefs` | `Ui/Settings/SettingsPanelState.h` | + `GamePortAdapter gamePortAdapter = None`. |
| `ISettingsApplySink` | `SettingsPanelState.h` | + `ApplyGamePortAdapter (GamePortAdapter)`. |

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
| `m_jacks` | `std::array<std::atomic<Byte>, 2>` | Each jack's `JoystickSwitches` bits, written by the sink. |
| `m_resetCycle` | `uint64_t` | Stamp from the last `OnMachineReset`. |
| `m_isInResetWindow` | `bool` | Set by `OnMachineReset`, cleared once `kReleaseCycles` have run. |

State machine:

```text
            SetAttached(true)                      OnMachineReset()
Detached  ------------------->  Active  ------------------------------->  Released
    ^                             |  ^                                        |
    |       SetAttached(false)    |  |   kReleaseCycles elapsed (checked      |
    +-----------------------------+  +---- lazily on the next read) ----------+
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

`$cassoUiPrefs.gamePortAdapter`, per machine, token as above. Omitted when
`"none"`. See [contracts/prefs-and-ui.md](contracts/prefs-and-ui.md).
