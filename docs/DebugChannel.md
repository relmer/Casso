# The Casso debug channel

A running Casso can be driven by other programs: a script, an editor
extension, or your own tool. They connect to its **debug channel**, a named
pipe that carries the same debugger commands you type in the debugger window
or give to `CassoCli debug`, and gets back structured replies.

This page covers turning the channel on, reaching it from the command line,
and the protocol itself, which is everything a client needs.

## Opening the channel

Start Casso with `--debugger`:

```
Casso.exe --debugger
```

The machine starts normally and is **not** paused. A client can attach to a
program that is already running and watch it without changing what it does.

The channel stays open until Casso exits. Breakpoints a client sets stay armed
when that client disconnects.

## From the command line

`CassoCli debug --list` shows every running Casso with a channel open:

```
pid     title       machine     disk1               disk2
20044   my-game     Apple //e   C:\Disks\game.woz   -
```

The columns are separated by tabs, so a title or a path containing spaces is
still one field. An empty drive, or an instance started without `--title`,
shows `-`.

`CassoCli debug --attach <pid>` runs commands against that instance, with the
same options a batch run takes:

```
CassoCli debug --attach 20044 --command "bp FDED" --command g
```

```
>bp FDED
Breakpoint #0 set at $FDED
>g
Stopped: breakpoint at $FDED
```

- A run the script starts is waited for before the next line runs, so each
  line acts on a stopped machine.
- Every run carries the `--max-cycles` budget (default 100,000,000).
- A run that has not stopped after `--timeout` seconds (default 120) is
  paused, and the command exits with status 3. The machine is left paused where
  it got to rather than running on after the script has gone.
- `--json` prints the records exactly as they arrive, one per line.

Exit statuses match a batch run:

| Status | Meaning |
|---|---|
| 0 | every line ran |
| 1 | a command failed |
| 2 | the channel closed, or no instance with that process id has one open |
| 3 | a run stopped on its budget, or outlasted `--timeout` and was paused |

## Connecting

The pipe is `\\.\pipe\Casso.Debug.<pid>`, with the Casso process id in decimal.
Any number of clients may connect at once.

Only the Windows account that started Casso can connect, and connections from
other computers are rejected. The channel can read and write the machine's
memory, so no other account is let in.

To find instances yourself, list the entries of `\\.\pipe\` whose names start
with `Casso.Debug.`, connect to each, and send `hello`.

A minimal PowerShell client:

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream ('.', 'Casso.Debug.20044', 'InOut')
$pipe.Connect (5000)

$writer = New-Object System.IO.StreamWriter ($pipe)
$writer.AutoFlush = $true
$writer.NewLine   = "`n"
$reader = New-Object System.IO.StreamReader ($pipe)

$writer.WriteLine ('{"type":"hello","id":1,"protocol":1}')
$reader.ReadLine()
```

## Framing

- One UTF-8 JSON object per line, ending in LF. A CR before the LF is accepted
  and never sent.
- No record contains a raw newline; newlines inside strings are escaped.
- A request line may be at most 1 MiB. A longer one is answered with an `error`
  record and the connection stays open.
- Every record has a `type`. **Ignore fields you do not recognize.** New fields
  are added without changing the protocol version.

## Handshake

Send `hello` first. It is optional, but it tells you what you reached.

```json
{"type":"hello","id":1,"client":"my-tool","protocol":1}
```

```json
{"type":"hello","id":1,"protocol":1,"pid":20044,"title":"my-game",
 "machine":"Apple //e","disks":["C:\\Disks\\game.woz",null],
 "mode":"applewin","state":"running"}
```

`state` is `running` or `paused`. An empty drive is `null`. Casso always answers
with its own `protocol`; if yours is newer, decide for yourself whether to go
on.

## Requests

```json
{"type":"command","id":7,"line":"bp 0300"}
{"type":"command","id":8,"line":"300.30F","mode":"monitor"}
{"type":"command","id":9,"line":"g","budget":5000000}
{"type":"pause","id":10}
```

| Field | Required | Meaning |
|---|---|---|
| `type` | yes | `hello`, `command` or `pause` |
| `id` | yes | an integer you choose, echoed in the reply; unique on your connection |
| `line` | for `command` | one command line, exactly as you would type it |
| `mode` | no | `applewin` or `monitor`, for this line only. Other clients keep the mode they have |
| `budget` | no | the cycle budget for a run this line starts, for this line only. Without it a run is unbounded unless a `BUDGET` command set one. Someone may be using the machine, so set a budget only for runs nobody is watching |

Commands from every client run one at a time, in the order Casso receives
them. A reply goes only to the client that sent the request.

`pause` stops a running machine. Its reply comes at once, and the stop arrives
as a `stopped` notification.

## Replies

```json
{"type":"reply","id":7,"status":"ok","command":"bp 0300",
 "data":{"kind":"breakpointSet","breakpoint":{"id":0,"kind":"address","address":768,"enabled":true}},
 "text":["Breakpoint #0 set at $0300"]}
```

| Field | Meaning |
|---|---|
| `status` | `ok`, `error`, `notAvailable` or `unknown` |
| `command` | the line as received |
| `data` | the typed result; `kind` says which fields it has. Absent unless `status` is `ok` |
| `text` | the lines the debugger window would print for this command |
| `error` | when `status` is not `ok`: `{"label":"...","detail":"..."}` |
| `running` | `true` when the command left a run going. Absent otherwise |

**All numbers are JSON integers.** Addresses and bytes are never hex strings,
so format them however your client prefers.

### Runs

Run commands (`g`, `p`, `t`, and the Monitor's `G`, `S` and `T`) reply as soon
as the run starts. When the reply carries `"running":true`, a `stopped`
notification with `causeId` equal to your request's `id` is still to come. When
it does not, the run already finished and its `stopped` notification has
already arrived.

### Data kinds

| kind | Fields |
|---|---|
| `registers` | `a`, `x`, `y`, `p`, `s`, `pc`, and `flags` with `n`, `v`, `b`, `d`, `i`, `z`, `c` as booleans |
| `memory` | `rows`: `{address, bytes, region}`, where `region` is `mainRam`, `auxRam`, `lcBank1`, `lcBank2`, `rom`, `slotRom` or `io`. An I/O byte that cannot be read without side effects is `null` |
| `disassembly` | `lines`: `{address, bytes, mnemonic, operand, target, symbol, documented}` |
| `breakpointSet`, `breakpointList` | `breakpoint` or `breakpoints`: `{id, kind, address, last, opcode, condition, access, mode, enabled, hits}`, with absent fields left out |
| `watchList`, `zeroPageList`, `bookmarkList` | `entries`: `{id, address, enabled, value}` |
| `searchHits` | `addresses` |
| `stack` | `sp`, and `entries`: `{address, value}` |
| `softSwitches` | `switches`: `{name, value}` |
| `symbols` | `symbols`: `{name, address, table}` |
| `cycles` | `count` |
| `mode` | `mode` |
| `fileIo` | `path`, `requested`, `transferred`, `mismatch` |
| `message` | nothing beyond `text` |

More kinds may be added. For a kind you do not recognize, show `text`.

## Notifications

Notifications have no `id` and go to every connected client.

```json
{"type":"stopped","reason":"breakpoint","pc":768,"breakpointId":0,"causeId":9,"cycles":1834211,
 "registers":{"a":0,"x":1,"y":2,"p":48,"s":255,"pc":768}}
{"type":"resumed"}
{"type":"reset","kind":"soft"}
{"type":"machineChanged","machine":"Apple //c"}
{"type":"modeChanged","mode":"monitor"}
{"type":"closing"}
```

| `stopped.reason` | What stopped the machine |
|---|---|
| `breakpoint`, `watchpoint` | a breakpoint or watchpoint fired. A `watchpoint` stop carries `watch` with the address, the access, and whether it stopped `after` the access or `before` the instruction that would make it |
| `step` | a step, step-over or step-out finished |
| `runTo` | the run-to address was reached |
| `budget` | the run's cycle budget ran out |
| `pause` | a `pause` request, or someone paused Casso itself |
| `brk`, `invalidOpcode` | a `BRK` or an invalid opcode, when those stops are on |

- `causeId` is present only when a command's run ended. A breakpoint hit while
  the machine was simply running, or a pause from the Casso window, carries
  none.
- `machineChanged` means every breakpoint and watchpoint was cleared, since
  their addresses belonged to the old machine. `reset` keeps them.
- `closing` is the last record before the pipe closes.

## Errors outside a command

```json
{"type":"error","error":{"label":"malformed request","detail":"The line is not a JSON object."}}
```

Sent for a line that is not JSON, a record with no `type` or `id`, or a line
over 1 MiB. The connection stays open.

## Versioning

`protocol` goes up only when an existing field changes meaning or is removed.
New fields, data kinds, notifications and stop reasons do not change it.
