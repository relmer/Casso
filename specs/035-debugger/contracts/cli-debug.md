# Contract: `CassoCli debug`

## Batch mode (phase 1)

```text
CassoCli debug --machine <name> [--disk1 <path>] [--disk2 <path>]
               --script <path> | --command <line> ...
               [--mode applewin|monitor|gssquared|windbg] [--output <format>] [--json] [--max-cycles <n>]
               [--write-disks]
```

| Option | Meaning |
|---|---|
| `--machine` | machine configuration name, as the emulator accepts it |
| `--disk1`, `--disk2` | disk images for drive 1 and 2 |
| `--script` | file of command lines; `-` reads standard input |
| `--command` | one command line; repeatable; runs after `--script` if both are given |
| `--mode` | starting command mode; default `applewin` |
| `--output` | starting output format, `applewin`, `monitor`, `gssquared` or `windbg` (FR-013); default: the mode's own |
| `--json` | print one JSON Lines record per reply and notification ([protocol](debug-channel-protocol.md)) instead of text |
| `--max-cycles` | cycle budget for every run in the script; default 100000000 |
| `--seed` | the DRAM power-on pattern seed; default `0xCA550001`, so two runs are identical |
| `--write-disks` | persist guest disk writes; without it, writes go to an in-memory overlay |

**Behavior**

- The machine is built paused at power-on reset. The script runs line by line,
  and the process exits after the last line.
- Text mode prints each command line prefixed by the mode's prompt (`>` for
  AppleWin and GSSquared, `*` for Monitor, `0:000>` for WinDbg), then its text output. Stop notifications print as
  text lines.
- JSON mode prints `reply` records and notification records only, in order.
- **Script format**: one command per line, in the current mode. Blank lines
  are ignored. A line whose first non-blank character is `;` is a comment in
  both modes, since `;` begins no command in either.
- **Mode switches** apply to the following lines.
- **Errors**: an `error` or `unknown` status does not stop the script. It sets
  the exit status.

**Exit status**

| Code | Meaning |
|---|---|
| 0 | every command returned `ok` or `notAvailable` |
| 1 | at least one command returned `error` or `unknown` |
| 2 | usage error, or the machine or a disk could not be loaded |
| 3 | the last run the script started ended with reason `budget`, whether or not other commands followed it; reported so a CI script can detect a missed stop. Status 1 takes precedence when a command also failed |

## Instance listing and attach (phase 2)

```text
CassoCli debug --list [--json]
CassoCli debug --attach <pid> --script <path> | --command <line> ...
               [--json] [--max-cycles <n>] [--timeout <seconds>]
```

- `--list` prints one line per instance with an open debugger:
  `pid  title  machine  disk1  disk2`. JSON prints the `hello` records.
- `--attach` sends each line as a `command` request and prints replies and
  notifications. Every run it starts carries `budget` from `--max-cycles`
  (default 100000000), since an attach script is unattended. After a run
  command it waits for the `stopped` notification before sending the next
  line, for at most `--timeout` seconds of wall-clock time (default 120,
  since the emulator runs throttled); on timeout it sends `pause`, prints the
  stop, and exits with status 3.
- It exits when the script ends, or with status 2 when the pipe closes.

## Emulator switch (phase 2)

```text
Casso --debugger
```

Opens the debugger when the machine starts: the channel in phase 2, and the
window plus channel from phase 3. It does not pause the machine.
