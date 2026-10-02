# Contract: Tape deck UI

## Placement

- A tape deck widget in the drive band, to the left of the drives, using the
  drive widgets' caption column ("TAPE"), name row and rail.
- It is hidden on models without a cassette port (the //c), as drives are
  hidden when absent.
- In the 3D desk scene the flat widget is hidden, and a Panasonic RQ-309DS,
  the recorder Apple recommended by name, sits on the desk instead. Its piano keys (record,
  play, rewind, stop/eject) map to the controls below, the cassette door
  inserts, and a counter shows position. It is absent on the //c.

## Controls

| Control | Action | Enabled when |
|---|---|---|
| Name row (click) | Opens the tape picker (recent tapes plus Browse) | always |
| Name row (drop a file) | Inserts that file | always |
| Rewind | Stops if moving, then sets position to 0 | a tape is inserted |
| Play | Starts playback, or recording if record is armed | stopped with a tape |
| Stop | Stops; commits a recording | playing or recording |
| Record | Toggles record armed | the tape is writable and stopped |
| Eject | Commits any recording, then empties the deck | a tape is inserted |

Menu (Tape submenu beside the disk items): Insert tape..., New blank tape...,
Eject tape, Rewind. All labels are sentence case.

## Display

- File name, marqueed when too long; `(empty)` with no tape.
- A progress rail showing position over length, and an `m:ss / m:ss` readout.
- State: play lit while playing; record lit red while recording and dimmed
  while armed; a protected-tape badge when record is unavailable.

## Settings

Hardware page: a "Fast tape loading" checkbox, on by default. Off means
loads run at the selected speed and the tape is audible.
