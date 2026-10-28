# Serial ports

The Super Serial Card (the ][, ][+, //e and Enhanced //e, in slot 2) and the
//c's two built-in ports each have a MOS 6551 ACIA/UART. Configure them in
Settings' **Serial card** tab for the SSC and **Serial ports** tab for a //c,
where port 1 is the printer port and port 2 the modem port.

| Attachment | What it does |
|---|---|
| None | Nothing on the connector. This is the default. |
| Loopback plug | TX to RX, RTS to CTS, DTR to DSR and DCD. |
| Write output to file | Everything sent is appended to a file. |
| Read input from file | A file is sent into the port at its baud rate once a program turns the receiver on. |
| COM port | A PC serial port, such as a USB serial adapter. |
| TCP connection | A TCP connection to a host and port, or one accepted on a listening port. |
| Modem | A Hayes-compatible modem that dials over TCP. |

Problems with an attachment, such as a COM port in use or a host that does not
answer, appear in the infobar at the top of the emulator window.

## COM port

The PC's COM port uses the baud rate, word length, parity, and stop bits that
the program running on the Apple sets in the 6551. DTR and RTS follow the
6551's. CTS, DSR and DCD from the device are passed to the 6551, and a change
of DSR or DCD raises its interrupt, as on the chip. Framing, parity and overrun
errors, and a break, are reflected in the 6551's status register.

- The COM port list is refreshed when it opens, so an adapter plugged in while
  Settings is already open is listed.
- A rate the PC port does not support is replaced by the closest one it has.
  When that rate is more than 2% off, the substitution is reported in an
  infobar; within 2%, each end of the line has room for the difference. The
  6551's 110 and 134.5 baud are 109.92 and 134.58, which a PC runs at 110 and
  135.
- Windows reports a line error without the byte it came with, so the error is
  attached to the last byte successfully read.
- An adapter unplugged while Casso is running turns DCD off, is reported via infobar, and is
  retried every two seconds.

## TCP connection

Choose **Connect to a host**, with the host and port, or **Listen for a
connection**, with the port. DCD and DSR are on while a connection is open.
After an outbound connection fails, or the other end closes it, Casso tries to reconnect
every five seconds.

With **Telnet** on, Casso manages the Telnet handshake and encodings/decodings ($FF -> $FF $FF; CR -> CR NUL). Echo and suppress-go-ahead are supported,
other options are not.

The 6551 receives at the baud rate the Apple program set and Casso buffers the receive stream.

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
| `ATX0` to `ATX4` | `CONNECT` alone (X0), or with the baud rate (X1 to X4; X4 is the default) |
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

Serial data received is stored in history, including errors, CTS, DSR, and DCD
changes, and connection open/close events. What the Apple sends is not stored:
replaying the program sends the same bytes at the same cycles, so the Serial
log lists them as it did live. Stepping back and seeking through a session
replays all of it, but nothing is sent to the COM port or connection while
history is behind live.

On a resume from a point in the past, the real connection stays open, so a call
to a BBS, for example, survives a rewind. The Apple receives again everything
the remote end sent after that point, then whatever it sends next. What the
Apple program sent after that point already went out, so if it sends it again,
the remote end gets it twice. Loading a machine's saved state closes any real
connection: DCD turns off and the modem returns `NO CARRIER`.

The debugger's **Serial log** lists the traffic of every attachment; see
[the debugger](Debugger.md#serial-and-network-logs).
