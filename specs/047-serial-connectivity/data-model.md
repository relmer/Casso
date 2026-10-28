# Data Model: Serial Connectivity

## SerialAttachmentOptions (config)

Part of `PortConfig`, saved in the machine's JSON with the port entry.

| Field | JSON key | Used by | Default |
|---|---|---|---|
| `comPort` | `comPort` | COM port | empty |
| `host` | `host` | TCP (connect) | empty |
| `tcpPort` | `port` | TCP: connect or listen port | 1977 |
| `listen` | `listen` | TCP | false |
| `telnet` | `telnet` | TCP, modem | false for TCP, true for the modem |
| `modemListenPort` | `listenPort` | modem; 0 = no listening | 0 |

Attachment device strings: `serial-loopback`, `serial-file-out`,
`serial-file-in` (existing), `serial-com-port`, `serial-tcp`, `serial-modem`.

Validation: a port is 1-65535 (0 only for `listenPort`); a TCP connect requires
a host; a COM attachment requires a port name. An attachment whose options fail
validation is not opened, and its problem is reported.

## SerialLineFormat

What the 6551's control and command registers select, passed to an
attachment on attach and on each change.

| Field | Values |
|---|---|
| `baud` | double, bits per second (transmit rate) |
| `wordBits` | 5-8 |
| `parity` | None, Odd, Even, Mark, Space |
| `stopBits` | One, OneAndHalf, Two |

## SerialLines

`cts`, `dsr`, `dcd` (inputs to the 6551) and `dtr`, `rts` (outputs). RI has no
6551 input on either the Super Serial Card or the //c and is not modeled.

## Receive entry

`{ Byte value; Byte errors; }` in the 6551's receive queue. Error bits:
`kRxErrorParity` 0x01, `kRxErrorFraming` 0x02, `kRxErrorOverrun` 0x04 (the
same positions as the status register's). Machine state version 3 saves the
error byte with each waiting entry.

## Stored input (journal)

See research R2. `AciaByte` with error bits in `data`, `AciaLines` with the
CTS/DSR/DCD bits in `value` (`kLineCts` / `kLineDsr` / `kLineDcd` of
`IoEventLog`), `AciaConnection` with the peer text in `payload`.

## Modem state (host-side, not journaled)

| Field | Values |
|---|---|
| mode | Command, Dialing, Ringing, Data, OnlineCommand |
| S-registers | 38 bytes; S0 0, S1 0, S2 43, S3 13, S4 10, S5 8, S6 2, S7 50, S8 2, S9 6, S10 14, S12 50 |
| flags | echo (E1), quiet (Q0), verbose (V1), X level (0), &C (1), &D (2) |
| command line | up to 40 characters after `AT`, plus the last line for `A/` |
| escape | count 0-3, cycle of the last guest byte |
| timers | dial deadline, next ring |

Transitions:

```text
Command --ATD--> Dialing --connected--> Data --+++ guard--> OnlineCommand
Dialing --failed / key / S7--> Command (NO CARRIER)
Command --incoming--> Ringing --ATA or S0--> Data
OnlineCommand --ATO--> Data
Data / OnlineCommand --ATH, ATZ, DTR drop (&D2), remote close, resume--> Command
```

## SerialLinkReport

`{ std::string portTitle; std::string text; uint64_t generation; }` in
`SerialLinkReports`. An empty text clears the port's report.
