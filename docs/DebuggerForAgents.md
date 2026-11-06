# Driving Casso's debugger from an AI agent

This page is for an AI agent, or any automated tool, that drives a Casso the
user already has running: it starts the debug channel, sends debugger commands
in a chosen command mode through `CassoCli debug --attach`, and reads the
replies. The commands themselves are in
[Debugger-Reference.md](Debugger-Reference.md) and
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

To stop when the disk in drive 1 has track $11 sector 0 read, in the default
mode, and see what the drive did:

```text
CassoCli debug --attach 20044 --json ^
    --command "BPDISK D1 READ 11 0" --command "g" --command "bpc 0"
```

The `stopped` record has the reason `disk` and a `disk` object with the field's
track, sector, volume and checksums and the instructions that read it;
[DebugChannel.md](DebugChannel.md) lists its fields, and
[Debugger-Reference.md](Debugger-Reference.md#disk-breakpoints) the other disk
events `BPDISK` takes.


## Through an MCP client

`CassoCli mcp` is a Model Context Protocol server for assistants whose client
speaks MCP, whether or not it has a shell. It runs on standard input and
output, so a client starts it as it starts any local server. In a client that
takes the usual `mcpServers` setting:

```json
{
  "mcpServers": {
    "casso": {
      "command": "C:\\Program Files\\Casso\\CassoCli.exe",
      "args": ["mcp"]
    }
  }
}
```

The server connects to Casso through the debug channel only. The tools are
for one Casso at a time: the one `launch` started or `attach` selected, else
the only one running.

| Tool | What it does |
|---|---|
| `list_instances`, `attach` | lists the instances with a channel open; `attach` selects one by process id |
| `launch`, `close` | starts the `Casso.exe` beside `CassoCli.exe` with its debugger open, optionally with a machine, disks and a title; `close` closes only an instance the server launched |
| `command` | any debugger line, in a command mode, and its reply |
| `run`, `resume`, `pause`, `step` | `run` runs to a stop, the address given, its cycle budget or its timeout, and pauses a run past the timeout; `resume` lets a paused machine run on and returns at once |
| `registers`, `read_memory`, `write_memory`, `disassemble` | the CPU and memory |
| `set_breakpoint`, `clear_breakpoint`, `list_breakpoints` | breakpoints, by address, symbol or `file:line` |
| `reset`, `type_text`, `press_keys` | Ctrl+Reset or a power cycle, text typed as a paste, and keys by name, `Ctrl+Reset` among them |
| `insert_disk`, `eject_disk` | the disk drives |
| `read_screen`, `screenshot` | the text screen, and the picture as a PNG image |
| `wait` | a number of seconds while the machine runs |

Each result has the reply's text, and its typed data as structured content;
a command that fails makes the result an error with the reply's label and
detail. The results of `wait`, `reset`, `type_text` and `press_keys` have a
line when the machine is paused, since a paused machine doesn't read what is
typed: call `resume`.

A session that types in and runs a program, as GSSquared's demo does:

1. `launch`, then `wait` five seconds for the boot to fail.
2. `press_keys` with `["Ctrl+Reset"]`.
3. `type_text` with the program and `RUN`, one line each, such as
   `10 GR`, `20 FOR I = 0 TO 15`, `30 COLOR= I`, `40 VLIN 0,39 AT I * 2 + 4`,
   `50 NEXT`.
4. `wait` a few seconds, then `screenshot`.

### Host files

A command from the channel that reads or writes a file on the host, such as
`BLOAD`, `BSAVE`, `W`, `R`, `SYM LOAD`, `DISK INSERT`, `SCREENSHOT`, `SAVE` or
`CD`, can access only these folders:

- the folders of the mounted disk images;
- the folders of the loaded debug and symbol files;
- `Documents\Casso`;
- each folder given with `--allow-path`.

The path is checked after `.`, `..`, relative parts, links and junctions are
resolved, so a link inside an allowed folder that points out of it fails. These
always fail:

- a UNC or network path (`\\server\share`, `\\?\UNC\...`, or a mapped drive),
  or a device path (`\\.\`, `\\?\`);
- a write of a file with the extension `.exe`, `.dll`, `.bat`, `.cmd`, `.ps1`,
  `.vbs`, `.js`, `.lnk` or `.scr`, or into a Startup folder;
- a write over a file that exists, unless the `command` tool's `overwrite`
  argument is `true`;
- a command that runs a file of commands (`RUN`, `LOAD`, `STARTUP`), unless
  the server was started with `--allow-scripts`.

A failure is an error result with the label `file access denied` and the
reason as its detail, such as `C:\Games\x.bin is outside the allowed folders.`
Every access adds a line to the console in Casso's debugger window with the
client's name and the full path, and a failure adds a line with the reason.

To let the client work in a project folder, and run scripts:

```json
{
  "mcpServers": {
    "casso": {
      "command": "C:\\Program Files\\Casso\\CassoCli.exe",
      "args": ["mcp", "--allow-path", "C:\\Work\\Game", "--allow-scripts"]
    }
  }
}
```

A relative `--allow-path` is taken from the directory the server starts in.
The rules apply to every command from the channel, so `CassoCli debug --attach`
has them too, with no extra folders and no scripts. Commands typed in Casso's
console, and scripts run from it, have none of them.
