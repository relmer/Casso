# Contract: the Apple II link

Spec FR-083 to FR-098; research R15 to R23. This is the wire contract
between Casso and an Apple-side client, and between Casso Explorer and the
link's session. Values marked *captured* are settled from traffic between
ADTPro 2.1.0's own host and client (R15) before the codec is written;
until then they are open in this contract, not guessed.

## Framing (ADTPro's, unchanged)

| Field | Bytes | Value |
|---|---|---|
| Start | 1 | `'A'` ($C1) |
| Payload length | 2 | LSB first (*captured*) |
| Command | 1 | below |
| Envelope check | 1 | EOR of the four bytes before it |
| Payload | n | per command |
| Payload check | 1 | EOR of the payload; absent when n is 0 |

ADTPro's data travels in Send Image Data ($D3): a two-byte start (LSB,
MSB), the page data RLE-encoded (*captured*), and a two-byte CRC over the
page data, LSB first (algorithm *captured*). Casso's client uses Sector
data (below) instead: unencoded, with CRC-16/XMODEM (polynomial $1021,
initial value 0), so it depends on nothing captured. The receiver of either
replies with Acknowledgement ($CB): ACK $06, or NAK $15 to have it sent
again.

A side that receives a damaged envelope or packet sends NAK. The sender
resends up to the retry limit (*captured*, default 5). When no byte arrives
within the timeout (*captured*, default 3 s), the link closes and the
operation fails (FR-087).

## ADTPro's client (served unchanged, FR-088)

Casso serves CD, Directory, Size, Get, Put, Batch, Multiple, Summary,
Home and Send File as ADTPro's host does, and serves virtual drives
(`'E'`, $C5) over 512-byte blocks. Image files are those in the folder open
in Casso Explorer's current tab, or the folder an open image is in; with
none, each request fails with ADTPro's "unable" status. Nibble ($CD) is
met with ADTPro's abort reply; raw tracks are the protected-disk follow-on's.

## Casso's client (extended)

Every command below is outside ADTPro's set, in the same framing. The
client sends first; the host's reply is the next operation.

| Command | Byte | From | Payload |
|---|---|---|---|
| Ready | $D2 `'R'` | client | program version (2), shim version (1), drive map (1: bit per slot 6 drive 1/2, //c internal and external) |
| Idle | $C9 `'I'` | host | none; client sends Ready again after 100 ms |
| Sector data | $CA `'J'` | either | track (1), sector (1), 256 bytes unencoded, CRC-16/XMODEM of the 256 bytes (2, LSB first) |
| Load program | $CF `'O'` | host | page count (1); then Sector data packets, one per page from $0800, with the page number in the track and sector fields; client jumps to it |
| Set speed | $D7 `'W'` | host | 6551 control byte (1); both ends switch after the ACK |
| Read sectors | $D5 `'U'` | host | drive (1), track (1), count (1), sector list (count); client replies with Sector data per sector |
| Write sectors | $D6 `'V'` | host | drive (1), track (1), count (1), sector list (count); host then sends Sector data per sector; client ACKs each after writing and reading it back |
| Format track | $C8 `'H'` | host | drive (1), track (1), volume (1) |
| Status | $D4 `'T'` | client | operation (1), drive (1), track (1), percent (1), error (1) |
| Cancel | $D1 `'Q'` | host | none; client stops, replies Ready (FR-098) |

The host distinguishes clients by the first command it receives: Ready is
Casso's client; any command in ADTPro's set is ADTPro's client (R16).

## Bootstrap (R23)

At 9,600 baud, 250 ms after each line, into the //c's monitor through
`IN#2`: `CALL -151`, the shim as `0300:` lines of 8 bytes, `300G`. The shim
then sends Ready at 9,600 baud and receives Load program.

## Session interface (Casso Explorer side)

`AppleLinkSession` owns an `IByteStream`, the protocol engine, a
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
