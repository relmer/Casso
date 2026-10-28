# Serial ports

The Super Serial Card (the ][, ][+, //e and Enhanced //e, in slot 2) and the
//c's two built-in ports each have a 6551. Whatever is plugged into a port's
serial connector is chosen in Settings: **Serial card** for the card, and
**Serial ports** on a //c, where port 1 is the printer port and port 2 the
modem port. A change takes effect when you press OK, which restarts the machine.

| Attachment | What it does |
|---|---|
| None | Nothing on the connector (the card only). |
| Loopback plug | TX to RX, RTS to CTS, DTR to DSR and DCD. A //c port has this by default. |
| Write output to file | Everything sent is appended to a file. |
| Read input from file | A file is sent into the port at its baud rate once a program turns the receiver on. |
| COM port | A PC serial port, such as a USB serial adapter. |
| TCP connection | A TCP connection to a host and port, or one accepted on a listening port. |
| Modem | A Hayes-compatible modem that dials over TCP. |

Problems with an attachment, such as a COM port in use or a host that does not
answer, appear in the infobar at the top of the emulator window.

## COM port

The PC's port runs at the baud rate, word length, parity and stop bits the
program sets in the 6551, and changes when the program changes them. DTR and
RTS follow the 6551's. CTS, DSR and DCD from the device are passed to the 6551,
which raises its interrupt as for any change of its input lines. Framing, parity and overrun
errors and a break arrive in the 6551's status register.

- The COM port list is read each time it opens, so an adapter plugged in after
  Settings opened is listed.
- A PC port has one rate for both directions. When a program sets the 6551's
  receiver to its external clock, the port runs at the transmit rate.
- A rate the PC port does not support is replaced by the nearest one it does,
  and the infobar reports the replacement once per rate.
- Windows reports a line error without the byte it came with, so the error is
  attached to the last byte read at that moment.
- An adapter unplugged while Casso runs turns DCD off, is reported, and is
  opened again every two seconds until it is back.

## TCP connection

Choose **Connect to a host**, with the host and port, or **Listen for a
connection**, with the port. A listening port takes one connection at a time
and closes a second at once. DCD and DSR are on while a connection is open.
After a connection out fails, or the other end closes it, Casso connects again
every five seconds.

Bytes pass both ways as they are. With **Telnet** on, $FF in the data is
doubled on the wire, a doubled $FF from the wire arrives as one byte, CR is
sent as CR NUL, and Casso replies to the other end's Telnet negotiation itself,
without passing it to the program: echo and suppress-go-ahead are agreed to,
every other option declined.

The 6551 receives at the baud rate the program set; what it has not yet taken
waits in the PC's socket buffer.

## Modem

A Hayes-compatible modem that dials over TCP, for terminal programs and BBSes.
Dial with `ATDT host:port`; with no port, the modem dials port 23. **Telnet**
is on by default, since most BBSes use it. With a **listen port**, a call to
that port rings (`RING`), and `ATA` picks up, or with S0 set the modem picks up
after S0 rings.

| Command | Effect |
|---|---|
| `AT` | `OK` |
| `ATD`, `ATDT`, `ATDP` *host[:port]* | Dial; `CONNECT` or `NO CARRIER` |
| `ATA` | Answer a ringing call |
| `ATH` | Hang up |
| `ATO` | Back to data mode after an escape |
| `ATZ`, `AT&F` | Defaults (`ATZ` hangs up as well) |
| `ATE0`/`ATE1` | Echo off/on |
| `ATQ0`/`ATQ1` | Results on/off |
| `ATV0`/`ATV1` | Results as numbers/words |
| `ATX0` to `ATX4` | `CONNECT` alone, or with the baud rate |
| `AT&C0`/`AT&C1` | DCD always on/follows the call |
| `AT&D0`/`AT&D2` | Ignore DTR/hang up when DTR drops |
| `ATS`*n*`=`*v*, `ATS`*n*`?` | Set or read S-register *n* |
| `ATI` | `Casso modem` |
| `A/` | Repeat the last command |

`B`, `L`, `M`, `N`, `W`, `Y`, `&K`, `&S` and `&W` are accepted and ignored, so
initialization strings written for real modems work. A command line longer than
40 characters is `ERROR`.

The escape to command mode is the Hayes one: a second of silence, `+++`, and a
second of silence, all in emulated time, so it works at any emulation speed.
`+++` inside other text is sent as data. S2 sets the escape character and S12
the guard time in fiftieths of a second; S2 above 127 or S12 of 0 turns the
escape off. The modem has no dial or connect tones.

## History

Everything an attachment brings in is stored in history: each byte with its
errors, each change of CTS, DSR and DCD, and each connection opening and
closing. Stepping back and seeking through a session replays it exactly, and
nothing is sent to the COM port or connection while history is behind live.

Resuming from a point in the past closes any real connection, as a line would
drop once the other end has moved on: DCD turns off and the modem returns
`NO CARRIER`. A change to an attachment made while behind live is applied once
the machine is live again.

The debugger's **Serial log** lists the traffic of every attachment; see
[the debugger](Debugger.md#serial-and-network-logs).
