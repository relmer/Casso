# Disk II and Input debug windows: feature inventory (T775, FR-217)

Read from commit `c35609ac3^`, the last commit with `Disk2DebugPanel` and
`InputDebugPanel`, from their tests, and from `specs/006-disk-ii-debug/`. No
spec defines the Input window; its features come from its code and tests.
"Specified, not built" marks spec 006 requirements the Dxui panel never met.

Each row has a proposal for the new debugger panes (FR-213 to FR-216):
**As is** (same behavior), **Change** (the behavior in the last
column), or **Drop**. The owner decides each row.

## Both windows

| Feature | Old window | Proposal |
|---|---|---|
| Standalone window | Top-level 960x600 DIP window from the Debug menu, `Disk ][ Debug...` (Ctrl+Shift+D) and `Input debug...` (Ctrl+Shift+I); close hides it and capture goes on | **Change**: dockable debugger panes, opened from the debugger's pane list. Ctrl+Shift+D and Ctrl+Shift+I open the debugger with that pane in front |
| Capture start | From the first time the window opens | **Change**: from session start, whether or not a pane is open (FR-211) |
| Depth | 100,000 rows, oldest dropped | **Change**: as deep as history, older events rebuilt by replay (FR-212) |
| Time column | Host local time at the moment the row is drawn, not when the event happened | **Change**: optional wall time column, stamped when the event happens (FR-215) |
| Uptime column | Time since the last reset or power cycle | **Drop**: the cycle column replaces it |
| Cycle column | Decimal with thousands separators; blank for host events in the Input window | **Change**: on every row, host events included (FR-215) |
| Reset clears the log | A soft reset or power cycle clears every row | **Change**: a reset or power cycle is a row in the log; nothing is cleared |
| Pause / Resume | Stops draining; on resume one "Events lost" row with the dropped count | **Drop**: every event is in history, and scrolling up stops the tail |
| Clear | Empties the list and the ring | **Drop**: the log is a view of history. Rewinding cuts it back (FR-211) |
| Events lost row | `Events lost`, or `??? N events lost (ring overflow)`, never filtered out | **Change**: a gap row where recording paused, and a row where history begins |
| Sticky tail | The newest row stays in view while the list is at the bottom | **As is** |
| Sort by column | Header click sorts, a second click reverses. The Input window sorts every column as text, so cycles sort lexically | **Change**: Cycle sorts numerically in both panes |
| Column show/hide | Header right-click menu with one checkable item per column | **As is** |
| Column resize | Drag a header edge; widths last until the window closes | **Change**: widths saved with the debugger layout |
| Selection | Disk II: the selected event remains selected across re-sorts. Input: the selected row index remains selected | **Change**: the selected event remains selected in both panes |
| Keyboard | Disk II: Tab through header, dividers and rows. Input: Up/Down, PgUp/PgDn, Home/End | **As is**, with the Disk II behavior in both panes |
| Copy | Disk II: none (spec 006 FR-019 specified, not built). Input: a Copy button copies every visible row, tab-separated, no header row | **Change**: Ctrl+C copies the selected rows and a Copy button copies every visible row, in both panes |
| Tooltips | On every filter control | **As is**, with the Input window's typos fixed ("DxuiToggle every emulator-input lane at once") |
| Settings saved | None | **Change**: filters, column set and widths saved with the debugger layout |
| Row colors | None | **As is**, none |

## Disk II event log

| Feature | Old window | Proposal |
|---|---|---|
| Motor events | `Motor command on` and `Motor command off` on every $C0E9/$C0E8 strobe; `Motor engaged` on the off-to-on edge; `Motor disengaged` when the spindown timer expires | **As is** |
| Head step | `quarter-track <prev> -> <new>` | **As is** |
| Head bump | `at quarter-track <n>`, when the head clamps at a stop | **As is** |
| Address mark | `T<t> S<s> V<v>` after a D5 AA 96 prologue with a valid checksum | **As is** |
| Data read | `T<t> S<s> V<v> (256 bytes)`, from the last address mark | **As is** |
| Data write | A checkbox, but no producer: the event never occurs | **Change**: built, so sector writes appear (FR-213) |
| Drive select | On $C0EA/$C0EB | **As is** |
| Disk inserted / ejected | From the mount and eject paths | **As is** |
| Audio outcomes | `Audio started`, `restarted`, `continued`, `loop started`, `loop stopped`, `silent` with kind and reason | **Drop**: drive sound depends on host settings, so a replay of history cannot rebuild these rows (FR-210) |
| Q6/Q7, latch, raw nibbles | Not captured | **As is**, not captured; the diagnostics pane has the read or write mode (FR-216) |
| Columns | Time, Uptime, Cycle, Drive, Event, Detail | **Change**: Cycle, Drive, Event, Detail, and optional wall time |
| Event filter | `Disk events:` Motor, HeadStep, HeadBump, AddrMark, Read, Write, Door, DriveSel, all on | **As is** |
| Audio filter | `Audio events:` All, Started, Restarted, Continued, Silent | **Drop** with the audio rows |
| Drive filter | All, Drive 1, Drive 2; a row with no drive is hidden under Drive 1 or 2 | **As is** |
| Track and sector filter | Text boxes; grammar `token (',' token)*`, a token a number or `lo-hi` range; tracks 0-39, sectors 0-15; `17.25`-style quarter-tracks; an empty box matches everything | **As is** |
| Quarter-track filter | `Quarter-track steps` checkbox reads bare numbers as quarter-tracks; quarter-track tokens never match a row, and head steps ignore the track filter | **Change**: quarter-track tokens match head step and head bump rows by quarter-track |
| Invalid filter text | Red `Invalid track: tok1, tok2` label under each box; spec 006's wavy underline specified, not built | **As is**, the label |
| Filter debounce | Every keystroke refilters (spec 006's 250 ms debounce specified, not built) | **As is** |
| Second controller | Slot 6 only; a stored "more than one controller" hint never appears | **As is**, slot 6 only |
| Menu item enabled | Always enabled; silent when the machine has no Disk II (spec 006's gray item specified, not built) | **Change**: the pane has a line that the machine has no Disk II |

## Disk II event log: from spec 012

Spec 012 (Disk II debug telemetry, written for the old window) was closed on
2026-10-13 as superseded by 035 and 040. These two of its items had no other
home and were added here after the owner approved the rows above; the
owner approved both proposals on 2026-10-13.

| Feature | Spec 012 | Proposal |
|---|---|---|
| Read stall | US4, FR-008, FR-009: a Warning row when the program reads the data latch at one head position for K revolutions or more with no address mark decoded; an address mark or a head step resets it, so it can come again. K of 2 or 3, a constant tuned against self-sync gaps and normal spacing between sectors | **Change**: added, K = 2 revolutions, from the Disk II event feed, so a replay rebuilds it (FR-210). It could also be a disk breakpoint kind (User Story 20) |
| Silent ratchet marker | FR-013: during a boot recalibrate, rapid bumps go through `Disk2AudioSource::OnHeadBump` in a thunk, silent, click, click cycle, with no audio event for the silent slot, so in a log of audio events that bump looks like a dropped event. The audio source would report the silent slot and the log would mark the bump | **Drop**: the mark applies to audio rows, which are dropped above |
## Input log

| Feature | Old window | Proposal |
|---|---|---|
| Guest $C000 read | Only when the byte changes: `Read $C000 -> $xx  key='c' strobe=0/1` | **As is** |
| Guest $C010 access | When it clears a set strobe or its value changes: `Clear strobe $C010 -> $xx  AKD=b cleared=b` | **As is** |
| Button reads $C061-$C063 | When the byte changes: `DxuiButton read $C06x -> $xx pressed=b` with Open Apple, Closed Apple, Shift or mouse button notes | **Change**: "Button read" (the old text has a rename typo); the $C063 note follows the current machine, not the one the window opened on |
| Paddle reads $C064-$C067 | When bit 7 changes: `Read PADDLn $C06x -> $xx counting=b` | **As is** |
| PTRIG $C070 | On every access, so a polling loop fills the log | **Change**: one row per strobe that a changed paddle read follows |
| Host key down / up | `Key down: $xx 'c'`; a repeated key-down once | **As is**, from the input journal (FR-214) |
| Host auto-repeat | Each time the //e repeat timer re-latches the key | **As is** |
| Host paddle / joystick axes | `HOST JOY<p> X = n` or `HOST PDL<n> = v`, labeled when the row is made | **Change**: the label follows the current pair view |
| Host buttons | `HOST BTN<i> DOWN/UP` | **As is** |
| Host mouse | Not captured | **Change**: added (FR-214 lists the mouse) |
| Columns | Wall, Uptime, Cycle, Source (Host/Guest/System), Address, Value, Meaning | **Change**: Cycle, Source, Address, Value, Meaning, and optional wall time |
| Emulator input filter | `Emulator input:` All, Keyboard, Joystick, Paddle; Joystick and Paddle appear for the pairs viewed that way | **As is** |
| Host input filter | `Host input:` Keyboard | **As is** |
| Pair views | `View PADDL0-PADDL1 as` Joystick 0 or Paddles 0, 1; the same for PADDL2-PADDL3; both Joystick by default | **As is** (FR-214) |
| Host / guest order | Host rows drained before guest rows each frame, so order follows the drain, not time | **Change**: rows in cycle order |
