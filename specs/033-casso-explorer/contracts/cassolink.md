# Contract: CassoLink

Spec FR-083 to FR-098; research R15 to R23. This is the wire contract
between Casso and an Apple-side client, and between Casso Explorer and the
link's session. The values once marked *captured* were settled on 2026-10-15
from traffic between ADTPro 2.1.0's own host and client (R15); the
captures are in `UnitTest/Fixtures/CassoLink/`.

## Framing (ADTPro's, unchanged)

| Field | Bytes | Value |
|---|---|---|
| Start | 1 | `'A'` ($C1) |
| Payload length | 2 | LSB first |
| Command | 1 | below |
| Envelope check | 1 | EOR of the four bytes before it |
| Payload | n | per command |
| Payload check | 1 | EOR of the payload; absent when n is 0 |

ADTPro's data travels in Send Image Data ($D3). Its length field is the
length of the data after decoding (1,024 for two blocks), not counting the
start block or the CRC. The payload is a two-byte starting block (LSB, MSB)
and the data encoded page by page; a CRC-16/XMODEM (polynomial $1021,
initial value 0) of the decoded data, LSB first, takes the place of the
payload check. In the encoding, each byte is the difference from the
previous decoded byte, modulo 256, starting from 0 in each packet. A 0 is
followed by an offset within the current 256-byte page, and the previous
byte repeats up to that offset, 0 meaning the end of the page. Casso's
client uses Sector data (below) instead: unencoded, with the same CRC.

Replies differ by direction. A host receiving a packet (Put, Batch)
replies with one unframed byte, ACK $06 or NAK $15. A client receiving one
(Get) replies with Acknowledgement ($CB), whose payload is ACK or NAK and
the two-byte number of the next block to send.

A side that receives a damaged data packet sends NAK. A damaged command
envelope has no reply, as from ADTPro's host: the client sends the command
again once its wait for a reply runs out. Casso resends up to five times
(FR-087). ADTPro's own peers, as captured:

- Its host resends a packet the client NAKs after 0.26 s, then after
  0.52 s, and stops after the third NAK. When the client next sends, the
  host ends the transfer with Home ($D8), with a check byte of $13 rather
  than the EOR, $19; its client accepts it.
- Its client waits about 2 s (2.03 to 2.09 million cycles) for a reply
  before sending again. When the host's ACKs are dropped it sends again with
  no limit, under the same starting block but with the data of later
  blocks, so a host ACKs a starting block it has already accepted and
  discards that packet's data.

When no byte arrives within 3 s, longer than the ADTPro client's 2 s so
that its resends arrive first, the link closes and the operation fails
(FR-087).

## ADTPro's client (served unchanged, FR-088)

Casso serves CD, Directory, Size, Get, Put, Batch, Nibble ($CE), Multiple
nibble ($CD) and Summary as ADTPro's host does, and serves virtual drives
(`'E'`, $C5) over 512-byte blocks, on an ADTPro-compatible server
connection only (R28). Image files are those in the folder the connection
shares, and a path outside that folder's subtree, by `..`, a drive, a UNC
path or a junction, has ADTPro's error reply. Home has no reply, as from
ADTPro's host. Send File and ADTPro's older commands are not served.

- **Directory** ($C4): the payload is an optional filespec (`*` and `?`,
  without regard to case) ending in a 0, then the page number as its last
  byte. A page has a header of at most two 40-column lines,
  `DIRECTORY OF: ` and the path when the path has at most 66 characters,
  keeping the end of a longer one; a return ends a header shorter than 80
  characters and a second follows one of at most 40, so the names start on
  the screen's fourth row. 40 dashes follow, then up to 18 names, each
  ending in a return unless it fills its line; a name longer than 40 is cut
  to 38 characters and `..`. An empty folder lists `NO FILES`. The text
  ends in a 0, and the byte after it is 1 when another page follows.
- **File chooser token**: in place of a file name, Size and Get accept
  the client's token for a name picked from the last listing: $01, the page
  counted from 1, and the screen row, the first name being on row 3. It
  resolves to name 18 × (page − 1) + (row − 3) of that listing, after the
  filespec.
- **Received files**: Put adds `.dsk` to a 140 KB image unless it ends in `.dsk` or
  `.do`, and `.po` to any other unless it ends in `.po` or `.hdv`. Each image a
  Batch receives is the prefix and the next free 4-digit number from 0001,
  with `.dsk` for 140 KB and `.po` otherwise.
- **Nibble send**: the reply to the command is a status byte, then 35
  tracks, each a 13,312-byte capture sent as Image data packets, each
  packet ACKed, and then an ACK for the track, which the client waits for
  before reading the next. The Summary ($D9) follows the last track, as it
  follows every Put and Batch. The image is written as a `.nib` of 6,656
  bytes a track: one revolution from the track's first address field,
  padded with $FF, or the capture's first 6,656 bytes for a track with no
  address field.
- **No keepalive**: an idle ADTPro client sends nothing, and before each
  byte it waits for DSR, so any ADTPro-compatible cable connects the
  handshake line that Casso monitors to detect a machine dropping.

The virtual-drive packets are unframed. ADTPro's VSDRIVE driver reads with
Read3 ($03 for drive 1, $05 for drive 2), whose reply has the host's ProDOS
date and time ahead of the block, and writes with $02 or $04. Each header's
check is the EOR of the bytes before it in the header, except in the reply
to a write, which ends with the written block's check; and each block's
check is the EOR of its 512 bytes alone. The driver's first request at
startup can arrive twice before the host's first reply.

## Casso's client (extended)

Every command below is outside ADTPro's set, in the same framing. The
client sends first; the host's reply is the next operation.

| Command | Byte | From | Payload |
|---|---|---|---|
| Ready | $D2 `'R'` | client | program version (2), shim version (1), drive map (1: bit per slot 6 drive 1/2, //c internal and external); the program adds the ROM's identification bytes $FBB3, $FBC0, $FBBF and $FB1E, and 1 for a IIgs (5), from which the host has the model |
| Idle | $C9 `'I'` | host | none; client sends Ready again after 100 ms |
| Sector data | $CA `'J'` | either | track (1), sector (1), 256 bytes unencoded, CRC-16/XMODEM of the 256 bytes (2, LSB first) |
| Load program | $CF `'O'` | host | page count (1); then Sector data packets, one per page from $0800, with the page number in the track and sector fields, each followed by the shim's bare ACK or NAK byte; the shim jumps to $0800 after the last. Sent to a running program, it returns that program to the shim, whose Ready follows |
| Set speed | $D7 `'W'` | host | 6551 control byte (1); the client acknowledges ($CB, ACK, 0, 0) at the old speed, and both ends then switch |
| Read sectors | $D5 `'U'` | host | drive (1), track (1), count (1), sector list (count), progress (2); client replies with Sector data per sector |
| Write sectors | $D6 `'V'` | host | drive (1), track (1), count (1), sector list (count), progress (2); for each sector the client sends Acknowledge ($CB: ACK, sector, track) with the number of the sector to send, or NAK for another copy of a damaged one, and the host sends its Sector data; the client writes it and reads it back before the next Acknowledge |
| Format track | $C8 `'H'` | host | drive (1), track (1), volume (1), progress (2) |
| Write track | $DB `'[']` | host | drive (1), track (1), volume (1), progress (2); the client requests each of the 16 sectors in physical order as for Write sectors, then formats the track with their data in one revolution, reads it back and compares it; reported as a write |
| (progress) | | | the part of the user's whole operation a request covers, as its starting and ending percentage (0-100), for the program's status row |
| Status | $D4 `'T'` | client | operation (1: 1 read, 2 write, 3 format), drive (1), track (1), sectors done (1), error (1); ends every Read, Write and Format |
| Label | $DC `'\'` | host | up to 38 ASCII characters, the user's operation; the program prints it and sends Ready (R24) |
| Cancel | $D1 `'Q'` | host | none; client stops, replies Ready (FR-098). During a read, any byte from the host is taken as Cancel, as the 6551 has room for only the first while the drive is busy |

The host distinguishes clients by the first command it receives: Ready is
Casso's client; any command in ADTPro's set is ADTPro's client (R16).

Status error codes: $00 none, $10 write-protected, $20 no address field
for the sector, $30 the data field did not decode, $40 the sector read back
differently, $50 the host's data did not arrive whole, $60 no such drive.
A sector is the physical sector, the number in its address field.

The shim occupies $3D00 to $3FFF, with the 6551 control byte for its starting speed at $3D02, the speed's text at $3D06, a Wi-Fi modem's dial command, ending in a zero byte, at $3D0C (R24), and the 6551's slot at $3D38: 2 for the //c's port 2, or a Super Serial Card's slot. The program
runs from $0800 and uses $4000 to $92FF for buffers, with no DOS or ProDOS
in memory.

## Sequences

Each diagram has the Apple on the left and the PC on the right, with time
running down. The Apple always speaks first; every reply from the PC is the
answer to the Apple's last packet.

### Connecting, and loading the program

The shim's Ready has program version 0, so the PC sends the program a page
at a time; a page whose CRC fails is sent again after a NAK. The program's
own Ready has its version, and the PC replies with Idle.

```mermaid
sequenceDiagram
    participant A as Apple (shim, then program)
    participant P as PC (CassoLinkHost)
    A->>P: Ready (version 0, shim version, drives)
    P->>A: Load program (page count)
    loop each page from $0800
        P->>A: Sector data (page, 256 bytes, CRC)
        A-->>P: ACK, or NAK for the page again
    end
    Note over A: jumps to $0800, reads the ROM's ID bytes
    A->>P: Ready (program version, drives, ROM ID)
    P->>A: Set speed (6551 control byte)
    A->>P: Acknowledge (ACK), at the old speed
    Note over A,P: both ends switch speed
    A->>P: Ready
    P->>A: Idle
```

### Idle, and a dropped Apple

While nothing is queued the Apple sends Ready about every 100 ms, and each
Idle clears its label row. Three seconds with no byte, or DSR dropping on
a serial cable wired to the Apple's DTR, is the Apple gone: a reset, a
power-off or a pulled cable. The PC closes the line and opens it again
every five seconds.

```mermaid
sequenceDiagram
    participant A as Apple (program)
    participant P as PC
    loop while nothing is queued
        A->>P: Ready
        P->>A: Idle
        Note over A: waits 100 ms, clears the label row
    end
    Note over A: reset, or turned off
    Note over P: 3 s with no byte, or DSR off
    Note over P: Disconnected; the line opens again after 5 s
```

### Reading a track

The label goes first, then one request per track. The Apple reads the
requested sectors as they pass under the head, then sends each in the
order requested, and ends with Status.

```mermaid
sequenceDiagram
    participant A as Apple (program)
    participant P as PC
    A->>P: Ready
    P->>A: Label ("SENDING D1 (/GAMES) TO PC")
    A->>P: Ready
    P->>A: Read sectors (drive, track, count, sectors, progress)
    Note over A: seeks, reads the sectors as they pass
    loop each requested sector
        A->>P: Sector data (track, sector, 256 bytes, CRC)
    end
    A->>P: Status (read, drive, track, sectors done, error)
    A->>P: Ready
    P->>A: Read sectors for the next track, or Idle
```

### Writing a whole track

Each sector is requested by an Acknowledge with its number, and encoded
as it arrives; the track is then written in one revolution and read back.

```mermaid
sequenceDiagram
    participant A as Apple (program)
    participant P as PC
    A->>P: Ready
    P->>A: Write track (drive, track, volume, progress)
    loop sectors 0 to 15
        A->>P: Acknowledge (ACK, sector, track)
        P->>A: Sector data (track, sector, 256 bytes, CRC)
        Note over A: a bad CRC is requested again with NAK
    end
    Note over A: formats the track with the data, reads it back, compares
    A->>P: Status (write, drive, track, 16, error)
```

### Canceling

```mermaid
sequenceDiagram
    participant A as Apple (program)
    participant P as PC
    P->>A: Read sectors
    A->>P: Sector data
    P->>A: Cancel (any byte, during a read)
    Note over A: stops where it is
    A->>P: Ready
```

### ADTPro's client

ADTPro's client also speaks first, with one of its own commands rather than
Ready; the PC serves it in ADTPro's protocol until the line drops, and
then waits to see which client is there next.

```mermaid
sequenceDiagram
    participant A as Apple (ADTPro)
    participant P as PC
    A->>P: an ADTPro command (DIR, CD, GET, PUT, ...)
    Note over P: ADTPro mode until the line drops
    P->>A: ADTPro's reply
```

## Bootstrap (R23)

At 9,600 baud, 250 ms after each line, into the //c's monitor through
`IN#2`: `CALL -151`, the shim as `3D00:` lines of 8 bytes, `3D00G`. The shim
then sends Ready at 9,600 baud and receives Load program. A shim on a boot
disk starts at the link's speed instead. On a Super Serial Card the user
types `IN#` and the card's slot, and Ctrl-A `14B` then Return; the card's firmware executes the command at the
Return.

On a ][+ or //e, the cassette bootstrap is the shim as one monitor tape
record, sent as audio into the cassette input after `CALL -151` and
`3D00.3FFFR 3D00G` are typed: a 770 Hz leader of 6 seconds, the sync
half-cycles of 200 and 250 microseconds, the bytes most significant bit
first, a 0 as one 2 kHz cycle and a 1 as one 1 kHz cycle, and the XOR
checksum seeded with $FF. The shim starts at the link's speed.

The shim's slot (R29): the header's when the boot disk's stage started it;
else the input hook's high byte at $39 when it is $C1 to $C7, as `IN#n`
left it; else the first slot with the Super Serial Card's identification
bytes. The 6551 is at $C088 plus 16 times the slot.

The shim's screen: a title, then `WAITING FOR THE PC ON PORT 2 AT <speed>`
on a //c or `WAITING FOR THE PC IN SLOT <n> AT <speed>` on a card,
`LOADING PROGRAM <n>%` and `STARTING VERSION <n>`. The program adds a PC line,
`CONNECTED` or `NO REPLY FOR <n> S`, sending Ready again after each second
with no reply; an error line with the last Status error until an operation
succeeds; and the last Label.

## The universal boot disk

Casso Explorer's boot disk works on every interface, as ADTPro's does. It
loads a first stage at $2000, with the 6551 shim at $2C00, built for $3D00,
and the W5100 shim at $2F00, built for $3B00. The host writes both shims'
headers as for their own disks.

The stage finds:

- on a //c, ports 1 and 2;
- a Super Serial Card in any slot, by $Cn05 = $38, $Cn07 = $18, $Cn0B = $01
  and $Cn0C = $31;
- an Uthernet II in any other slot but a Disk II's, by its mode register
  at $C084 plus 16 times the slot reading with bits 6, 5 and 2 clear, and
  then the W5100 taking two address pointer values and returning them.

The header's MAC address has the sum of RNDL and RNDH ($4E, $4F) and a
16-bit sum of $4000-$BFFF added into its last three bytes before the
W5100 is given it (R31).

Each 6551 is set to the 6551 shim's speed with DTR on. The W5100 is reset,
given the header's MAC address, 8 KB of receive memory and 4 KB of transmit
memory to socket 0 and 4 KB of transmit memory to socket 1, and an address
by DHCP: DISCOVER, OFFER, REQUEST and ACK from port 68 to port 67 broadcast,
the transaction ID the MAC address's last four bytes, three attempts of about
three seconds each. With no ACK the header's address is used. Socket 0 then
listens on the header's port, and socket 1 broadcasts an announcement to UDP
port 6510 from port 6511 about every two seconds:

| Bytes | Field |
|---|---|
| 9 | `CASSOLINK` in ASCII |
| 1 | version, 1 |
| 4 | the W5100's address |
| 2 | its TCP port, high byte first |
| 6 | its MAC address |

The screen: the title, `WAITING FOR THE PC ON:`, then a row for each
interface: `PORT <n> AT <speed>` on a //c, `SLOT <n> AT <speed>` for a card,
and `SLOT <n> AT <address>:<port>` for the Uthernet II.

The stage waits on all of them. A $C1 byte, an envelope's start, on a 6551,
or a connection to the W5100 chooses that interface: the stage copies the
machine's zero page and reset vector into the chosen shim, which it saved
before using any, sets the shim's zpsaved byte and slot, copies the shim to
its address and starts it. Started this way, a shim starts as one from its
own disk does, except that it saves nothing. A 6551 shim with a dial
command is started at once on the first 6551, since no connection from the
PC is possible until the modem dials it.

Casso Explorer listens on UDP port 6510 on every interface, with the port
bound for reuse, and lists the Apple of each announcement, by its MAC
address, as an Uthernet II link at the announcement's address. An
announcement from this PC is Casso's emulated card, whose listening socket
is on 127.0.0.1, and is listed at 127.0.0.1.

## Session interface (Casso Explorer side)

`CassoLinkSession` owns an `IByteStream`, the protocol engine, a
`LinkedDisk` per drive and a queue of `LinkOperation`s. Casso Explorer
calls only:

| Call | Effect |
|---|---|
| `Start (settings)` | opens the stream (FR-097), waits for Ready, loads the program if versions differ |
| `ListDrives` | the drive map from Ready, with each volume's name once read |
| `Enumerate (drive, inner)` | a catalog over the drive's `LinkedDisk`, faulting sectors in (R20) |
| `ReadFile` / `WriteFile` / `Delete` / `Rename` / `MakeDirectory` | the volume's own operation over the `LinkedDisk`; writes send only changed sectors |
| `ReadDisk (drive, format, path)` | every track, then the image written atomically (FR-086) |
| `WriteDisk (drive, path)` | every track formatted, written and read back |
| `Cancel` | the running operation stops; queued ones remain |
| `Stop` | the stream is closed and the port released |

Progress and results are posted to the window; no call waits on the line.
