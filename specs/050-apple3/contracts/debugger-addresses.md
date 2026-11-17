# Contract: bank-qualified addresses in the debugger

## Text form

```
address      := [ space separator ] hex16
space        := bank | "S" | "R"
bank         := hex digit            ; 0-E
separator    := ":" | "/"            ; by mode, see below
hex16        := 1 to 4 hex digits, with the mode's usual prefix rules
```

| Text | Meaning |
|---|---|
| `3000` | `$3000` in the current view |
| `2:3000` | `$3000` in bank 2 |
| `S:A000` | `$A000` in the system bank |
| `R:F000` | `$F000` in the ROM, whatever the environment register selects |

`b:nnnn` is Apple's notation for a bank-switched address (SOS Reference
Manual 2.1.1). It is the form the debugger prints and the form every pane,
Go to box, dialog field and JSON field accepts.

### Separator by command mode

| Mode | Accepts | Prints | Why |
|---|---|---|---|
| Casso | `:` | `:` | no existing use of `:` in an address |
| WinDbg | `:` | `:` | the same |
| Apple II Monitor | `/` | `/` | `addr: bytes` is the deposit command, so `2:30` already means "store `$30` at `$0002`" |
| GSSquared | `/` | `/` | `addr:bytes` is its deposit command too, and `/` is its existing bank separator |

**The rule** (owner decision, 2026-11-17): `:` everywhere, and `/` wherever
`:` already has a meaning. Two more places use `:` today, so both take `/`:

- **Assembler prompt** (`addr:MNE operand` moves the assembly address): a
  bank-qualified address is written `b/nnnn:MNE operand`.
- **Range fields** (heat map ranges accept `start:end`, `start..end` and
  `start,length`): an end with a bank is written `b/nnnn`, for example
  `2/3000:2/3FFF` or `2/3000..2/3FFF`.

Any conflict found later is handled the same way, with no new decision: that
place accepts and prints `/`.

To check when this is built: whether `/` or `:` is an operator in the Casso
and WinDbg modes' address expressions. If `:` is, that mode takes `/` under
the rule above; if `/` is as well, the owner decides.

### Rules

- A bank on an address outside `$2000-$9FFF` is an error: "illegal address",
  with the rule on the second line.
- A bank beyond the installed memory is the same error.
- On an Apple II machine a space prefix is an error, except in GSSquared
  mode, where 035 already accepts bank `00`.
- A symbol can be defined at a bank-qualified address; the symbol then
  evaluates to that address.

## Printed form

- An address outside the banked region prints as it does today.
- An address in the banked region prints as `b:aaaa` (`b/aaaa` in the
  Monitor and GSSquared modes) when it came from a specific bank (a stored
  breakpoint, a trace record, a resolved extended access) and as `aaaa` when
  it is in the current view.
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
2:3040   B1 50      LDA ($50),Y     ; -> 3:4A10
```

The annotation after `->` is the resolved address. It appears when the
machine is stopped at the line or the line came from the trace, and is
absent in a static listing.

## SOS call

```
2:3120   00 C8 40 31   SOS OPEN, $3140
```

The line contains the `BRK`, the call number and the parameter list
address. The next line starts at `2:3124`.
