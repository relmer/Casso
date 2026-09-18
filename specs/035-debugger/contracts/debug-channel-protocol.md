# Contract: Debug Channel Protocol

**Version**: 1

This document is the whole contract for a client of a running Casso's debug
channel, and is written so that a client (the planned VS Code debug adapter)
can be built from it alone. The same records are what `CassoCli debug --json`
prints.

## Transport

- **Pipe name**: `\\.\pipe\Casso.Debug.<pid>`, where `<pid>` is the Casso
  process ID in decimal with no padding.
- **Duplex byte-mode named pipe.** Open it with `CreateFileW (name,
  GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr)`, or
  with any runtime's named-pipe client (Node: `net.connect ('\\\\.\\pipe\\Casso.Debug.1234')`).
- **Availability**: the pipe exists only while that instance's debugger is
  open (the debugger window, or a Casso started with `--debugger`). If the pipe
  does not exist, the debugger is closed.
- **Access**: only the user account that runs Casso can connect. Remote
  connections are rejected.
- **Clients**: any number may connect at once.
- **Discovery**: enumerate the entries of `\\.\pipe\` whose names start with
  `Casso.Debug.`, connect, and send `hello`. `CassoCli debug --list` does
  exactly this.

## Framing

- **UTF-8 JSON, one object per line**, terminated by LF (`\n`). CR before LF is
  tolerated on input and never sent.
- **No object contains a raw newline.** Newlines inside strings are escaped as
  `\n`.
- **Maximum line length is 1 MiB.** A longer line is rejected with an `error`
  record and the connection stays open.
- **Every record has a `type` field.** Clients MUST ignore fields they do not
  recognize, and servers MUST ignore unknown request fields, so fields can be
  added without a version bump.

## Handshake

The first record a client sends SHOULD be `hello`. A client MAY send commands
without it.

```json
{"type":"hello","id":1,"client":"vscode-casso","protocol":1}
```

```json
{"type":"hello","id":1,"protocol":1,"pid":1234,"title":"035-debugger",
 "machine":"Apple //e Enhanced","disks":["C:\\Disks\\game.woz",null],
 "mode":"applewin","state":"paused"}
```

If the client's `protocol` is higher than the server's, the server still
replies with its own `protocol`, and the client decides whether to continue.

## Requests

```json
{"type":"command","id":7,"line":"bp 0300"}
{"type":"command","id":8,"line":"300.30F","mode":"monitor"}
{"type":"command","id":9,"line":"g","budget":5000000}
```

| Field | Required | Meaning |
|---|---|---|
| `type` | yes | `hello`, `command`, `pause` |
| `id` | yes | client-chosen integer, echoed in the reply; unique per connection |
| `line` | for `command` | one command line, exactly as typed in the chosen mode |
| `mode` | no | `applewin` or `monitor` for this line only; default is the session's mode |
| `budget` | no | cycle budget for a run started by this line. Absent: the session's `BUDGET`, which is unbounded unless a client set one. A person may be using the machine, so a client should set a budget only for unattended runs |

- **Ordering**: commands from all clients run one at a time in the order the
  server receives them. A reply is sent only to the client that sent the
  request.
- **`pause`**: needs no `line`. It stops a running machine and produces a
  `stopped` notification with reason `pause`. Its reply is sent at once.

## Replies

```json
{"type":"reply","id":7,"status":"ok","command":"bp 0300",
 "data":{"kind":"breakpointSet","breakpoint":{"id":0,"kind":"address","address":768,"enabled":true}},
 "text":["Breakpoint #0 set at $0300"]}
```

| Field | Meaning |
|---|---|
| `status` | `ok`, `error`, `notAvailable`, `unknown` |
| `command` | the line as received |
| `data` | typed payload; `kind` identifies the shape (see below). Absent on `error` and `unknown` |
| `text` | array of lines, the same text the window and batch mode print in the line's mode |
| `error` | on non-`ok`: `{"label":"...","detail":"..."}`, the two-line error shape |
| `running` | `true` when the command left a run in progress, so a `stopped` notification with this `id` as `causeId` is still to come. Absent otherwise. Added during implementation so an unattended client (`debug --attach`) knows whether to wait |

**All numbers are JSON integers. Addresses and bytes are never hex strings.**
Clients format them.

- **Run commands** (`g`, `p`, `t` and the Monitor `G`/`S`/`T`) reply `ok` when
  the run starts. The outcome arrives as a `stopped` notification.
- **Commands still in progress**: a `stopped` notification caused by a command
  carries that command's `id` as `causeId`.

### Data kinds

| kind | Fields |
|---|---|
| `registers` | `a`, `x`, `y`, `p`, `s`, `pc`, `flags` (`{"n":bool,"v":…,"b":…,"d":…,"i":…,"z":…,"c":…}`) |
| `memory` | `rows`: `[{"address":int,"bytes":[int],"region":"mainRam"|"auxRam"|"lcBank1"|"lcBank2"|"rom"|"slotRom"|"io"}]`; an unreadable I/O byte is `null` |
| `disassembly` | `lines`: `[{"address":int,"bytes":[int],"mnemonic":str,"operand":str,"operandAddress":int|null,"operandSymbol":str|null,"target":int|null,"label":str|null,"documented":bool}]`. `operand` is always numeric; `operandSymbol` names `operandAddress` when a symbol is loaded, and `label` names `address` (or the data block the line starts). |
| `breakpointSet`, `breakpointList` | `breakpoint` / `breakpoints`: `{"id","kind","address","last","opcode","condition","access","mode","enabled","hits"}` (absent fields omitted). `mode` is `after` or `before` on a memory watchpoint |
| `watchList`, `zeroPageList`, `bookmarkList` | `entries`: `[{"id","address","enabled","value"}]` |
| `searchHits` | `addresses`: `[int]` |
| `stack` | `sp`, `entries`: `[{"address":int,"value":int}]` |
| `softSwitches` | `switches`: `[{"name":str,"value":bool}]` |
| `symbols` | `symbols`: `[{"name","address","table"}]` |
| `cycles` | `count` |
| `mode` | `mode` |
| `fileIo` | `path`, `requested`, `transferred`, `mismatch`: bool |
| `message` | no fields beyond `text` |

New kinds may be added; a client that does not recognize a kind uses `text`.

## Notifications

Notifications have no `id` and are sent to every connected client.

```json
{"type":"stopped","reason":"breakpoint","pc":768,"breakpointId":0,"cycles":1834211,
 "registers":{"a":0,"x":1,"y":2,"p":48,"s":255,"pc":768}}
{"type":"stopped","reason":"watchpoint","pc":2051,"watch":{"id":1,"address":1024,"value":65,"previous":160,"access":"write","accessPc":2048,"mode":"after"}}
{"type":"resumed"}
{"type":"reset","kind":"soft"}
{"type":"machineChanged","machine":"Apple //c"}
{"type":"modeChanged","mode":"monitor"}
{"type":"closing"}
```

| `stopped.reason` | Cause |
|---|---|
| `breakpoint`, `watchpoint` | a table entry fired. A `watchpoint` stop carries `watch`: `mode` says whether it stopped `after` the access or `before` the instruction that would make it; `value` is the byte read or written, and `previous` the byte a write replaced, absent where the address is served by a device or the mode is `before` |
| `step` | a step, step-over or step-out completed |
| `runTo` | the run-to address was reached |
| `budget` | the run's cycle budget was spent |
| `pause` | a `pause` request, or the user paused in Casso |
| `brk`, `invalidOpcode` | `BRK` / `BRKOP` stops |

- **`closing`** is the last record before the server closes the pipe, because
  the debugger was closed or Casso is exiting. Breakpoints and pause state are
  unaffected.
- **`machineChanged`** means all breakpoints and watchpoints were cleared.
- **`reset`** keeps them.

## Errors outside a command

```json
{"type":"error","error":{"label":"malformed request","detail":"The line is not a JSON object."}}
```

Sent for unparsable lines, a missing `type` or `id`, or an oversize line. The
connection stays open.

## Example session

```text
> {"type":"hello","id":1,"protocol":1}
< {"type":"hello","id":1,"protocol":1,"pid":1234,...,"state":"running"}
> {"type":"command","id":2,"line":"bp 0300"}
< {"type":"reply","id":2,"status":"ok",...}
< {"type":"stopped","reason":"breakpoint","pc":768,"breakpointId":0,...}
> {"type":"command","id":3,"line":"r"}
< {"type":"reply","id":3,"status":"ok","data":{"kind":"registers",...},"text":["A=00 X=01 ..."]}
> {"type":"command","id":4,"line":"g"}
< {"type":"reply","id":4,"status":"ok",...}
< {"type":"resumed"}
```

## Versioning

`protocol` increases only when an existing field changes meaning or is
removed. Added fields, data kinds, notification types and stop reasons do not
change it.
