# Contract: W5100 Guest Interface

The interface presented to Apple II software. Values from the W5100 datasheet v1.1.8 and the Uthernet II manual.

## Slot I/O (`n = slot + 8`)

| Address | Read | Write |
|---|---|---|
| `$C0n0`-`$C0n3` | not decoded (empty-slot behavior) | not decoded |
| `$C0n4` | MR | MR; bit 7 runs a software reset |
| `$C0n5` | pointer high byte | pointer high byte |
| `$C0n6` | pointer low byte | pointer low byte |
| `$C0n7` | internal byte at pointer, then advance if AI | internal byte at pointer, then advance if AI |

With MR.IND clear, `$C0n4`-`$C0n7` access internal `$0000`-`$0003`.

## MR bits

| Bit | Name | Emulated |
|---|---|---|
| 7 | RST | software reset, reads back 0 |
| 4 | PB | stored; the virtual gateway's ping replies are unaffected because the guest is the pinger, not the pinged |
| 3 | PPPoE | stored, no effect |
| 1 | AI | auto-increment |
| 0 | IND | indirect mode |

## Common registers

| Address | Name | Reset | Notes |
|---|---|---|---|
| `$0000` | MR | `$00` | |
| `$0001`-`$0004` | GAR | `$00` | no effect on host routing |
| `$0005`-`$0008` | SUBR | `$00` | no effect on host routing |
| `$0009`-`$000E` | SHAR | `$00` | MACRAW filter compares against it |
| `$000F`-`$0012` | SIPR | `$00` | no effect on host routing |
| `$0015` | IR | `$00` | bits 7, 6, 5 clear on write 1; bits 3-0 follow Sn_IR != 0 |
| `$0016` | IMR | `$00` | IRQ asserted while `IR & IMR` |
| `$0017`-`$0018` | RTR | `$07D0` | stored |
| `$0019` | RCR | `$08` | stored |
| `$001A` | RMSR | `$55` | 2 bits per socket, socket 0 in bits 1-0 |
| `$001B` | TMSR | `$55` | as RMSR |
| `$001C`-`$001D` | PATR | `$0000` | read only |
| `$0028` | PTIMER | `$28` | stored |
| `$0029` | PMAGIC | `$00` | stored |
| `$002A`-`$002D` | UIPR | `$00` | read only |
| `$002E`-`$002F` | UPORT | `$0000` | read only |

Other locations below `$0400` and in `$0800`-`$3FFF` read `$00` and ignore writes.

## Socket registers (`$0400 + n * $100`)

| Offset | Name | Reset | Access |
|---|---|---|---|
| `$00` | Sn_MR | `$00` | R/W |
| `$01` | Sn_CR | `$00` | R/W, reads `$00` after a command |
| `$02` | Sn_IR | `$00` | R, write 1 clears |
| `$03` | Sn_SR | `$00` | R |
| `$04`-`$05` | Sn_PORT | `$0000` | R/W |
| `$06`-`$0B` | Sn_DHAR | `$FF` x6 | R/W |
| `$0C`-`$0F` | Sn_DIPR | `$00` | R/W; set from the peer on LISTEN accept |
| `$10`-`$11` | Sn_DPORT | `$0000` | R/W; set from the peer on LISTEN accept |
| `$12`-`$13` | Sn_MSSR | `$0000` | R/W |
| `$14` | Sn_PROTO | `$00` | R/W |
| `$15` | Sn_TOS | `$00` | R/W |
| `$16` | Sn_TTL | `$80` | R/W |
| `$20`-`$21` | Sn_TX_FSR | buffer size | R |
| `$22`-`$23` | Sn_TX_RD | `$0000` | R |
| `$24`-`$25` | Sn_TX_WR | `$0000` | R/W |
| `$26`-`$27` | Sn_RX_RSR | `$0000` | R |
| `$28`-`$29` | Sn_RX_RD | `$0000` | R/W |

## Sn_MR protocol (low nibble) and Sn_CR commands

Protocols: `$0` closed, `$1` TCP, `$2` UDP, `$3` IPRAW (SOCK_CLOSED after OPEN), `$4` MACRAW (socket 0 only), `$5` PPPoE (SOCK_CLOSED after OPEN). Bit 6 MF filters MACRAW frames to SHAR or broadcast.

Commands: `$01` OPEN, `$02` LISTEN, `$04` CONNECT, `$08` DISCON, `$10` CLOSE, `$20` SEND, `$21` SEND_MAC, `$22` SEND_KEEP, `$40` RECV.

## Sn_SR values

`$00` CLOSED, `$13` INIT, `$14` LISTEN, `$15` SYNSENT, `$16` SYNRECV, `$17` ESTABLISHED, `$18` FIN_WAIT, `$1A` CLOSING, `$1B` TIME_WAIT, `$1C` CLOSE_WAIT, `$1D` LAST_ACK, `$22` UDP, `$42` MACRAW.

## Sn_IR bits

Bit 0 CON, bit 1 DISCON, bit 2 RECV, bit 3 TIMEOUT, bit 4 SEND_OK.

## Buffers

TX memory `$4000`-`$5FFF`, RX memory `$6000`-`$7FFF`. Socket n's base is the sum of the sizes of sockets below it; a socket whose base would be 8 KB or more has no buffer. Physical address = base + (pointer register & (size - 1)).

## Receive headers

- UDP: peer IP (4), peer port (2), payload length (2, excludes the header).
- MACRAW: length (2, includes these 2 bytes).
