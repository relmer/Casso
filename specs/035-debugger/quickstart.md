# Quickstart: Debugger Validation

Validation scenarios by story. Command syntax is in
[contracts/cli-debug.md](contracts/cli-debug.md),
[contracts/command-modes.md](contracts/command-modes.md) and
[contracts/gssquared-mode.md](contracts/gssquared-mode.md). Record formats are
in [contracts/debug-channel-protocol.md](contracts/debug-channel-protocol.md)
and [contracts/debug-file-format.md](contracts/debug-file-format.md).

## Prerequisites

```powershell
scripts/FetchRoms.ps1 -Fixtures
scripts/Build.ps1
scripts/RunTests.ps1 -Build
```

The ROM fixtures are gitignored. Without them the FR-027, FR-028 and FR-029
tests fail, and are not skipped.

Launch the emulator for the window scenarios with
`Start-Process x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', (Split-Path -Leaf (git rev-parse --show-toplevel)), '--debugger'`
and kill only the PID you started.

## Stories 1-3: engine, Monitor mode, channel (delivered)

1. **Acceptance tests.** Run `scripts/RunTests.ps1 -Filter Debugger`. The run
   must include `MonitorCommandTableTests` (every shipped ROM),
   `MonitorListing1979Tests` and `MonitorRomFactsTests`, with non-zero item
   counts in their output. A filtered run is not the suite; the full suite
   runs before merge.
2. **Break on a soft-switch read (Story 1).** `stop.txt` enters a loop at
   $0300 (`LDA $C019`, `JMP $0300`) and stops on the read:

   ```text
   MEB 300 AD 19 C0 4C 00 03
   R PC 300
   BPMR C019
   G
   R
   T
   T
   T
   ```

   ```powershell
   x64\Debug\CassoCli.exe debug --machine apple2e --script stop.txt
   ```

   Expect a stop with reason `watchpoint` whose access PC is $0300, then
   registers, then three step reports with increasing PC.
3. **Budget.** Run `bp 0001` then `g` with `--max-cycles 1000000`. Expect a
   stop with reason `budget` and exit status 3.
4. **Determinism.** Run step 2 twice with `--json` into two files, then
   `fc.exe /b` them. Expect no differences.
5. **Monitor mode (Story 2).** Run with `--mode monitor` and the lines
   `300: A9 41 60`, `300.302`, `300L`, `41<300.3FFS`, `300S`, `^E`,
   `: 01 02 03`, `/bpl`. Expect Monitor-format output. The search prints
   `0301`, and `/bpl` lists breakpoints set earlier in AppleWin mode.
6. **Host file I/O.** Run `800.9FFW out.bin`, then `800.9FFR out.bin`. Expect a
   512-byte file, and a read that reports no mismatch.
7. **Channel (Story 3).** With the emulator running, `CassoCli debug --list`
   shows its PID and title; `debug --attach <pid> --command "bpmr C000"
   --command g` returns `ok` then a `stopped` notification from the keyboard
   read. Two attached clients both print the stop. From another Windows
   account the instance is absent and connecting fails with access denied.
8. **Independent client (SC-007).** `scripts/DebugChannelClient.ps1`, using
   only `NamedPipeClientStream` and the protocol document, sends `hello`,
   `bp`, `g`, and receives `stopped`.

## Story 4: the window

1. Open Debug > Debugger and pause. Expect every pane in the monospace face,
   rows no taller than the font's line height plus 2 DIP (FR-026a), and the
   breakpoint, watch and stack panes showing at least ten rows without
   scrolling.
2. `SYM LOAD` a fixture debug file, then look at the disassembly. Expect each
   line that has a symbol to show it in its own column, and `JSR`/`LDA`
   operands to show the symbol with the numeric address beside it (FR-010a).
3. Click a disassembly line. Expect a breakpoint in the list and `bpl` to
   list it.
4. Switch the keyboard scheme in preferences to AppleWin, then to GSSquared;
   press each scheme's step key. Expect a step under each.
5. **SC-009.** Run the pinned throughput measurement with the window open and
   the trace off, then closed. Expect within 2%.

## Story 5: memory editing

1. Open two memory windows at `0300` and `C000`. Click the first hex cell of
   the `0300` window, type `A9`. Expect the byte to land before the next
   keystroke and the caret to be on the next cell; `d 300` prints `A9`
   (SC-012).
2. Switch the window to words, type `1234` in one cell. Expect `34 12` in
   memory.
3. Click a text cell and type `HI`. Expect `48 49` in the hex column.
4. Undo twice. Expect the two bytes restored, and the other window's undo
   list untouched.
5. Open a window at `F800`, edit a byte. Expect the disassembly at `F800` to
   show the patched instruction and the machine to execute it; `d F800`
   shows the patch.
6. Try to edit a cell in the `C000` window. Expect a refusal in the console
   and no write; `OUT C030` still works.

## Story 6: source-level debugging

1. `CassoCli as65 -g main.a65` on a source with an include file and a macro.
   Expect `main.dbg` in cc65 v2 with `file`, `line`, `span`, `seg`, `sym`
   records and a `sha1` key on each `file` (SC-017). cc65's own `dbginfo`
   sample reader, if at hand, loads it with only "unknown key" warnings.
2. `SYM LOAD main.dbg` in the window with the sources in place. Expect the
   source pane to open on the line of the PC when stopped, and to follow the
   disassembly's selection.
3. Stop inside the macro. Expect the source pane to show the invocation line
   with the body line indicated, and step into the body (FR-057).
4. Set a breakpoint on a source line; run. Expect the stop on the first
   instruction of that line (FR-055).
5. With the source pane focused, step over (`F10`, or `P` typed) a `JSR`
   followed by inline parameters (the ProDOS MLI pattern in the fixture) and a
   recursive call. Expect the next source line in both (SC-011). Click into
   the disassembly pane and step again. Expect one instruction.
6. Move the source folder; reopen. Expect a prompt; drag `main.a65` onto the
   debugger. Expect it matched by hash and the folder remembered; reopening
   finds it without the drag (FR-058, FR-060).
7. Edit a line of the source and reopen. Expect the file to open with the
   mismatch warning (FR-059).
8. Load a Merlin 8/16 listing captured from the emulated Merlin. Expect the
   listing itself as the source view and its symbols loaded (FR-033b).

## Story 7: docking

1. Drag the registers pane onto the stack pane's tab zone. Expect a tab
   group; drag it to the right edge. Expect a new split.
2. Float the memory window with the keyboard (Dock To > Float). Expect a
   top-level window; move it to a monitor of a different scale. Expect it to
   render at that monitor's scale (FR-043).
3. Auto-hide the trace pane. Expect a tab on the edge that slides the pane
   in on hover and on focus.
4. Close and reopen the debugger. Expect the same layout. Disconnect the
   second monitor and reopen. Expect the floating pane on the primary at its
   saved size (SC-014).

## Story 8: trace

1. `HISTORY ON`, run for a second, pause. Expect the trace pane to show the
   newest entries ending at the stop, each with the cycle count, PC, bytes,
   registers, the access address, direction, data and symbol; scrolling back
   reaches 100,000 entries (SC-013).
2. `HISTORY SAVE trace.txt`. Expect every retained entry in the file.
3. `HISTORY OFF`, run the SC-008 measurement. Expect the closed-window
   throughput.

## Story 9: device panels

1. Open the Disk II panel and boot a disk. Expect the phases, quarter track,
   motor and LSS rows to change while the head moves, and the head graphic to
   track them, at least once per frame (SC-015).
2. Open the MMU panel on the //e. Expect every soft switch with its bit
   decode, and the memory-map bar to change on `C008`/`C009`.
3. Open the Mockingboard panel on the speech demo. Expect the 6522 timers and
   AY registers to change and the meters to move.
4. On the ][+, expect the panel menu to omit the MMU and Mockingboard panels
   (FR-053).

## Story 10: conditional breakpoints

1. `BP 300 IF A=41` and run a loop that reaches $0300 with other values first.
   Expect one stop, with `A=41`.
2. `BPV 6 = 7` (value breakpoint) and a program that increments `$06`. Expect
   the stop after the write that makes it 7 (FR-062).
3. `BP 300 IF @C000=80`. Expect a refusal naming the I/O read.

## Story 11: profiling

1. `PROFILE ON`, run the demo for a second, `PROFILE LIST`. Expect count,
   cycles and share per mnemonic and addressing mode, and the three penalty
   rows (page crossing, taken branches, branches crossing a page).
2. `PROFILE LIST ADDR`. Expect the per-address table with symbols.
   `PROFILE SAVE profile.txt` writes the same rows.

## Story 12: GSSquared mode

1. `MODE GSSQUARED`, then `300.30F`, `bp 300`, `bpd C019 r`, `bp`, `watch
   6.7`, `l 300`, `o`, `r`. Expect each reply to match the captured fixture
   line for line (SC-016); `/bpl` lists the same breakpoints.
2. `OUTPUT APPLEWIN` then `bp`. Expect the AppleWin listing form for the same
   breakpoints; `MODE APPLEWIN` resets the output format too.
3. `bpr a=0` in AppleWin mode. Expect it accepted (FR-015a).

## Release

1. Boot the Mockingboard speech demo, open the debugger with the Mockingboard
   panel, the trace and a memory window, stop on a write to the AY, and
   capture the window for the README (FR-065).
2. Run the pre-merge gate: Debug and Release rebuilds with code analysis, both
   suites, `scripts/CheckStyle.ps1 -Mode Tree`.
