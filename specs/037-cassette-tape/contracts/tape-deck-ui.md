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
| Name row (click) | Opens the file picker on the inserted tape's folder (a recent-tapes list is a follow-on) | always |
| Rewind | Stops if moving, then sets position to 0 | a tape is inserted |
| Play | Starts playback, or recording if record is armed | stopped with a tape |
| Stop | Stops; commits a recording | playing or recording |
| Record | Toggles record armed | the tape is writable and stopped |
| Eject | Commits any recording, then empties the deck | a tape is inserted |

Menu: the Disk menu gains Insert tape..., New blank tape..., Play tape, Stop
tape, Rewind tape and Eject tape, after the drive items. All are disabled on the
//c, and the transport items follow the same enabled rules as the buttons. All
labels are sentence case.

## Display

- File name, centered and clipped to the row; `(empty)` with no tape. A marquee for long names is a follow-on.
- A progress rail showing position over length, and an `m:ss / m:ss` readout.
- State: play lit while playing; record lit red while recording and dimmed
  while armed. A protected tape shows the record button disabled, like a tape
  with its tab broken out.
- The transport marks (rewind, play, stop, record, eject) are drawn shapes:
  Segoe MDL2 has no eject glyph, and drawn shapes stay one set.

## Settings

Disk page: a "Fast tape loading" toggle, on by default, beside the drive audio
toggle it copies. Off means loads run at the selected speed and the tape is
audible.
