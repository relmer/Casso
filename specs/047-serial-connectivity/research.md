# Research: Serial Connectivity

Decisions taken while planning 047, each with the reason and the alternatives
weighed. Prior art (AppleWin, MAME, DOSBox, VICE, tcpser) was consulted
through its documentation only.

## R1. Where the host side runs: polled on the emulation thread

**Decision**: Every host attachment (COM port, TCP, modem) is polled from
`Acia6551::Tick` on the emulation thread, at most once every
`kActivePollCycles` (1,024 cycles, about 1 ms) while traffic is flowing and
every `kIdlePollCycles` (8,192 cycles, about 8 ms) after a quiet poll. Nothing
is polled while `HostInputGate::IsHeld` returns true.

**Rationale**: the Uthernet II already works this way (`UthernetCard::Tick`
pumps `W5100SocketBridge` on the emulation thread). Every host event is then
taken on the thread that journals it, in the order it arrived, with no
cross-thread queue to reconcile. A paused machine polls nothing, so the OS
buffers the traffic until the machine runs again, as a real UART's FIFO would.
At the idle interval an idle line costs roughly 125 non-blocking calls a
second (SC-006).

**Alternatives**: a reader thread per attachment pushing through the existing
thread-safe `EnqueueReceivedBytes` / `SetModemInputs`. Not chosen: bytes, line
changes and errors would arrive at the chip through two different queues, and
the journal has to preserve their relative order (a byte with an error, then
DCD dropping). A thread per attachment also has to be woken and shut down,
which a polled attachment does not.

## R2. What is journaled

**Decision**: Only the 6551's inputs, as observed entries stamped with the
cycle the instruction began at:

| Entry | `kind` | `value` | `detail` | `data` | `payload` |
|---|---|---|---|---|---|
| a byte | `AciaByte` | the byte | chip index | error bits (`kRxError*`) | none |
| line levels | `AciaLines` | CTS/DSR/DCD bits | chip index | none | none |
| connection opened or closed | `AciaConnection` | 1 opened, 0 closed | chip index | none | peer text |

`AciaByte` entries written before 047 have `data` 0, so they read as bytes
without errors. Host-side state (the modem's mode and S-registers, the socket,
the COM handle) is never journaled.

**Rationale**: the brief and the Uthernet precedent: a replay reproduces the
guest's view without the host. The modem is host-side equipment, so its result
codes and echoed characters are bytes sent toward the chip, journaled like any
other.

**Alternatives**: the modem as machine state, saved in keyframes and replayed
rather than journaled. Not chosen: a replay would then run the modem's command
interpreter, and a dial command in a replay has no socket to dial. FR-019 also
resets the modem's state when a state file loads.

## R3. Replay timing of line changes

**Decision**: Live, a line change from a poll is stored as the chip's pending
modem inputs and applied in the same `Tick`. In a replay, `ApplyInput` stores
the journaled levels the same way, and the next `Tick` applies them. Either
way the status register changes after the instruction at the entry's cycle.

**Rationale**: `Replayer` applies an observed entry just before the
instruction that starts at its cycle. Applying a line change there directly
would change the status register one instruction earlier than live, and a
guest polling the status register would read the difference. A byte is not
affected: it goes into the receive queue, and the receiver first looks at it
in the tick either way.

## R4. Receive errors

**Decision**: Each receive queue entry has an error byte beside the data byte.
`LandReceivedByte` sets the parity and framing bits from it, as the 6551 does
for each character (they clear on the next good one), and a host-side overrun
sets the overrun bit. A break arrives as $00 with a framing error, as on the
part.

The Windows serial driver reports errors through `ClearCommError` without the
position of the byte in error. The COM attachment calls it before each read and
attributes an error to the last of the bytes then waiting, which is the byte
that was arriving when the error was latched. At a poll every millisecond the
uncertainty is a few characters.

**Alternatives**: `DCB.fErrorChar` substitution. Not chosen: it reports parity
errors only, and it replaces the byte the guest is owed.

## R5. COM port line format

**Decision**: The COM attachment runs the PC port at the transmit baud rate the
6551's control register selects (code 0 is the crystal divided by 16, 115,200
baud), with the guest's word length, parity (odd, even, mark, space or none)
and stop bits (1, 1.5 with a 5-bit word and no parity, else 2). A PC port has
one rate for both directions, so the receiver's external clock setting is not
reproduced; the transmit rate is used, and the docs state it.

If the driver reports `BAUD_USER` in `COMMPROP.dwSettableBaud`, the rate is
passed through, rounded to an integer. Otherwise the nearest rate in the mask
is used, and an infobar reports the substitution once per rate when it differs
by more than 2%.

Hardware flow control in the driver is off. CTS is passed to the 6551, and the
6551 stops its transmitter while CTS is false, so the PC port never waits on
CTS itself.

## R6. Host name lookup

**Decision**: `IHostNetwork::LookUpHost` starts a lookup and returns an
`IHostNameLookup` the attachment polls. The Win32 implementation is an
asynchronous `GetAddrInfoExW`, canceled when the lookup is abandoned. A dotted IPv4 literal completes at once
without a lookup. The fake completes from a table the test fills.

**Rationale**: `getaddrinfo` can block for seconds, and the attachment runs on
the emulation thread.

## R7. Telnet

**Decision**: With the Telnet option on (default off for TCP, on for the
modem), `TelnetCodec`:

- doubles $FF in data sent and undoubles it in data received;
- replies to `DO SGA` with `WILL SGA`, to `WILL ECHO` and `WILL SGA` with
  `DO`, to every other `DO` with `WONT` and to every other `WILL` with
  `DONT`; a request for the state an option is in already is not
  acknowledged again, so negotiation cannot loop;
- drops subnegotiation (`SB` ... `IAC SE`) and the other `IAC` commands;
- sends CR as CR NUL, and drops the NUL after a received CR (RFC 854's NVT
  rule, since binary mode is never agreed).

**Rationale**: BBSes negotiate echo and suppress-go-ahead to run
character-at-a-time. With every other option declined, the remote stays in plain NVT.

## R8. Modem behavior beyond the spec's list

**Decisions**:

- Command buffer: 40 characters after `AT`; more is `ERROR`.
- `X0` reports `CONNECT`; `X1`-`X4` report `CONNECT <rate>` with the 6551's
  baud rate, and numeric codes 1, 5, 10, 11 and 12 for 300, 1200, 2400, 4800
  and 9600 (1 for any other rate). The default is `X4`, as on later Hayes
  modems (owner decision 2026-10-28); `ATX0` gives the bare word.
- Accepted and ignored so period initialization strings succeed: `B`, `L`,
  `M`, `N`, `W`, `X`, `Y`, `&C`, `&D`, `&K`, `&S`, `&W`, each with an optional
  digit. `&C1` (default) makes DCD follow the connection; with `&C0` DCD is
  always on. `&D2` (default) hangs up when DTR drops while connected; `&D0`
  ignores DTR.
- `ATI` returns `Casso modem` then `OK`. `A/` repeats the last command line.
- `ATO` with no connection returns `NO CARRIER`; `ATA` with nothing ringing
  returns `NO CARRIER`.
- While dialing, any character from the guest abandons the call with
  `NO CARRIER`, as on a Hayes. A dial that does not connect within S7 seconds
  (default 50) of emulated time returns `NO CARRIER`.
- `RING` repeats every 6 seconds of emulated time while a call waits. S1
  counts rings; with S0 above 0, the modem goes off hook when S1 equals S0.
- DSR is on and CTS is on while the modem attachment exists.
- The `+++` characters of a successful escape are sent to the remote end, as a
  Hayes sends them.
- S-registers 0-37 are accepted, values 0-255; anything else is `ERROR`.

## R9. The //c ports' default

**Decision**: An empty `device` on a //c serial port continues to mean the
loopback plug, which every //c has had since 016, and the //c list does not
include "Nothing". A port with an explicit device uses it.

**Rationale**: changing what an existing configuration does is outside 047.
This is listed as a question for the owner.

## R10. Settings

**Decision**: A `SerialAttachmentView` (one class pair) contains the
attachment drop-down and the chosen attachment's options. `SerialCardPage`
uses one for the Super Serial Card; a new `SerialPortsPage` uses two for the
//c (port 1, printer; port 2, modem) and is present only on a //c. The COM port
list is read through `IHostSerialPorts::EnumeratePorts` when the drop-down
opens (`DxuiComboBox::SetOnOpen`). Attachment changes rebuild the machine (the existing `RequiresReset`
rule, extended to the options and the //c ports). A rebuild behind live is
already a machine change, for which the shell offers to discard the history
ahead, so it is applied once the machine is live.

## R11. Reporting

**Decision**: Attachments post problems to `SerialLinkReports`, a thread-safe
list in `MachineHost`, each with the port's title. `ShellSerialCard` polls its
generation each UI tick and displays the newest in the Super Serial Card's
infobar, behind the card's firmware notices, cleared when the attachment
recovers. A COM port that returns is reopened, at most
every 2 seconds of emulated time.
