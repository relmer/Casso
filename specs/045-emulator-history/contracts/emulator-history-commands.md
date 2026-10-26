# Contract: History Commands and Keys in the Emulator Window

**Requirements**: FR-002, FR-005 to FR-009, FR-026 | **Research**: R-006, R-008, R-013, R-014

## Keys and menu

The History menu of the emulator window's main menu, from the command table in `EmulatorCommands.cpp`. The keys are proposals for the owner's approval (R-014).

| Item | Key | Command | Enabled |
|---|---|---|---|
| Rewind | F2, while it is down | the rewind loop (below) | history on, and the machine past the oldest point |
| Step back | Ctrl+F11 | `ReverseCommand::StepBack` | history on, and the machine past the oldest point |
| Go live | Ctrl+F2 | `ReverseCommand::GoLive` | behind live |
| History on | none; a check item | the setting, with FR-026's confirmation when cleared | always |

Labels are sentence case. F2's press and release are routed by `ShellKeyRouting` to `ShellKeyOwner::Rewind`; the guest receives neither, and its character is swallowed.

## The rewind loop (CPU thread)

| Moment | Action |
|---|---|
| F2 press (not a repeat) | Rewinding starts: the machine is paused, sound is muted |
| Each frame while rewinding | `ReverseController::StepBackCycles (kFrameCycles)`, then the frame is rendered and presented; the caption and indicator updated |
| At the oldest point | Rewinding stops there; the machine is paused |
| F2 release | Rewinding stops at the frame on screen; the machine is paused (SC-003) |
| A modal dialog open | The window receives no keys, so F2 does nothing |

## Behind live

| Action | Result |
|---|---|
| Run (the Pause item, or the debugger's Run) | Replays the recorded future from the position, inputs included; the replay indicator is drawn |
| A key, paste, game port or mouse input | Deferred until the replayed guest reads it; history forks at that read, and the input goes in live |
| A disk mount, eject or write-protect change, a reset, a power cycle, a changed-file answer, a debugger restart, a state file loaded without history, a debugger edit | History forks at the position, with no prompt, and the change runs live |
| A machine switch, a configuration change, exit | No fork; the history is saved at the position on screen (R-010) |
| A replay at the live end | The machine is live; the indicator goes |

## The replay indicator

| State | Drawn |
|---|---|
| Behind live and running | "Replaying" and the host time of the position, over the top of the viewport, pulsing |
| Behind live and paused, or rewinding | Not drawn; the rewind bar and caption display the position |
| Live | Not drawn |
| A capture paint | Hidden, as the other application overlays are |

## Turning history off

| Step | Result |
|---|---|
| The user clears History on, in the menu, Settings > General or the debugger's options | A confirmation dialog: its text specifies that all of the machine's history, in memory and saved, will be deleted |
| Confirm | Recording stops, the store is cleared, the rewind bar and key are disabled, and the next save writes no history section |
| Cancel | The setting is on |

## Tests

- `ShellKeyRouting`: F2 is `ShellKeyOwner::Rewind`, its character is swallowed, and the guest receives it in no state.
- The rewind loop over a test machine: frames step back by `kFrameCycles`, stop at the oldest point, and stop on release; sound muted throughout.
- Each "Behind live" row: replay or fork, with no prompt and no question text left in the code.
- The off confirmation: confirm clears and disables; cancel changes nothing.
