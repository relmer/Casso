# Quickstart: Debugger Validation

Validation scenarios by phase. Command syntax is in
[contracts/cli-debug.md](contracts/cli-debug.md) and
[contracts/command-modes.md](contracts/command-modes.md). Record formats are in
[contracts/debug-channel-protocol.md](contracts/debug-channel-protocol.md).

## Prerequisites

```powershell
scripts/FetchRoms.ps1 -Fixtures
scripts/Build.ps1
scripts/RunTests.ps1 -Build
```

The ROM fixtures are gitignored. Without them the FR-027, FR-028 and FR-029
tests fail, and are not skipped.

## Phase 1: headless engine, both modes, batch

1. **Acceptance tests.** Run `scripts/RunTests.ps1 -Filter Debugger`. The run
   must include `MonitorCommandTableTests` (every shipped ROM),
   `MonitorListing1979Tests` and `MonitorRomFactsTests`, with non-zero item
   counts in their output. A filtered run is not the suite; the full suite
   runs before merge.
2. **Break on a soft-switch read (Story 1).** Use a script containing `bp C019`,
   `g`, `r`, `t`, `t`, `t`:

   ```powershell
   x64\Debug\CassoCli.exe debug --machine apple2e --disk1 UnitTest\Fixtures\dos33.dsk --script stop.txt
   ```

   Expect a breakpoint stop at the instruction that reads $C019, registers,
   and three step reports with increasing PC.
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

## Phase 2: channel

1. Start the emulator with the debugger open:
   `Start-Process x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', (Split-Path -Leaf (git rev-parse --show-toplevel)), '--debugger'`.
2. Run `x64\Debug\CassoCli.exe debug --list`. Expect this instance's PID and
   title.
3. Run `CassoCli debug --attach <pid> --command "bp C019" --command g`. Expect
   an `ok` reply, then a `stopped` notification with reason `breakpoint`.
4. **Fan-out.** Attach two clients, set a breakpoint from one, and resume.
   Both clients print the stop.
5. **Other user (manual).** From a different Windows account, run
   `debug --list`. The instance is absent, and connecting by PID fails with
   access denied.
6. **Independent client (SC-007).** A small PowerShell client that uses only
   `System.IO.Pipes.NamedPipeClientStream` and the protocol document sends
   `hello`, `bp`, `g`, and receives `stopped`.

## Phase 3: window

1. Open the debugger window from the menu. Pause. Expect disassembly with the
   PC line highlighted, and the registers, flags, stack and memory panes.
2. Click a disassembly line. Expect a breakpoint added to the list, and that
   `bpl` in the command line lists it.
3. Edit a byte in the memory view. Expect `300` in Monitor mode, or `d 300`,
   to show the new value.
4. Close the window while a client is attached. Expect the client to receive
   `closing`, and `debug --list` to omit the instance.

Kill only the Casso PID you started.
