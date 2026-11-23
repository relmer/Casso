# Contract: bank-qualified addresses in the debugger

## Text form

```
address      := [ space separator ] hex16
space        := "BANK" bank | "SYSTEM" | "ROM" | "MAIN" | "AUX"   ; any case
bank         := hex digit            ; 0-E
separator    := ":" | "/"            ; by mode, see below
hex16        := 1 to 4 hex digits, with the mode's usual prefix rules
```

| Text | Meaning |
|---|---|
| `3000` | `$3000` in the current view |
| `BANK2:3000` | `$3000` in the Apple ///'s bank 2 |
| `SYSTEM:A000` | `$A000` in the Apple ///'s system bank |
| `ROM:F000` | `$F000` in the Apple ///'s ROM, whatever the environment register selects |
| `MAIN:0800` | `$0800` in the //e's or //c's main memory |
| `AUX:0800` | `$0800` in the //e's or //c's auxiliary memory |

**Words, not letters** (owner decision, 2026-11-23): every machine's
spaces are words, never one letter, so the Apple ///'s match the //e's
`MAIN` and `AUX`. The words are case-insensitive and print in capitals.
035's parser has the words in one table, and 050 adds the Apple ///'s
rows to it.

Apple's manuals write a bank-switched address `b:nnnn` (SOS Reference
Manual 2.1.1); `BANKb:nnnn` is that form with the space written out. It is
the form the debugger prints and the form every pane, Go to box, dialog
field and JSON field accepts.

### Separator by command mode

| Mode | Accepts | Prints | Why |
|---|---|---|---|
| Casso | `:` | `:` | no existing use of `:` in an address |
| WinDbg | `:` | `:` | the same |
| Apple II Monitor | `/` | `/` | `addr: bytes` is the deposit command |
| GSSquared | `/` | `/` | `addr:bytes` is its deposit command too, and `/` is its existing bank separator |

**The rule** (owner decision, 2026-11-17): `:` everywhere, and `/` wherever
`:` already has a meaning. Two more places use `:` today, so both take `/`:

- **Assembler prompt** (`addr:MNE operand` moves the assembly address): a
  bank-qualified address is written `BANK2/3000:MNE operand`.
- **Range fields** (heat map ranges accept `start:end`, `start..end` and
  `start,length`): an end with a bank is written with `/`, for example
  `BANK2/3000:BANK2/3FFF` or `BANK2/3000..BANK2/3FFF`.

Any conflict found later is handled the same way, with no new decision: that
place accepts and prints `/`.

To check when this is built: whether `/` or `:` is an operator in the Casso
and WinDbg modes' address expressions. If `:` is, that mode takes `/` under
the rule above; if `/` is as well, it is the owner's call.

### Rules

- On the Apple ///, `BANKb` is valid only on `$2000-$9FFF`, the switched
  region. `SYSTEM` covers `$0000-$1FFF` and `$A000-$FFFF`, and `ROM` covers
  `$F000-$FFFF`. An address outside its space's range is an error: "illegal
  address", with the rule on the second line.
- A bank beyond the installed memory is the same error.
- A space the machine does not have is the same error: `BANK2` on a //e,
  `AUX` on an Apple ///. `MAIN` and `AUX` are valid on a //e or //c.
- In GSSquared mode, bank `00` stays valid on an Apple II.
- A symbol can be defined at a bank-qualified address; the symbol then
  evaluates to that address.

## Printed form

- An address outside the banked region prints as it does today.
- An address in the banked region prints with its space, for example
  `BANK2:3000` (`BANK2/3000` in the Monitor and GSSquared modes), when it
  came from a specific bank (a stored breakpoint, a trace record, a resolved
  extended access, a search hit) and as `3000` when it is in the current
  view.
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

035 builds the parser and the space prefix in search, memory dump, enter,
fill, BSAVE, BLOAD and the MCP adapter's memory reads and writes, with
`MAIN` and `AUX`. 050 adds the Apple ///'s spaces, breakpoints, the panes'
bank column and trace printing.

## Extended access annotation

```
BANK2:3040   B1 50      LDA ($50),Y     ; -> BANK3:4A10
```

The annotation after `->` is the resolved address. It appears when the
machine is stopped at the line or the line came from the trace, and is
absent in a static listing.

## SOS call

```
BANK2:3120   00 C8 40 31   SOS OPEN, $3140
```

The line contains the `BRK`, the call number and the parameter list
address. The next line starts at `BANK2:3124`.
