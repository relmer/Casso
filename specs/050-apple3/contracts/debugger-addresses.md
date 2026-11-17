# Contract: bank-qualified addresses in the debugger

## Text form

```
address      := [ space "/" ] hex16
space        := bank | "S" | "R"
bank         := hex digit            ; 0-E
hex16        := 1 to 4 hex digits, with the mode's usual prefix rules
```

| Text | Meaning |
|---|---|
| `3000` | `$3000` in the current view |
| `2/3000` | `$3000` in bank 2 |
| `S/A000` | `$A000` in the system bank |
| `R/F000` | `$F000` in the ROM, whatever the environment register selects |

- Accepted wherever an address is accepted, in all four command modes, the
  Go to boxes, and every dialog field that takes an address.
- A bank on an address outside `$2000-$9FFF` is an error: "illegal address",
  with the rule on the second line.
- A bank beyond the installed memory is the same error.
- On an Apple II machine a space prefix is an error, except in GSSquared
  mode, where 035 already accepts bank `00`.
- A symbol can be defined at a bank-qualified address; the symbol then
  evaluates to that address.

## Printed form

- An address outside the banked region prints as it does today.
- An address in the banked region prints as `b/aaaa` when it came from a
  specific bank (a stored breakpoint, a trace record, a resolved extended
  access) and as `aaaa` when it is in the current view.
- The disassembly, trace and memory panes have a bank column that is hidden
  on machines without banks.

## Commands

| Command (Casso mode) | Change |
|---|---|
| registers | lists and sets `E`, `B` and `Z` after the 6502 registers |
| breakpoint, watch, memory, disassemble, fill, search, bookmark, symbol | accept and print the text form above |
| new: break on register | `bank`, `env`, `zp`, each with an optional value or bit |
| BASIC commands | work for Business BASIC |

The other three modes get the same abilities in the syntax of each; the
tasks list each command per mode. The debug channel and the MCP adapter use
the Casso mode's text form in every address field of their JSON.

## Extended access annotation

```
1A/2040   B1 50      LDA ($50),Y     ; -> 3/4A10
```

The annotation after `->` is the resolved address. It appears when the
machine is stopped at the line or the line came from the trace, and is
absent in a static listing.

## SOS call

```
2/3120   00 C8 40 31   SOS OPEN, $3140
```

The line contains the `BRK`, the call number and the parameter list
address. The next line starts at `2/3124`.
