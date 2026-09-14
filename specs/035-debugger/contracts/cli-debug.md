# Contract: `CassoCli debug`

## Batch mode (phase 1)

```text
CassoCli debug --machine <name> [--disk1 <path>] [--disk2 <path>]
               --script <path> | --command <line> ...
               [--mode applewin|monitor] [--json] [--max-cycles <n>]
               [--write-disks]
```

| Option | Meaning |
|---|---|
| `--machine` | machine configuration name, as the emulator accepts it |
| `--disk1`, `--disk2` | disk images for drive 1 and 2 |
| `--script` | file of command lines; `-` reads standard input |
| `--command` | one command line; repeatable; runs after `--script` if both are given |
| `--mode` | starting command mode; default `applewin` |
| `--json` | print one JSON Lines record per reply and notification ([protocol](debug-channel-protocol.md)) instead of text |
| `--max-cycles` | default cycle budget for every run in the script; default 100000000 |
| `--write-disks` | persist guest disk writes; without it, writes go to an in-memory overlay |

**Behavior**

- The machine is built paused at power-on reset. The script runs line by line,
  and the process exits after the last line.
- Text mode prints each command line prefixed by the mode's prompt (`>` for
  AppleWin, `*` for Monitor), then its text output. Stop notifications print as
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
| 3 | a run ended at its cycle budget and the script ended without any later command; reported so a CI script can detect a missed stop |

## Instance listing and attach (phase 2)

```text
CassoCli debug --list [--json]
CassoCli debug --attach <pid> --script <path> | --command <line> ... [--json]
```

- `--list` prints one line per instance with an open debugger:
  `pid  title  machine  disk1  disk2`. JSON prints the `hello` records.
- `--attach` sends each line as a `command` request and prints replies and
  notifications. After a run command it waits for the `stopped` notification,
  up to the run's budget, before sending the next line.
- It exits when the script ends, or with status 2 when the pipe closes.

## Emulator switch (phase 2)

```text
Casso --debugger
```

Opens the debugger when the machine starts: the channel in phase 2, and the
window plus channel from phase 3. It does not pause the machine.
