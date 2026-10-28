# Contract: modem commands and port configuration

## Modem command set

A command line starts with `AT` (either case) and ends with S3 (CR). Commands
may follow each other on one line; `D` takes the rest of the line.

| Command | Effect | Result |
|---|---|---|
| `AT` | none | `OK` |
| `D[T|P]host[:port]` | dial over TCP; port 23 when none | `CONNECT` or `NO CARRIER` |
| `A` | answer a ringing call | `CONNECT` or `NO CARRIER` |
| `H[0|1]` | hang up | `OK` |
| `O` | back to data mode | `CONNECT` or `NO CARRIER` |
| `Z` | hang up, defaults | `OK` |
| `&F` | defaults | `OK` |
| `E0`/`E1` | echo off/on | `OK` |
| `Q0`/`Q1` | results on/off | `OK` (none with Q1) |
| `V0`/`V1` | numeric/verbose results | `OK` |
| `Sn=v` | set S-register | `OK` or `ERROR` |
| `Sn?` | read S-register: 3 decimal digits | value, then `OK` |
| `I` | identify | `Casso modem`, `OK` |
| `X0`-`X4`, `&C`, `&D` | see research R8 | `OK` |
| `B L M N W Y &K &S &W` | ignored | `OK` |
| `A/` | repeat last line, without a CR | as the line |
| anything else | | `ERROR` |

Verbose results are S3 S4 word S3 S4. Numeric results are digits then S3:
`OK` 0, `CONNECT` 1, `RING` 2, `NO CARRIER` 3, `ERROR` 4.

Escape: S12 fiftieths of silence, three S2 characters each within S12 of the
one before, S12 of silence; then `OK` in online command mode. S2 > 127 or
S12 = 0 disables it. All times are emulated.

## Port configuration (machine JSON)

Super Serial Card, under the slot entry:

```json
{ "slot": 2, "device": "super-serial-card",
  "ports": [ { "device": "serial-modem", "telnet": true, "listenPort": 6400 } ] }
```

//c, the machine's root `ports`:

```json
"ports": [
    { "name": "serial1", "device": "serial-com-port", "comPort": "COM3" },
    { "name": "serial2", "device": "serial-tcp", "host": "localhost", "port": 1977, "listen": false, "telnet": false }
]
```

Keys absent from an entry take the defaults in data-model.md.
