# Driving Casso's debugger from an AI agent

This page is for an AI agent, or any automated tool, that drives a Casso the
user already has running: it starts the debug channel, sends debugger commands
in a chosen command mode through `CassoCli debug --attach`, and reads the
replies. The commands themselves are in [Debugger.md](Debugger.md) and
[Debugger-Commands.md](Debugger-Commands.md); the wire protocol underneath is in
[DebugChannel.md](DebugChannel.md).

## Ground rules

- **Someone may be using the machine.** Attaching does not pause it. Prefer
  commands that only read (`R`, `D`, `U`, `BPL`, `SWITCHES`, `MAP`) until the
  user has asked for a change, and say what you changed when you are done.
- **Every run is bounded.** `--max-cycles` (default 100,000,000) caps each run a
  line starts, and `--timeout` (default 120 seconds) caps the wait for it. A run
  that outlasts the timeout is paused, so the machine is left stopped where it
  got to, not running on after your script ends.
- **One invocation is one script.** Each `CassoCli debug --attach` call opens the
  channel, runs its lines in order and exits. Breakpoints, watches and symbols
  you set stay in Casso after it exits; the command mode you switch to with
  `MODE` does not, since `--mode` and `MODE` lines apply to your connection
  alone.

## 1. Start the channel

The channel is open whenever Casso's debugger window is. Either the user starts
Casso with it:

```text
Casso.exe --debugger --title my-game
```

or chooses **Debug > Debugger...** in a Casso that is already running. You
cannot open the channel from outside; if `--list` below shows nothing, ask the
user to do one of these.

## 2. Find the instance

```text
CassoCli debug --list
```

```text
pid     title       machine     disk1               disk2
20044   my-game     Apple //e   C:\Disks\game.woz   -
```

The columns are separated by tabs. Pick the row by `title` or by the disk the
user mentioned, and use its `pid`. With `--json` the list is machine-readable.

## 3. Send commands

Give lines with `--command` (repeatable, run in order) or a file with
`--script`:

```text
CassoCli debug --attach 20044 --command "R" --command "D 300,10"
CassoCli debug --attach 20044 --script steps.txt
```

In a script, a line starting with `;` is a comment. A run command (`G`, `T`,
`P`, and each mode's own forms) is waited for until the machine stops, so the
next line always acts on a stopped machine.

### Choosing a mode

`--mode` sets the command mode the lines are read in: `applewin` (the default),
`monitor`, `gssquared`, `windbg` or `casso`. Pick the one whose commands you
know best; every mode reaches every Casso command through its marker (a bare
name in AppleWin, Casso and GSSquared modes, `/` in Monitor mode, `!` in WinDbg
mode):

```text
CassoCli debug --attach 20044 --mode windbg --command "bp 300" --command "g" --command "r"
CassoCli debug --attach 20044 --mode monitor --command "300.30F" --command "/map"
```

A `MODE name` line switches the lines after it within the same script. Replies
are written in the mode's own format.

## 4. Read the replies

By default the output echoes each line after a prompt (`>` in AppleWin, Casso
and GSSquared modes, `*` in Monitor mode, `0:000> ` in WinDbg mode) and then the
lines the debugger window would print:

```text
>bp FDED
Breakpoint #0 set at $FDED
>g
Stopped: breakpoint at $FDED
```

For anything you will parse, add `--json`. Each record the channel sends is
printed as it arrived, one JSON object per line:

- a `reply` per command, with `status` (`ok`, `error`, `notAvailable` or
  `unknown`), `data` (typed, with a `kind`; all numbers are integers, never hex
  strings), `text` (the window's lines) and, when the status is not `ok`,
  `error` with a `label` and a `detail`;
- a `stopped` notification when a run ends, with the `reason`, the `pc` and the
  registers;
- other notifications (`resumed`, `reset`, `machineChanged`, `modeChanged`,
  `closing`) as they happen.

Ignore fields and kinds you do not recognize; for an unknown `kind`, use
`text`. [DebugChannel.md](DebugChannel.md) lists every kind.

## 5. Check the exit status

| Status | Meaning | What to do |
|---|---|---|
| 0 | every line ran | read the output |
| 1 | a command failed (`error` or `unknown`) | read that reply's error and fix the line |
| 2 | the channel closed, or no instance with that pid has one open | run `--list` again; ask the user to open the debugger |
| 3 | a run stopped on its budget, or outlasted `--timeout` and was paused | the machine is paused; inspect it, or raise `--max-cycles` or `--timeout` |

`notAvailable` does not fail the script: the command exists but does not apply
to this machine or this way of running, and its `detail` says why.

## A worked example

Stop at the next character output, look at the registers and the memory map,
and step one instruction, all in WinDbg mode:

```text
CassoCli debug --attach 20044 --mode windbg --json ^
    --command "bp FDED" --command "g" --command "r" --command "!map" --command "t" --command "bc 0"
```

The last line clears the breakpoint again, so the user's machine is left as it
was apart from where it stopped.
