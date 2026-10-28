# Feature Specification: Serial Connectivity

**Feature Branch**: `047-serial-connectivity` (cut from `035-debugger`; merges to master after 035)

**Created**: 2026-10-27

**Status**: Draft

**Input**: Owner decision, 2026-10-27: the emulated serial ports cannot connect to a
real COM port or a TCP connection, and that is a bug. Connect the Super Serial
Card and the //c's two serial ports to a host COM port, to TCP, and to a
Hayes-compatible modem that dials over TCP. Everything a connection brings in
is stored in history, so rewinding and seeking replay it.

## Background

The Super Serial Card (042) and the //c's two built-in ports each have a 6551
ACIA. Today a port can attach only to a loopback plug, an output file, or a
fixed byte source. A program on the emulated Apple therefore cannot talk to a
real device on a PC serial port, to another emulator, or to a BBS on the
internet, all of which other Apple II emulators allow. AppleWin attaches its
Super Serial Card to a COM port or to TCP port 1977; MAME attaches a serial
port to a file or a raw socket; DOSBox, VICE and tcpser offer a Hayes modem
that dials Telnet hosts. Those are prior art for what users look for; no code
from them is used.

The work is built on 035's input journal and history, which store what comes
in from outside the machine and replay it on a rewind. That is why the branch
is cut from `035-debugger`.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Call a BBS from period software (Priority: P1)

A user runs a 1980s terminal program on the emulated Apple, with the Super
Serial Card attached to the modem. The program sends `ATDT bbs.example.org:23`;
the modem returns `CONNECT`, carrier detect comes on, and the BBS's screens
arrive. The user types `+++`, pauses, types `ATH`, and the modem returns
`NO CARRIER`.

**Why this priority**: Calling BBSes is what most period serial software was
written for, and an internet connection is all it takes.

**Independent Test**: Run a modem test against a local TCP server: dial it,
exchange text both ways, escape to command mode, hang up, and check every
result code and the carrier detect line.

**Acceptance Scenarios**:

1. **Given** the modem attachment and a reachable host, **When** the guest
   sends `ATDT host:port`, **Then** the modem returns `CONNECT` and carrier
   detect turns on.
2. **Given** a connection, **When** the guest sends text containing `+++`
   between other characters, **Then** all of it goes to the remote end and the
   modem stays in data mode.
3. **Given** a connection, **When** the guest is silent for the guard time,
   sends `+++` and is silent for the guard time again, **Then** the modem
   returns `OK` in command mode with the connection open.
4. **Given** command mode with a connection, **When** the guest sends `ATH`,
   **Then** the connection closes, carrier detect turns off and the modem
   returns `OK`.
5. **Given** the remote end closes the connection, **Then** the modem returns
   `NO CARRIER` and carrier detect turns off.
6. **Given** a host that closes the connection at once or is offline, **When** the guest
   dials it, **Then** the modem returns `NO CARRIER`.

---

### User Story 2 - Use a real serial device (Priority: P1)

A user attaches a port to a PC COM port, with a USB serial adapter wired to a
real Apple, a printer, or a terminal. The guest's baud rate and framing are
set on the PC's port as the guest sets them, the handshake lines pass both
ways, and line errors arrive in the 6551 as on real hardware.

**Why this priority**: Talking to real hardware is the other main use of a
serial port, and it is the only way to see real line behavior, including
errors from a mismatched device.

**Independent Test**: With two PC ports wired to each other by a null modem
cable, attach the emulated port to one and a test program to the other. Check
the settings, both directions of data, each handshake line, and a framing
error from a deliberately mismatched baud rate.

**Acceptance Scenarios**:

1. **Given** a COM port attachment, **When** the guest writes the 6551's
   control and command registers, **Then** the PC port runs at the same baud
   rate, word length, parity and stop bits.
2. **Given** the guest turns DTR or RTS on or off, **Then** the PC port's line
   follows.
3. **Given** the device at the far end changes CTS, DSR or DCD, **Then** the
   6551 status register and the guest's interrupt follow it.
4. **Given** the far end runs at a different baud rate, **Then** the guest
   receives bytes with framing or parity errors in the status register, as on
   real hardware.
5. **Given** the USB adapter is unplugged, **Then** an infobar reports it and
   carrier detect turns off in the 6551.

---

### User Story 3 - Connect over TCP (Priority: P1)

A user attaches a port to TCP, either connecting out to a host and port or
listening on a port for a connection in. Another emulator, a script, or a
Telnet client on the PC then talks to the emulated Apple.

**Why this priority**: TCP is the simplest way to link two emulators or to
drive the Apple from a script, and it is the transport the modem uses.

**Independent Test**: Attach a port to listen on a local port, connect a test
client, exchange bytes both ways including $FF, and close from each side.

**Acceptance Scenarios**:

1. **Given** a listening attachment, **When** a client connects, **Then** DCD
   and DSR turn on and the 6551 raises the line change.
2. **Given** a connection, **Then** bytes pass both ways unaltered, $FF
   included, unless the Telnet option is on.
3. **Given** the Telnet option, **Then** $FF in the data is doubled on the
   wire, a doubled $FF from the wire arrives as one byte, and Telnet commands
   from the remote end are replied to and are not passed to the guest.
4. **Given** the client disconnects, **Then** DCD and DSR turn off.
5. **Given** an outgoing attachment whose host is offline, **Then** an
   infobar reports it and DCD stays off.

---

### User Story 4 - Rewind through a serial session (Priority: P2)

A user debugging a terminal program rewinds through a session with a BBS or a
device. The replay delivers the same bytes and line changes at the same moments,
and nothing is sent to the real port or connection. On a resume from a point in
the past the real connection stays open, so a call to a BBS survives a rewind;
loading a state file closes it.

**Why this priority**: History and the debugger's serial log are only useful
for serial work if the traffic replays exactly.

**Independent Test**: Record a session over a local TCP server, step back and
seek through it, and compare the machine state and the serial log with the
live run; confirm the server received nothing during the replay.

**Acceptance Scenarios**:

1. **Given** a stored session, **When** the user seeks back and forward,
   **Then** the guest receives every byte, line change and error at the same
   cycle as live.
2. **Given** a replay, **Then** the real port or connection receives nothing.
3. **Given** a connection open live, **When** the user resumes from a point
   in the past, **Then** the real connection is open, and the 6551's carrier
   detect matches it at the next poll.
4. **Given** a connection open, **When** the user loads a state file, **Then**
   carrier detect turns off in the 6551, the modem returns `NO CARRIER`, and
   the real connection is closed.

---

### User Story 5 - Choose a port's attachment (Priority: P2)

A user picks each port's attachment and its options in the machine's settings:
for each Super Serial Card slot and each //c port, one of loopback, file,
COM port, TCP or modem.

**Why this priority**: The attachments are reachable only through settings.

**Independent Test**: Set each attachment in turn, restart the machine, and
check that the choice and its options are applied after the restart.

**Acceptance Scenarios**:

1. **Given** the settings, **Then** each port lists the attachments and the
   options of the chosen one: COM port name; host and port, or listen port;
   the Telnet option; the modem's listen port.
2. **Given** a COM port list, **Then** it contains the ports present on the PC
   when the list is opened.
3. **Given** a changed attachment, **When** the settings are applied, **Then**
   the old connection closes and the new one opens.

### Edge Cases

- Two ports set to the same COM port or the same listen port: the second one
  reports an infobar and stays disconnected.
- A guest sets a baud rate the PC port does not support: the nearest
  supported rate is used and an infobar reports the substitution once.
- The 6551's undocumented 16x external clock setting with a COM port: the
  baud rate is the one the card's crystal produces for the guest's setting.
- A modem command line longer than the command buffer: the modem returns
  `ERROR`.
- `+++` sent with S12 set to 0, or with S2 above 127: the characters are data.
- An incoming connection while the modem is dialing out: closed at once.
- A host name that does not resolve: `NO CARRIER` for the modem, an infobar
  for TCP.
- Emulation running faster or slower than real time: the guard time and the
  6551's pacing follow emulated time.
- A port's attachment changed while history is behind live: applied when the
  machine returns to live.

## Requirements *(mandatory)*

### Functional Requirements

**COM port attachment**

- **FR-001**: A serial port MUST attach to a host COM port.
- **FR-002**: The COM port MUST run at the baud rate, word length, parity and
  stop bits the guest sets in the 6551's control and command registers, and
  MUST change when the guest changes them.
- **FR-003**: The guest's DTR and RTS MUST drive the COM port's DTR and RTS.
- **FR-004**: The COM port's CTS, DSR and DCD MUST be passed to the 6551. A
  change MUST set the 6551's status bits and raise its interrupt as a change
  of its input lines does.
- **FR-005**: Framing, parity and overrun errors and a break on the COM port
  MUST set the 6551's status register with the byte they arrived with.

**TCP attachment**

- **FR-006**: A serial port MUST attach to TCP, either connecting to a host
  and port or listening on a port for one connection at a time.
- **FR-007**: DCD and DSR MUST be on while a connection is open and off
  otherwise. Each transition MUST raise the 6551's line change.
- **FR-008**: Without the Telnet option bytes MUST pass unaltered both ways.
  With it, $FF in the data MUST be doubled on the wire, a doubled $FF from the
  wire MUST arrive as one byte, and Telnet commands from the remote end MUST
  be replied to and MUST NOT be passed to the guest.
- **FR-009**: The 6551 MUST pace bytes from TCP at the baud rate and framing
  the guest set.

**Modem attachment**

- **FR-010**: A serial port MUST attach to a Hayes-compatible modem that
  connects over TCP.
- **FR-011**: The modem MUST accept the AT command set: `AT`, `ATD` with `T`
  or `P` and a `host:port` (port 23 when none is given), `ATA`, `ATH`, `ATO`,
  `ATZ`, `AT&F`, `ATE`, `ATQ`, `ATV`, `ATS`n`=`v and `ATS`n`?`, and answer
  `OK`, `CONNECT`, `RING`, `NO CARRIER` and `ERROR`, as words or as numbers
  per `ATV`.
- **FR-012**: The modem MUST listen on a chosen port; an incoming connection
  MUST produce `RING`, and `ATA`, or S0 rings when S0 is set, MUST answer it.
- **FR-013**: DCD MUST follow the connection.
- **FR-014**: The escape MUST require silence for the guard time (S12, in
  fiftieths of a second, default 50), then the escape character (S2, default
  `+`) three times with each within the guard time of the one before, then
  silence for the guard time. Any other timing MUST pass the characters as
  data.
- **FR-015**: S2 above 127 or S12 set to 0 MUST turn the escape off.
- **FR-016**: The guard time MUST be measured in emulated time.

**History and replay**

- **FR-017**: Everything an attachment brings in (bytes, line changes, error
  flags, and connections opening and closing) MUST be stored in history at
  the cycle the machine takes it in.
- **FR-018**: A replay MUST deliver the stored input at the same cycles and
  MUST send nothing to a COM port or connection.
- **FR-019**: On a resume from a point in the past the real connection MUST
  stay open, and the guest's CTS, DSR and DCD MUST match the attachment at the next
  poll. The bytes the 6551 took from the attachment after the point MUST be
  received again, in order and each with its receive errors. Loading a state file with a connection open MUST close the real
  connection and turn DCD off for the guest; the modem MUST answer
  `NO CARRIER`.
- **FR-020**: 035's serial log MUST list the traffic of every attachment.

**Settings and failures**

- **FR-021**: Each Super Serial Card slot and each //c port MUST have an
  attachment and options in the machine's settings, saved across launches.
- **FR-022**: A COM port that disappears, an offline host, and a
  port already in use MUST be reported in an infobar, and the guest MUST see
  DCD off.
- **FR-023**: Changing a port's attachment MUST close the old connection
  before opening the new one.

### Key Entities

- **Attachment**: what a serial port connects to: loopback, file, byte
  source, COM port, TCP or modem, and its options.
- **Line state**: the levels of DTR, RTS, CTS, DSR, DCD and RI at a moment.
- **Modem state**: command or data mode, the S-registers, the connection, and
  the escape timer.
- **Stored input**: one item of what an attachment brought in, at a cycle:
  a byte, a line change, an error flag, or a connection event.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A period terminal program connects to a Telnet BBS through the
  modem, exchanges text, escapes, and hangs up, with every result code
  correct.
- **SC-002**: Over a null modem cable between two PC ports, data passes both
  ways at every baud rate the 6551 offers up to 19,200, with every byte delivered.
- **SC-003**: A deliberate baud rate mismatch on a COM port produces framing
  errors in the guest's status register.
- **SC-004**: Text containing `+++` sent in one burst never switches the modem out of data mode,
  over 1,000 trials with the escape in different positions.
- **SC-005**: A replay of a stored session matches the live run byte for
  byte and cycle for cycle, and sends nothing outside the machine.
- **SC-006**: Attaching to TCP or the modem costs nothing measurable in
  emulation speed while the line is idle.

## Assumptions

- The PC's serial ports are accessed through the operating system's serial
  port interface, which returns bytes, line levels and error flags, not the
  raw line, so the debugger's bit-level graph is drawn from the bytes and
  settings in force.
- One connection at a time per port; a listening port closes a second
  client's connection at once while one is connected.
- The modem has no audible dial or connect tones.
- No authentication or encryption on TCP; SSH is out of scope.
- Out of scope: XMODEM and other file transfer protocols in the emulator,
  serial over USB device emulation, printing over serial (042 writes a file),
  and the Uthernet II.
- The work merges whole, in one merge, after 035.
