# Casso debugger tutorials

Short, guided walk-throughs of Casso's debugger, built on the demo disks and
programs in `Apple2/Demos`. Each one has a summary, the steps, and what you
should see, and links into [Debugger-Reference.md](Debugger-Reference.md)
for the details. [Debugger-Commands.md](Debugger-Commands.md) lists every
command's syntax.

The paths below are relative to the folder Casso was cloned into. The keys are
the default Visual Studio key scheme's; **Tools > Keyboard scheme** switches to
AppleWin's or GSSquared's, and every menu row lists its key in the scheme in
force ([key schemes](Debugger-Reference.md#key-schemes)). Console commands are
in the default AppleWin mode.

## Contents

1. [A first stop on the symbols demo](#1-a-first-stop-on-the-symbols-demo)
2. [Source-level debugging an as65 program](#2-source-level-debugging-an-as65-program)
3. [Finding who wrote a byte](#3-finding-who-wrote-a-byte)
4. [Debugging Applesoft](#4-debugging-applesoft)
5. [Following a disk loader](#5-following-a-disk-loader)
6. [Sound and speech with the Mockingboard panel](#6-sound-and-speech-with-the-mockingboard-panel)
7. [Profiling a game](#7-profiling-a-game)
8. [Scripting](#8-scripting)

## The symbols demo

Tutorials 1 to 3 use `Apple2/Demos/symbols-demo.dsk`, a small program that
clears the text screen, draws a title, then counts from 0 to 39 over and over,
drawing the count in hex and one `*` per count in a bar across the screen. Its
source is `Apple2/Demos/SymbolsDemo/symbols-demo.a65`, loaded at $6000. The
debug file `symbols-demo.dbg`, written by `CassoCli as65 -g`, is beside the
source, and `Apple2/Demos/SymbolsDemo/Build.ps1` rebuilds the disk and the
debug file together. The subroutines `clear_screen`, `print_text`,
`draw_counter`, `hex_digit`, `draw_bar` and `wait` are called with `JSR`, the
loop counter is the byte `count` at $60C1, and every label has a comment on its
defining line.

## 1. A first stop on the symbols demo

**In this tutorial:** stopping a running program, loading its symbols, stepping,
watching a variable, and setting a breakpoint.

**Steps**

1. Boot the disk with `Casso.exe --disk1 Apple2\Demos\symbols-demo.dsk`. The
   title appears and the bar starts to fill.
2. Open the debugger with **Debug > Debugger...** (F12), then choose
   **Debug > Break** (Shift+F5). The machine stops; most of the time the PC is
   in the delay loop of `wait`, around $60B6.
3. Choose **File > Open symbol or debug file...** and pick
   `Apple2\Demos\SymbolsDemo\symbols-demo.dbg`. Typing
   `SYM LOAD "Apple2\Demos\SymbolsDemo\symbols-demo.dbg"` in the console, or
   dropping the file on the debugger, does the same. The disassembly now has
   the program's labels, and a source pane opens beside it.
4. Press F11 (**Debug > Step into**) a few times. Each press runs one
   instruction; the registers that changed are in the changed color. Press
   Shift+F11 (**Debug > Step out**) to run to the end of `wait`, and F10
   (**Debug > Step over**) on a `JSR` to run the whole call as one step.
5. Watch the counter: in the Watches pane, type `count.b` in the add row and
   press Enter. `count` alone would watch the word at $60C1; `.b` reads the
   byte.
6. Set a breakpoint on `draw_bar`: type `BP draw_bar` in the console, or
   select the `draw_bar` row in the disassembly and press F9. A red dot marks
   the row.
7. Press F5 (**Debug > Run**) several times.
8. Hover the operands for the symbol tips:
   - The operand of `JSR clear_screen` at $6001. The tip has the symbol's
     description and its comment, "fill the text page with spaces". The `JSR`
     operands `print_text`, `draw_counter`, `draw_bar` and `wait` have theirs.
   - The operand of `LDA count` or `INC count` in the loop at $6042, with the
     data label's comment, "loop counter, 0 to BAR_LENGTH-1".
   - `BAR_LENGTH` is an equate, so `SYM BAR_LENGTH` marks it `constant`, and
     `CMP #$28` is a number in the disassembly.
9. Type `BPC *` to clear the breakpoints, then press F5 to let the demo run.

**What you should see:** each F5 in step 7 stops at `draw_bar` ($60AD), one
`*` further along the bar, and the watch on `count.b` is one higher, in the
changed color. The stop line in the console has the breakpoint and the
address.

**Reference:** [running and stopping](Debugger-Reference.md#running-and-stopping),
[execution breakpoints](Debugger-Reference.md#execution-breakpoints),
[watches](Debugger-Reference.md#watches),
[symbol tables](Debugger-Reference.md#symbol-tables) and
[symbol tips](Debugger-Reference.md#symbol-tips).

## 2. Source-level debugging an as65 program

**In this tutorial:** the source pane, source rows in the disassembly,
`file:line` breakpoints, stepping by source line, and how data directives are
disassembled.

**Steps**

1. Boot `Apple2\Demos\symbols-demo.dsk` as in tutorial 1 and open the
   debugger. To build the program yourself, `CassoCli as65 symbols-demo.a65 -g
   -o symbols-demo.bin` in `Apple2\Demos\SymbolsDemo` writes the binary and
   `symbols-demo.dbg`; `Build.ps1` there also puts the binary on the disk
   ([Assembler.md](Assembler.md#debug-files)).
2. Choose **File > Open source file...** and pick
   `Apple2\Demos\SymbolsDemo\symbols-demo.a65`. With no debug file loaded,
   Casso loads `symbols-demo.dbg` beside it first. The source is found from
   the debug file because the debug file has the path of `symbols-demo.a65`
   relative to itself; when the pane reports that the source is not found,
   drag `symbols-demo.a65` onto the debugger.
3. Choose **View > Symbols**, right-click `start` and choose **Go to**. With
   **Show source code** checked on the disassembly view's toolbar, each source
   line is on a row above the code it produced. Uncheck it to see the
   instructions alone, and check it again.
4. Type `BP symbols-demo.a65:71` in the console and press F5. Line 71 is the
   top of the loop, `main_loop`, at $6042.
5. Click in the source pane and press F11 a few times. With the source pane
   focused, a step runs one source line; in the disassembly it runs one
   instruction. `SRC ON`, or **Debug > Step by source line**, makes every step
   command step by source line.
6. Step over `jsr draw_counter` with F10, or into it with F11 and out again
   with Shift+F11, and watch the rows of `inc count` and of `sta ROW_BAR,x` in
   `draw_bar`.
7. In **View > Symbols**, go to `count`. The bytes at $60C1 are disassembled as
   data, from the `.byte` directives in the source: `count` as a byte,
   `hex_chars` and the strings as text, each under its label, with the
   directive in italics.
8. Right-click the `hex_chars` row and choose **Clear data marking**. The
   bytes are disassembled as instructions until the debug file loads again;
   load it again with **File > Open symbol or debug file...**.
9. Hover `#>count_text` in a source line such as `lda #>count_text`. The tip
   has `count_text = $60F2` and `#>count_text = #$60, its high byte`.
10. Type `BPC *` and press F5.

**What you should see:** the PC's row is marked in the source pane, and a
stepped instruction's row has its operand, in the comment color, and
`Result: `, in the result color, after the code. In the disassembly, the
breakpoint, the PC's arrow and the branch arrow of the first instruction under
a source line are on the source line.

**Reference:** [debug files](Debugger-Reference.md#debug-files),
[source documents](Debugger-Reference.md#source-documents),
[source rows](Debugger-Reference.md#source-rows),
[data marking](Debugger-Reference.md#data-marking) and
[the Symbols pane](Debugger-Reference.md#the-symbols-pane).

## 3. Finding who wrote a byte

**In this tutorial:** a data breakpoint, the heat map's last writer, stepping
back and reverse continue. History recording has to be on, which it is by
default (**Tools > Options...**, History page).

**Steps**

1. Boot `Apple2\Demos\symbols-demo.dsk`, open the debugger, and load
   `symbols-demo.dbg` as in tutorial 1. Add the watch `count.b`.
2. Open the heat map with **View > Heat map** (Ctrl+Alt+H) and let the demo
   run for a few seconds. The heat map counts accesses only while its pane is
   open and in front.
3. Rest the pointer on the cell for $60C1; Ctrl with the wheel zooms the map,
   and the tip begins with the address under the pointer. The tip has how often the byte is
   read and written, and after a moment the instruction that last wrote it and
   the one that last read it.
4. Ctrl+click the cell. The disassembly moves to the last writer, usually the
   `INC count` in `main_loop`.
5. Choose **Debug > Break**, then type `BPMW count` in the console and press
   F5. The machine stops just after the next write to `count`; the instruction
   above the PC is the one that wrote it.
6. Press Alt+F11 (**Debug > Step back into**). The machine steps back one
   instruction, to the `INC`, and the watch on `count.b` has the value from
   before the write.
7. Press Alt+F5 (**Debug > Reverse continue**). The machine runs backward
   through history to the previous write to `count`, one count earlier.
8. Choose **Debug > Go live** to return to where the machine was running.
9. Ctrl+Alt+click the $60C1 cell in the heat map. The machine goes back
   through history to the cell's last write, just after the instruction that
   made it.
10. Type `BPC *`, choose **Debug > Go live** and press F5.

**What you should see:** while a reverse command replays history, the status
bar has **Replaying history**. After each step back or reverse continue, the
heat map, the registers and the watch are what they were at that point in
history.

**Reference:** [data breakpoints](Debugger-Reference.md#data-breakpoints),
[stepping back](Debugger-Reference.md#stepping-back),
[the heat map](Debugger-Reference.md#the-heat-map) and
[last writer and last reader](Debugger-Reference.md#last-writer-and-last-reader).

## 4. Debugging Applesoft

**In this tutorial:** the BASIC program pane, a variable breakpoint, `BPBASIC`,
the BASIC steps, and Applesoft variables in the Watches pane. It uses
`Apple2/Demos/Joyport.do`, a DOS 3.3 disk whose `HELLO` program runs the
Applesoft program `JOYPORT`, the Joyport test. `Apple2/Demos/Joyport.bas.txt`
is its listing.

**Steps**

1. Boot the disk with `Casso.exe --machine Apple2e --disk1 Apple2\Demos\Joyport.do`.
   DOS 3.3 boots and the Joyport test's menu appears, ending in
   `CHOOSE A TEST:`. Line 220 of the program, `GET A$`, reads the key.
2. Open the debugger (F12), choose **Debug > Break**, and open the program
   with **View > Source files > BASIC program** (Ctrl+Alt+P, P).
3. Type `BPBASICVAR A$` in the console and press F5. Click in the emulator
   window and press B, the menu's beep toggle.
4. In the Watches pane, type `A$` in the add row, and `$300.b`, the byte at
   $0300 (768) where the program saves the beep setting.
5. Click in the BASIC program pane and press F11 a few times. With the
   keyboard focus in this pane, Step into runs `BASICSTEP`, a statement at a
   time, Step over runs `BASICLINE`, a line at a time, and Step out runs
   `BASICOUT`.
6. Step past line 285, `POKE 768,1 - PEEK(768): GOTO 100`. The watch on
   `$300.b` changes from 0 to 1, in the changed color, and the program goes to
   line 100.
7. Line 100 calls `GOSUB 950`. Step into it with F11; the call stack lists the
   active `GOSUB` return. Press Shift+F11 to run to the statement after the
   `GOSUB` returns, `SK = 0`.
8. Type `BASICVARS` to list every variable, and `BASICSTACK` for the active
   `GOSUB` returns and `FOR` loops.
9. Type `BPC *`, then `BPBASIC 285`, and press F5. In the emulator, press B
   again. The machine stops as line 285 begins. Double-click a row in the BASIC
   program pane to set or clear a line breakpoint there.
10. Type `BPC *` and press F5.

To debug another program, **File > Load BASIC program...** or `BASICLOAD file`
replaces the program in memory with a listing, such as
`Apple2\Demos\EightyFortyTest.bas.txt`; type `RUN` in the emulator to start it.

**What you should see:** the stop in step 3 reports that `A$` changed in line
220, and the machine stops at the start of the next statement, line 230. The
line being run has the PC's row color and arrow in the BASIC program pane, with
its statement in full color. The Automatic section of the Watches pane lists
the variables the statement being run uses.

**Reference:** [Applesoft](Debugger-Reference.md#applesoft),
[BASIC breakpoints and steps](Debugger-Reference.md#basic-breakpoints-and-steps)
and [Applesoft in the window](Debugger-Reference.md#applesoft-in-the-window).

## 5. Following a disk loader

**In this tutorial:** the Disk II log and panel, an RWTS breakpoint, and a
DOS 3.3 sector breakpoint, on the DOS 3.3 disk `Apple2/Demos/Joyport.do`.

**Steps**

1. Start Casso with the debugger open:
   `Casso.exe --machine Apple2e --debugger --disk1 Apple2\Demos\Joyport.do`.
2. Open **View > Logs > Disk II log** (Ctrl+Alt+L, D) and
   **View > Device panels > Disk II** (or `PANEL disk`).
3. Choose **Machine > Power cycle** to boot the disk again with the log open.
   The log lists the motor starting, the head stepping, the address fields and
   the sectors read. On the Disk II panel, the head moves along the ruler and
   the stepper's phase lamps light as it steps.
4. When the Joyport menu appears, click in the emulator and press Q. The
   program ends at the `]` prompt.
5. Type `BPDISK RWTS` in the console. In the emulator, type `CATALOG` and press
   Return.
6. The machine stops before RWTS's first instruction. The stop report has the
   IOB's request: the slot, drive, volume, track, sector, command and buffer.
   The catalog is on track $11. The call stack lists the calls that led to
   RWTS.
7. Type `CALC DISK.QTRACK` for the head's quarter track, then press F5 to run
   to the next RWTS call.
8. Type `BPC *`, then `BPDISK DOS 11 F`, and press F5. In the emulator, type
   `CATALOG` again. The machine stops when DOS 3.3's logical sector $F of track
   $11, the first catalog sector, is read.
9. Type `BPC *` and press F5.

**What you should see:** each stop's report begins with the breakpoint and the
event. While DOS reads the catalog, the Disk II panel has the head on track 17
($11) with the motor lit, and the Disk II log's Track, Sector and Volume
columns have each event's place.

**Reference:** [disk breakpoints](Debugger-Reference.md#disk-breakpoints),
[Disk II](Debugger-Reference.md#disk-ii) and
[the call stack](Debugger-Reference.md#the-call-stack).

## 6. Sound and speech with the Mockingboard panel

**In this tutorial:** the Mockingboard panel, a channel mute, the sound log,
and the SSI-263 speech chip's card. The Apple //e has a Mockingboard C in slot
4. It uses `Apple2/Demos/mockingboard-music.dsk`, a tune for the card's two
AY-3-8910 chips, and `Apple2/Demos/mockingboard-speech-demo-hgr.dsk`, a demo
of speech and song through the SSI-263.

**Steps**

1. Boot the music disk with
   `Casso.exe --machine Apple2e --disk1 Apple2\Demos\mockingboard-music.dsk`.
   The tune starts.
2. Open the debugger (F12) and choose **View > Device panels > Mockingboard**,
   or type `PANEL mockingboard`. Each AY-3-8910 has a card with a row per
   channel: the note and frequency, tone and noise indicators, a level meter
   and a scope. In this tune, the first chip has the melody, a chord and the
   bass, and the second the chord pad and the drums.
3. Click the mute on the first chip's channel A. The melody drops out of the
   host's audio, and that channel's scope is unaffected. Click it again.
4. Open **View > Logs > Sound log**. Use the Source column's Filter to list the
   AY-3-8910s alone.
5. Choose **Debug > Break** and type `SOUNDLOG LIST 20` for the last entries in
   the console.
6. Type `DISK INSERT D1 Apple2\Demos\mockingboard-speech-demo-hgr.dsk`, then
   choose **Machine > Power cycle** and press F5.
7. The SSI-263's card is across the top of the Mockingboard panel: the phoneme
   by name, its waveform, its three formants, and a strip of the recent
   phonemes under the Recent phonemes heading. To go back through the strip,
   tilt the wheel over it, or turn the wheel while pressing Shift.
8. In the sound log, filter the Source column to the SSI-263. Each write to the
   speech chip is decoded by register: the phoneme and its duration, the rate,
   the inflection, the articulation and amplitude, or the filter.

**What you should see:** the panel's level meters and scopes follow the music,
and the arrow under the strip's labels marks the phoneme playing. A tip on each
part of a card has the raw register values.

**Reference:** [the Mockingboard](Debugger-Reference.md#the-mockingboard),
[video and sound logs](Debugger-Reference.md#video-and-sound-logs) and
[device panels](Debugger-Reference.md#device-panels).

## 7. Profiling a game

**In this tutorial:** a profile built from history, the routines and the call
tree, the flame graph, the profiler's timeline, and the stopwatch. It uses
`Apple2/Demos/LodeRunner.woz`; any game disk in `Apple2/Demos` works the same
way.

**Steps**

1. Boot the game with `Casso.exe --machine Apple2e --disk1 Apple2\Demos\LodeRunner.woz`
   and run a game, or let its attract mode run, for a minute.
2. Open the debugger (F12), choose **Debug > Break**, and open **View >
   Profiler**.
3. Choose **Last 60 s** from the bar's **Show** drop-down. "Loading
   profile... (n%)" is over the list while the profile builds from history.
4. The **Routines** view lists each routine's Self, Total, Calls and Per call.
   With no symbols loaded, a routine is its address. Right-click a hot routine
   and choose **Add symbol here...** to label its address.
5. Choose **Flame graph** from the view drop-down. Click a routine's bar to
   select it and move the disassembly to it; double-click it to zoom in, and
   double-click Top level to zoom out.
6. On the timeline across the top of the pane, rest the pointer on a bar for
   that frame's routines, turn the wheel to zoom, and drag across frames to
   profile only those frames. Alt+Left and Alt+Right step back and forward
   through the zooms and selections.
7. Time one routine: right-click its first instruction in the disassembly and
   choose **Start stopwatch here**, then **Stop stopwatch here** on its `RTS`.
   Press F5, let the game run, then break and type `STOPWATCH` for the laps.
8. Type `STOPWATCH OFF`, then press F5.

**What you should see:** the line above the list has the profile's totals and
the time its cycles cover. On the timeline, each bar has its cycles by routine,
the eight hottest in color and the rest in gray, and the same colors mark the
routines in the list. `STOPWATCH` reports the count, the last, the shortest,
the longest and the average lap.

**Reference:** [the profiler](Debugger-Reference.md#the-profiler),
[the Profiler pane](Debugger-Reference.md#the-profiler-pane),
[the flame graph and timeline](Debugger-Reference.md#the-flame-graph-and-timeline)
and [the stopwatch](Debugger-Reference.md#the-stopwatch).

## 8. Scripting

**In this tutorial:** a batch run, a script against a running Casso over the
debug channel, and an AI assistant over MCP, all on the symbols demo.

**Steps**

1. Save this script as `draw-bar.txt` in the folder Casso was cloned into:

   ```text
   ; Stop in draw_bar twice and print the count.
   SYM LOAD "Apple2\Demos\SymbolsDemo\symbols-demo.dbg"
   BP draw_bar
   G
   CALC *count
   G
   CALC *count
   ```

2. Run it in batch mode, from that folder:
   `CassoCli debug --machine Apple2e --disk1 Apple2\Demos\symbols-demo.dsk --script draw-bar.txt`.
   Add `--json` for one JSON record per reply and stop. `echo $LASTEXITCODE`
   in PowerShell prints the exit status, 0 when every command succeeded.
3. Start Casso with its debugger open and a title:
   `Casso.exe --debugger --title symbols --disk1 Apple2\Demos\symbols-demo.dsk`.
4. Type `CassoCli debug --list`, and find the `pid` of the row titled
   `symbols`.
5. Run the same script against it with
   `CassoCli debug --attach <pid> --script draw-bar.txt`. Relative paths in the
   script are resolved in the current directory of the Casso it attaches to; a
   first line of `CD` and the full path of the clone folder makes them resolve
   the same way. The breakpoint appears in the debugger window's Breakpoints
   pane, and the machine is paused at `draw_bar` when the script ends;
   press F5 in the debugger window to let it run.
6. For the same access from an AI assistant, add `CassoCli mcp` to its MCP
   client's servers, as in
   [DebuggerForAgents.md](DebuggerForAgents.md#through-an-mcp-client). The
   assistant can then `launch` Casso with a disk, `set_breakpoint` on
   `draw_bar`, `run`, `read_memory` at $60C1, and `screenshot` the screen.

**What you should see:** both runs stop at `draw_bar` twice, and the two
`CALC` lines print the value of `count`, 0 at the first stop and 1 at the
second.

**Reference:** [batch mode](Debugger-Reference.md#batch-mode),
[attaching](Debugger-Reference.md#attaching),
[the debug channel](Debugger-Reference.md#the-debug-channel),
[MCP](Debugger-Reference.md#mcp), [DebugChannel.md](DebugChannel.md) and
[DebuggerForAgents.md](DebuggerForAgents.md).
