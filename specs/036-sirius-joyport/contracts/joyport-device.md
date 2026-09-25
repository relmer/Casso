# Contract: Joyport device and the button reads

Covers FR-003, FR-004, FR-009, FR-010, FR-011, FR-013. Types are in
[data-model.md](../data-model.md).

## SiriusJoyport

```cpp
class SiriusJoyport
{
public:
    static constexpr uint64_t kReleaseCycles = 500'000;

    void  SetAnnunciatorSource (const AppleSoftSwitchBank * bank);
    void  SetCycleSource       (const uint64_t * totalCycles);

    void  SetAttached          (bool attached);                         // UI thread
    bool  IsAttached           () const;
    void  SetJackSwitches      (size_t jack, JoystickSwitches switches); // UI thread, via the sink

    void  OnMachineReset();                                             // CPU thread, after the CPU resets

    bool  TryReadButton        (int index, Byte & value);               // CPU thread
    bool  IsDrivingPaddles     () const;                                // CPU thread
};
```

### TryReadButton (index 0-2)

1. Detached: return false.
2. In the reset window (fewer than `kReleaseCycles` since the stamp, computed
   on each read): return false.
3. Otherwise: `jack = AN0 ? 1 : 0`. The switch is:

   | index | AN1 off | AN1 on |
   |---|---|---|
   | 0 | Fire | Fire |
   | 1 | Left | Up |
   | 2 | Right | Down |

   `value = closed ? 0x00 : 0x80`; return true.

A false return means "answer as if no Joyport were attached." The caller does
exactly what it did before this feature.

### IsDrivingPaddles

True while attached, reset window or not. A caller reads bit 7 set for
`$C064`-`$C067` (the timer never expires: no pot on an Atari stick).

## Reading devices

| Device | Address | Change |
|---|---|---|
| `AppleGamePort` (][, ][+) | `$C061`-`$C063` | `ReadButton` asks `m_joyport->TryReadButton` first. |
| `AppleGamePort` | `$C064`-`$C067` | `ReadPaddle` returns `0x80` while `IsDrivingPaddles`. |
| `Apple2eKeyboard` (//e) | `$C061`-`$C063` | Asks `TryReadButton` first; on false, the existing Open Apple / hold, Closed Apple / hold, and Shift logic. |
| `Apple2eSoftSwitchBank` (//e) | `$C064`-`$C067` | `ReadPaddle` returns `0x80` while `IsDrivingPaddles`. |

- Both keep emitting their button-read event with the value actually returned.
- `m_joyport` is null on the //c and whenever no Joyport was built; a null
  pointer is exactly today's behavior (FR-013).
- `$C070` / `$C07x` paddle triggers are unchanged.

## Annunciators

```cpp
class AppleSoftSwitchBank
{
public:
    bool  IsAnnunciatorOn (int index) const;       // index 0-2
    void  PowerCycle      (Prng & prng) override;  // clears AN0-AN2, then SoftReset()
};
```

- `$C058`-`$C05D`, read or write, set or clear AN0-AN2 on the ][, ][+, //e and
  enhanced //e.
- //c: with IOU access on, `$C058`-`$C05F` still go only to
  `AppleMouse::AccessIouSwitch`, and the annunciators do not change.
- //e: `$C05E`/`$C05F` still set and clear DHIRES, and still raise
  `bankingChange`.

## Reset ordering

`MachineHost::SoftReset` and `MachineHost::PowerCycle` call
`m_joyport->OnMachineReset()` as their last step, after the CPU's own reset,
so a power cycle stamps the zeroed counter. A machine switch builds a new
Joyport, applies the machine's saved adapter, and then runs `PowerCycle`.

## Tests (UnitTest/EmuTests/)

- `AnnunciatorTests.cpp`: each of `$C058`-`$C05D` by read and by write on the
  ][+ and //e; DHIRES still toggles; //c IOU on routes to the mouse and leaves
  AN0-AN2 alone; `PowerCycle` clears; `SoftReset` keeps.
- `SiriusJoyportTests.cpp`: all 12 (index, AN0, AN1) combinations against a
  jack pattern; the window opens on reset and closes at exactly
  `kReleaseCycles`; attach while running needs no window; reset while detached
  still stamps; `IsDrivingPaddles` through the window.
- `JoyportMachineTests.cpp` on `TestMachine` for the ][+ and //e: a guest
  `LDA $C059`/`LDA $C05B`/`LDA $C062` program reads the right jack's up
  switch; with the Joyport attached and nothing held, `SoftReset` and
  `PowerCycle` take the normal reset path (no self-test, no reboot) 20 of 20
  times; an Open Apple hold through reset still reboots (SC-003); paddles read
  255; detached, the existing `KeyboardTests` and `GamePortTests` expectations
  hold (SC-006).
