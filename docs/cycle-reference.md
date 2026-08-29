# Cycle Reference: 6502 and 65C02

GENERATED FILE -- do not edit by hand. Regenerate with
`scripts/UpdateCycleReference.ps1`; a unit test fails when this file and the
emulator's instruction tables disagree.

Every number here is read out of Casso's own `Microcode` tables -- the
`baseCycles` the emulator actually bills for each instruction -- so the
document cannot describe a machine other than the one this build emulates.

## What the cycle column is

`baseCycles` is the *unconditional* cost. Three costs are added at run time
and are therefore NOT in the column; add them yourself for the real figure:

1. **Page crossing, +1.** An indexed READ through `abs,X`, `abs,Y` or `(zp),Y`
   pays one extra cycle when the effective address lands in a different page
   from the base. Stores and read-modify-writes normally do not: the part
   cannot know whether the page crossed until it has read, and it must write
   either way, so that cycle is already inside their `baseCycles`. The 65C02's
   `abs,X` shifts and rotates are the exception -- there the cost really is
   conditional, and the table marks them with a trailing `+`.
2. **Branch taken, +1; taken across a page, +1 more.** A conditional branch
   not taken costs the 2 in the column, and the page is measured against the
   instruction after the branch, so a displacement of zero is still a taken
   branch and still costs the extra cycle. `BRA` is unconditional, so its real
   minimum is 3 rather than the 2 stored for it here; `BBRn` and `BBSn` branch
   as well, costing 5, 6 taken, and 7 taken across a page.
3. **Decimal arithmetic on the 65C02, +1.** `ADC` and `SBC` cost one extra
   cycle while the decimal flag is set, which is what buys the CMOS part its
   correct N, V and Z in decimal mode. The NMOS core pays no such cycle.

Those three are the whole of it, so two cycle counts on the same row differ
only because the two cores genuinely bill that opcode differently.

## Reading the tables

- `Len` is the whole instruction in bytes, opcode included.
- `---` means the core does not implement that opcode. Only the NMOS column
  ever shows it: the 65C02 defines all 256. Casso executes an unimplemented
  opcode as a one-byte, two-cycle NOP and keeps running rather than trapping,
  because period software does execute them.
- A trailing `+` on a cycle count marks an instruction that pays one more
  cycle only when the indexed address crosses a page. Ordinary indexed reads
  pay that cycle too and are not marked; the marker is for the read-modify-
  writes, where the same opcode is conditional on one core and flat on the
  other.
- A trailing `*` marks a slot the assembler will not emit: the NMOS
  undocumented opcodes, and the 65C02's reserved opcode-map fill. They
  execute and disassemble normally; they simply cannot be written by
  mnemonic, so that a filler `NOP` can never shadow the canonical `$EA`.
- Addressing modes are written as the operand syntax an assembler accepts.
  `(abs)` appears for both cores at `$6C` even though the 65C02 fixes the
  page-boundary bug there: the syntax is the same, the behavior is not.
- The 65C02's fill slots are one byte and one cycle apart from the handful
  that read operand bytes. `$5C` is the outlier worth knowing about: three
  bytes and eight cycles, the most expensive NOP either part has.

## Base cycles at a glance: NMOS 6502

```text
     x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF
0x    7  6 --  8  3  3  5  5  3  2  2 --  4  4  6  6
1x    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
2x    6  6 --  8  3  3  5  5  4  2  2 --  4  4  6  6
3x    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
4x    6  6 --  8  3  3  5  5  3  2  2 --  3  4  6  6
5x    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
6x    6  6 --  8  3  3  5  5  4  2  2 --  5  4  6  6
7x    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
8x    2  6  2  6  3  3  3  3  2  2  2 --  4  4  4  4
9x    2  6 -- --  4  4  4  4  2  5  2 -- --  5 -- --
Ax    2  6  2  6  3  3  3  3  2  2  2 --  4  4  4  4
Bx    2  5 --  5  4  4  4  4  2  4  2 --  4  4  4  4
Cx    2  6  2  8  3  3  5  5  2  2  2 --  4  4  6  6
Dx    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
Ex    2  6  2  8  3  3  5  5  2  2  2 --  4  4  6  6
Fx    2  5 --  8  4  4  6  6  2  4  2  7  4  4  7  7
```

## Base cycles at a glance: 65C02 (Rockwell R65C02)

```text
     x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF
0x    7  6  2  1  5  3  5  5  3  2  2  1  6  4  6  5
1x    2  5  5  1  5  4  6  5  2  4  2  1  6  4 6+  5
2x    6  6  2  1  3  3  5  5  4  2  2  1  4  4  6  5
3x    2  5  5  1  4  4  6  5  2  4  2  1  4  4 6+  5
4x    6  6  2  1  3  3  5  5  3  2  2  1  3  4  6  5
5x    2  5  5  1  4  4  6  5  2  4  3  1  8  4 6+  5
6x    6  6  2  1  3  3  5  5  4  2  2  1  6  4  6  5
7x    2  5  5  1  4  4  6  5  2  4  4  1  6  4 6+  5
8x    2  6  2  1  3  3  3  5  2  2  2  1  4  4  4  5
9x    2  6  5  1  4  4  4  5  2  5  2  1  4  5  5  5
Ax    2  6  2  1  3  3  3  5  2  2  2  1  4  4  4  5
Bx    2  5  5  1  4  4  4  5  2  4  2  1  4  4  4  5
Cx    2  6  2  1  3  3  5  5  2  2  2  1  4  4  6  5
Dx    2  5  5  1  4  4  6  5  2  4  3  1  4  4  7  5
Ex    2  6  2  1  3  3  5  5  2  2  2  1  4  4  6  5
Fx    2  5  5  1  4  4  6  5  2  4  4  1  4  4  7  5
```

## Every opcode

| Op  | NMOS   | Mode     | Len | Cyc | 65C02  | Mode     | Len | Cyc |
| --- | ------ | -------- | --- | --- | ------ | -------- | --- | --- |
| $00 | BRK    | impl     | 1   | 7   | BRK    | impl     | 1   | 7   |
| $01 | ORA    | (zp,X)   | 2   | 6   | ORA    | (zp,X)   | 2   | 6   |
| $02 | ---    | ---      | -   | -   | NOP*   | #imm     | 2   | 2   |
| $03 | SLO*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $04 | NOP*   | zp       | 2   | 3   | TSB    | zp       | 2   | 5   |
| $05 | ORA    | zp       | 2   | 3   | ORA    | zp       | 2   | 3   |
| $06 | ASL    | zp       | 2   | 5   | ASL    | zp       | 2   | 5   |
| $07 | SLO*   | zp       | 2   | 5   | RMB0   | zp       | 2   | 5   |
| $08 | PHP    | impl     | 1   | 3   | PHP    | impl     | 1   | 3   |
| $09 | ORA    | #imm     | 2   | 2   | ORA    | #imm     | 2   | 2   |
| $0A | ASL    | A        | 1   | 2   | ASL    | A        | 1   | 2   |
| $0B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $0C | NOP*   | abs      | 3   | 4   | TSB    | abs      | 3   | 6   |
| $0D | ORA    | abs      | 3   | 4   | ORA    | abs      | 3   | 4   |
| $0E | ASL    | abs      | 3   | 6   | ASL    | abs      | 3   | 6   |
| $0F | SLO*   | abs      | 3   | 6   | BBR0   | zp,rel   | 3   | 5   |
| $10 | BPL    | rel      | 2   | 2   | BPL    | rel      | 2   | 2   |
| $11 | ORA    | (zp),Y   | 2   | 5   | ORA    | (zp),Y   | 2   | 5   |
| $12 | ---    | ---      | -   | -   | ORA    | (zp)     | 2   | 5   |
| $13 | SLO*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $14 | NOP*   | zp,X     | 2   | 4   | TRB    | zp       | 2   | 5   |
| $15 | ORA    | zp,X     | 2   | 4   | ORA    | zp,X     | 2   | 4   |
| $16 | ASL    | zp,X     | 2   | 6   | ASL    | zp,X     | 2   | 6   |
| $17 | SLO*   | zp,X     | 2   | 6   | RMB1   | zp       | 2   | 5   |
| $18 | CLC    | impl     | 1   | 2   | CLC    | impl     | 1   | 2   |
| $19 | ORA    | abs,Y    | 3   | 4   | ORA    | abs,Y    | 3   | 4   |
| $1A | NOP*   | impl     | 1   | 2   | INC    | A        | 1   | 2   |
| $1B | SLO*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $1C | NOP*   | abs,X    | 3   | 4   | TRB    | abs      | 3   | 6   |
| $1D | ORA    | abs,X    | 3   | 4   | ORA    | abs,X    | 3   | 4   |
| $1E | ASL    | abs,X    | 3   | 7   | ASL    | abs,X    | 3   | 6+  |
| $1F | SLO*   | abs,X    | 3   | 7   | BBR1   | zp,rel   | 3   | 5   |
| $20 | JSR    | abs      | 3   | 6   | JSR    | abs      | 3   | 6   |
| $21 | AND    | (zp,X)   | 2   | 6   | AND    | (zp,X)   | 2   | 6   |
| $22 | ---    | ---      | -   | -   | NOP*   | #imm     | 2   | 2   |
| $23 | RLA*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $24 | BIT    | zp       | 2   | 3   | BIT    | zp       | 2   | 3   |
| $25 | AND    | zp       | 2   | 3   | AND    | zp       | 2   | 3   |
| $26 | ROL    | zp       | 2   | 5   | ROL    | zp       | 2   | 5   |
| $27 | RLA*   | zp       | 2   | 5   | RMB2   | zp       | 2   | 5   |
| $28 | PLP    | impl     | 1   | 4   | PLP    | impl     | 1   | 4   |
| $29 | AND    | #imm     | 2   | 2   | AND    | #imm     | 2   | 2   |
| $2A | ROL    | A        | 1   | 2   | ROL    | A        | 1   | 2   |
| $2B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $2C | BIT    | abs      | 3   | 4   | BIT    | abs      | 3   | 4   |
| $2D | AND    | abs      | 3   | 4   | AND    | abs      | 3   | 4   |
| $2E | ROL    | abs      | 3   | 6   | ROL    | abs      | 3   | 6   |
| $2F | RLA*   | abs      | 3   | 6   | BBR2   | zp,rel   | 3   | 5   |
| $30 | BMI    | rel      | 2   | 2   | BMI    | rel      | 2   | 2   |
| $31 | AND    | (zp),Y   | 2   | 5   | AND    | (zp),Y   | 2   | 5   |
| $32 | ---    | ---      | -   | -   | AND    | (zp)     | 2   | 5   |
| $33 | RLA*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $34 | NOP*   | zp,X     | 2   | 4   | BIT    | zp,X     | 2   | 4   |
| $35 | AND    | zp,X     | 2   | 4   | AND    | zp,X     | 2   | 4   |
| $36 | ROL    | zp,X     | 2   | 6   | ROL    | zp,X     | 2   | 6   |
| $37 | RLA*   | zp,X     | 2   | 6   | RMB3   | zp       | 2   | 5   |
| $38 | SEC    | impl     | 1   | 2   | SEC    | impl     | 1   | 2   |
| $39 | AND    | abs,Y    | 3   | 4   | AND    | abs,Y    | 3   | 4   |
| $3A | NOP*   | impl     | 1   | 2   | DEC    | A        | 1   | 2   |
| $3B | RLA*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $3C | NOP*   | abs,X    | 3   | 4   | BIT    | abs,X    | 3   | 4   |
| $3D | AND    | abs,X    | 3   | 4   | AND    | abs,X    | 3   | 4   |
| $3E | ROL    | abs,X    | 3   | 7   | ROL    | abs,X    | 3   | 6+  |
| $3F | RLA*   | abs,X    | 3   | 7   | BBR3   | zp,rel   | 3   | 5   |
| $40 | RTI    | impl     | 1   | 6   | RTI    | impl     | 1   | 6   |
| $41 | EOR    | (zp,X)   | 2   | 6   | EOR    | (zp,X)   | 2   | 6   |
| $42 | ---    | ---      | -   | -   | NOP*   | #imm     | 2   | 2   |
| $43 | SRE*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $44 | NOP*   | zp       | 2   | 3   | NOP*   | zp       | 2   | 3   |
| $45 | EOR    | zp       | 2   | 3   | EOR    | zp       | 2   | 3   |
| $46 | LSR    | zp       | 2   | 5   | LSR    | zp       | 2   | 5   |
| $47 | SRE*   | zp       | 2   | 5   | RMB4   | zp       | 2   | 5   |
| $48 | PHA    | impl     | 1   | 3   | PHA    | impl     | 1   | 3   |
| $49 | EOR    | #imm     | 2   | 2   | EOR    | #imm     | 2   | 2   |
| $4A | LSR    | A        | 1   | 2   | LSR    | A        | 1   | 2   |
| $4B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $4C | JMP    | abs      | 3   | 3   | JMP    | abs      | 3   | 3   |
| $4D | EOR    | abs      | 3   | 4   | EOR    | abs      | 3   | 4   |
| $4E | LSR    | abs      | 3   | 6   | LSR    | abs      | 3   | 6   |
| $4F | SRE*   | abs      | 3   | 6   | BBR4   | zp,rel   | 3   | 5   |
| $50 | BVC    | rel      | 2   | 2   | BVC    | rel      | 2   | 2   |
| $51 | EOR    | (zp),Y   | 2   | 5   | EOR    | (zp),Y   | 2   | 5   |
| $52 | ---    | ---      | -   | -   | EOR    | (zp)     | 2   | 5   |
| $53 | SRE*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $54 | NOP*   | zp,X     | 2   | 4   | NOP*   | zp,X     | 2   | 4   |
| $55 | EOR    | zp,X     | 2   | 4   | EOR    | zp,X     | 2   | 4   |
| $56 | LSR    | zp,X     | 2   | 6   | LSR    | zp,X     | 2   | 6   |
| $57 | SRE*   | zp,X     | 2   | 6   | RMB5   | zp       | 2   | 5   |
| $58 | CLI    | impl     | 1   | 2   | CLI    | impl     | 1   | 2   |
| $59 | EOR    | abs,Y    | 3   | 4   | EOR    | abs,Y    | 3   | 4   |
| $5A | NOP*   | impl     | 1   | 2   | PHY    | impl     | 1   | 3   |
| $5B | SRE*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $5C | NOP*   | abs,X    | 3   | 4   | NOP*   | abs      | 3   | 8   |
| $5D | EOR    | abs,X    | 3   | 4   | EOR    | abs,X    | 3   | 4   |
| $5E | LSR    | abs,X    | 3   | 7   | LSR    | abs,X    | 3   | 6+  |
| $5F | SRE*   | abs,X    | 3   | 7   | BBR5   | zp,rel   | 3   | 5   |
| $60 | RTS    | impl     | 1   | 6   | RTS    | impl     | 1   | 6   |
| $61 | ADC    | (zp,X)   | 2   | 6   | ADC    | (zp,X)   | 2   | 6   |
| $62 | ---    | ---      | -   | -   | NOP*   | #imm     | 2   | 2   |
| $63 | RRA*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $64 | NOP*   | zp       | 2   | 3   | STZ    | zp       | 2   | 3   |
| $65 | ADC    | zp       | 2   | 3   | ADC    | zp       | 2   | 3   |
| $66 | ROR    | zp       | 2   | 5   | ROR    | zp       | 2   | 5   |
| $67 | RRA*   | zp       | 2   | 5   | RMB6   | zp       | 2   | 5   |
| $68 | PLA    | impl     | 1   | 4   | PLA    | impl     | 1   | 4   |
| $69 | ADC    | #imm     | 2   | 2   | ADC    | #imm     | 2   | 2   |
| $6A | ROR    | A        | 1   | 2   | ROR    | A        | 1   | 2   |
| $6B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $6C | JMP    | (abs)    | 3   | 5   | JMP    | (abs)    | 3   | 6   |
| $6D | ADC    | abs      | 3   | 4   | ADC    | abs      | 3   | 4   |
| $6E | ROR    | abs      | 3   | 6   | ROR    | abs      | 3   | 6   |
| $6F | RRA*   | abs      | 3   | 6   | BBR6   | zp,rel   | 3   | 5   |
| $70 | BVS    | rel      | 2   | 2   | BVS    | rel      | 2   | 2   |
| $71 | ADC    | (zp),Y   | 2   | 5   | ADC    | (zp),Y   | 2   | 5   |
| $72 | ---    | ---      | -   | -   | ADC    | (zp)     | 2   | 5   |
| $73 | RRA*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $74 | NOP*   | zp,X     | 2   | 4   | STZ    | zp,X     | 2   | 4   |
| $75 | ADC    | zp,X     | 2   | 4   | ADC    | zp,X     | 2   | 4   |
| $76 | ROR    | zp,X     | 2   | 6   | ROR    | zp,X     | 2   | 6   |
| $77 | RRA*   | zp,X     | 2   | 6   | RMB7   | zp       | 2   | 5   |
| $78 | SEI    | impl     | 1   | 2   | SEI    | impl     | 1   | 2   |
| $79 | ADC    | abs,Y    | 3   | 4   | ADC    | abs,Y    | 3   | 4   |
| $7A | NOP*   | impl     | 1   | 2   | PLY    | impl     | 1   | 4   |
| $7B | RRA*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $7C | NOP*   | abs,X    | 3   | 4   | JMP    | (abs,X)  | 3   | 6   |
| $7D | ADC    | abs,X    | 3   | 4   | ADC    | abs,X    | 3   | 4   |
| $7E | ROR    | abs,X    | 3   | 7   | ROR    | abs,X    | 3   | 6+  |
| $7F | RRA*   | abs,X    | 3   | 7   | BBR7   | zp,rel   | 3   | 5   |
| $80 | NOP*   | #imm     | 2   | 2   | BRA    | rel      | 2   | 2   |
| $81 | STA    | (zp,X)   | 2   | 6   | STA    | (zp,X)   | 2   | 6   |
| $82 | NOP*   | #imm     | 2   | 2   | NOP*   | #imm     | 2   | 2   |
| $83 | SAX*   | (zp,X)   | 2   | 6   | NOP*   | impl     | 1   | 1   |
| $84 | STY    | zp       | 2   | 3   | STY    | zp       | 2   | 3   |
| $85 | STA    | zp       | 2   | 3   | STA    | zp       | 2   | 3   |
| $86 | STX    | zp       | 2   | 3   | STX    | zp       | 2   | 3   |
| $87 | SAX*   | zp       | 2   | 3   | SMB0   | zp       | 2   | 5   |
| $88 | DEY    | impl     | 1   | 2   | DEY    | impl     | 1   | 2   |
| $89 | NOP*   | #imm     | 2   | 2   | BIT    | #imm     | 2   | 2   |
| $8A | TXA    | impl     | 1   | 2   | TXA    | impl     | 1   | 2   |
| $8B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $8C | STY    | abs      | 3   | 4   | STY    | abs      | 3   | 4   |
| $8D | STA    | abs      | 3   | 4   | STA    | abs      | 3   | 4   |
| $8E | STX    | abs      | 3   | 4   | STX    | abs      | 3   | 4   |
| $8F | SAX*   | abs      | 3   | 4   | BBS0   | zp,rel   | 3   | 5   |
| $90 | BCC    | rel      | 2   | 2   | BCC    | rel      | 2   | 2   |
| $91 | STA    | (zp),Y   | 2   | 6   | STA    | (zp),Y   | 2   | 6   |
| $92 | ---    | ---      | -   | -   | STA    | (zp)     | 2   | 5   |
| $93 | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $94 | STY    | zp,X     | 2   | 4   | STY    | zp,X     | 2   | 4   |
| $95 | STA    | zp,X     | 2   | 4   | STA    | zp,X     | 2   | 4   |
| $96 | STX    | zp,Y     | 2   | 4   | STX    | zp,Y     | 2   | 4   |
| $97 | SAX*   | zp,Y     | 2   | 4   | SMB1   | zp       | 2   | 5   |
| $98 | TYA    | impl     | 1   | 2   | TYA    | impl     | 1   | 2   |
| $99 | STA    | abs,Y    | 3   | 5   | STA    | abs,Y    | 3   | 5   |
| $9A | TXS    | impl     | 1   | 2   | TXS    | impl     | 1   | 2   |
| $9B | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $9C | ---    | ---      | -   | -   | STZ    | abs      | 3   | 4   |
| $9D | STA    | abs,X    | 3   | 5   | STA    | abs,X    | 3   | 5   |
| $9E | ---    | ---      | -   | -   | STZ    | abs,X    | 3   | 5   |
| $9F | ---    | ---      | -   | -   | BBS1   | zp,rel   | 3   | 5   |
| $A0 | LDY    | #imm     | 2   | 2   | LDY    | #imm     | 2   | 2   |
| $A1 | LDA    | (zp,X)   | 2   | 6   | LDA    | (zp,X)   | 2   | 6   |
| $A2 | LDX    | #imm     | 2   | 2   | LDX    | #imm     | 2   | 2   |
| $A3 | LAX*   | (zp,X)   | 2   | 6   | NOP*   | impl     | 1   | 1   |
| $A4 | LDY    | zp       | 2   | 3   | LDY    | zp       | 2   | 3   |
| $A5 | LDA    | zp       | 2   | 3   | LDA    | zp       | 2   | 3   |
| $A6 | LDX    | zp       | 2   | 3   | LDX    | zp       | 2   | 3   |
| $A7 | LAX*   | zp       | 2   | 3   | SMB2   | zp       | 2   | 5   |
| $A8 | TAY    | impl     | 1   | 2   | TAY    | impl     | 1   | 2   |
| $A9 | LDA    | #imm     | 2   | 2   | LDA    | #imm     | 2   | 2   |
| $AA | TAX    | impl     | 1   | 2   | TAX    | impl     | 1   | 2   |
| $AB | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $AC | LDY    | abs      | 3   | 4   | LDY    | abs      | 3   | 4   |
| $AD | LDA    | abs      | 3   | 4   | LDA    | abs      | 3   | 4   |
| $AE | LDX    | abs      | 3   | 4   | LDX    | abs      | 3   | 4   |
| $AF | LAX*   | abs      | 3   | 4   | BBS2   | zp,rel   | 3   | 5   |
| $B0 | BCS    | rel      | 2   | 2   | BCS    | rel      | 2   | 2   |
| $B1 | LDA    | (zp),Y   | 2   | 5   | LDA    | (zp),Y   | 2   | 5   |
| $B2 | ---    | ---      | -   | -   | LDA    | (zp)     | 2   | 5   |
| $B3 | LAX*   | (zp),Y   | 2   | 5   | NOP*   | impl     | 1   | 1   |
| $B4 | LDY    | zp,X     | 2   | 4   | LDY    | zp,X     | 2   | 4   |
| $B5 | LDA    | zp,X     | 2   | 4   | LDA    | zp,X     | 2   | 4   |
| $B6 | LDX    | zp,Y     | 2   | 4   | LDX    | zp,Y     | 2   | 4   |
| $B7 | LAX*   | zp,Y     | 2   | 4   | SMB3   | zp       | 2   | 5   |
| $B8 | CLV    | impl     | 1   | 2   | CLV    | impl     | 1   | 2   |
| $B9 | LDA    | abs,Y    | 3   | 4   | LDA    | abs,Y    | 3   | 4   |
| $BA | TSX    | impl     | 1   | 2   | TSX    | impl     | 1   | 2   |
| $BB | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $BC | LDY    | abs,X    | 3   | 4   | LDY    | abs,X    | 3   | 4   |
| $BD | LDA    | abs,X    | 3   | 4   | LDA    | abs,X    | 3   | 4   |
| $BE | LDX    | abs,Y    | 3   | 4   | LDX    | abs,Y    | 3   | 4   |
| $BF | LAX*   | abs,Y    | 3   | 4   | BBS3   | zp,rel   | 3   | 5   |
| $C0 | CPY    | #imm     | 2   | 2   | CPY    | #imm     | 2   | 2   |
| $C1 | CMP    | (zp,X)   | 2   | 6   | CMP    | (zp,X)   | 2   | 6   |
| $C2 | NOP*   | #imm     | 2   | 2   | NOP*   | #imm     | 2   | 2   |
| $C3 | DCP*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $C4 | CPY    | zp       | 2   | 3   | CPY    | zp       | 2   | 3   |
| $C5 | CMP    | zp       | 2   | 3   | CMP    | zp       | 2   | 3   |
| $C6 | DEC    | zp       | 2   | 5   | DEC    | zp       | 2   | 5   |
| $C7 | DCP*   | zp       | 2   | 5   | SMB4   | zp       | 2   | 5   |
| $C8 | INY    | impl     | 1   | 2   | INY    | impl     | 1   | 2   |
| $C9 | CMP    | #imm     | 2   | 2   | CMP    | #imm     | 2   | 2   |
| $CA | DEX    | impl     | 1   | 2   | DEX    | impl     | 1   | 2   |
| $CB | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $CC | CPY    | abs      | 3   | 4   | CPY    | abs      | 3   | 4   |
| $CD | CMP    | abs      | 3   | 4   | CMP    | abs      | 3   | 4   |
| $CE | DEC    | abs      | 3   | 6   | DEC    | abs      | 3   | 6   |
| $CF | DCP*   | abs      | 3   | 6   | BBS4   | zp,rel   | 3   | 5   |
| $D0 | BNE    | rel      | 2   | 2   | BNE    | rel      | 2   | 2   |
| $D1 | CMP    | (zp),Y   | 2   | 5   | CMP    | (zp),Y   | 2   | 5   |
| $D2 | ---    | ---      | -   | -   | CMP    | (zp)     | 2   | 5   |
| $D3 | DCP*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $D4 | NOP*   | zp,X     | 2   | 4   | NOP*   | zp,X     | 2   | 4   |
| $D5 | CMP    | zp,X     | 2   | 4   | CMP    | zp,X     | 2   | 4   |
| $D6 | DEC    | zp,X     | 2   | 6   | DEC    | zp,X     | 2   | 6   |
| $D7 | DCP*   | zp,X     | 2   | 6   | SMB5   | zp       | 2   | 5   |
| $D8 | CLD    | impl     | 1   | 2   | CLD    | impl     | 1   | 2   |
| $D9 | CMP    | abs,Y    | 3   | 4   | CMP    | abs,Y    | 3   | 4   |
| $DA | NOP*   | impl     | 1   | 2   | PHX    | impl     | 1   | 3   |
| $DB | DCP*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $DC | NOP*   | abs,X    | 3   | 4   | NOP*   | abs      | 3   | 4   |
| $DD | CMP    | abs,X    | 3   | 4   | CMP    | abs,X    | 3   | 4   |
| $DE | DEC    | abs,X    | 3   | 7   | DEC    | abs,X    | 3   | 7   |
| $DF | DCP*   | abs,X    | 3   | 7   | BBS5   | zp,rel   | 3   | 5   |
| $E0 | CPX    | #imm     | 2   | 2   | CPX    | #imm     | 2   | 2   |
| $E1 | SBC    | (zp,X)   | 2   | 6   | SBC    | (zp,X)   | 2   | 6   |
| $E2 | NOP*   | #imm     | 2   | 2   | NOP*   | #imm     | 2   | 2   |
| $E3 | ISC*   | (zp,X)   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $E4 | CPX    | zp       | 2   | 3   | CPX    | zp       | 2   | 3   |
| $E5 | SBC    | zp       | 2   | 3   | SBC    | zp       | 2   | 3   |
| $E6 | INC    | zp       | 2   | 5   | INC    | zp       | 2   | 5   |
| $E7 | ISC*   | zp       | 2   | 5   | SMB6   | zp       | 2   | 5   |
| $E8 | INX    | impl     | 1   | 2   | INX    | impl     | 1   | 2   |
| $E9 | SBC    | #imm     | 2   | 2   | SBC    | #imm     | 2   | 2   |
| $EA | NOP    | impl     | 1   | 2   | NOP    | impl     | 1   | 2   |
| $EB | ---    | ---      | -   | -   | NOP*   | impl     | 1   | 1   |
| $EC | CPX    | abs      | 3   | 4   | CPX    | abs      | 3   | 4   |
| $ED | SBC    | abs      | 3   | 4   | SBC    | abs      | 3   | 4   |
| $EE | INC    | abs      | 3   | 6   | INC    | abs      | 3   | 6   |
| $EF | ISC*   | abs      | 3   | 6   | BBS6   | zp,rel   | 3   | 5   |
| $F0 | BEQ    | rel      | 2   | 2   | BEQ    | rel      | 2   | 2   |
| $F1 | SBC    | (zp),Y   | 2   | 5   | SBC    | (zp),Y   | 2   | 5   |
| $F2 | ---    | ---      | -   | -   | SBC    | (zp)     | 2   | 5   |
| $F3 | ISC*   | (zp),Y   | 2   | 8   | NOP*   | impl     | 1   | 1   |
| $F4 | NOP*   | zp,X     | 2   | 4   | NOP*   | zp,X     | 2   | 4   |
| $F5 | SBC    | zp,X     | 2   | 4   | SBC    | zp,X     | 2   | 4   |
| $F6 | INC    | zp,X     | 2   | 6   | INC    | zp,X     | 2   | 6   |
| $F7 | ISC*   | zp,X     | 2   | 6   | SMB7   | zp       | 2   | 5   |
| $F8 | SED    | impl     | 1   | 2   | SED    | impl     | 1   | 2   |
| $F9 | SBC    | abs,Y    | 3   | 4   | SBC    | abs,Y    | 3   | 4   |
| $FA | NOP*   | impl     | 1   | 2   | PLX    | impl     | 1   | 4   |
| $FB | ISC*   | abs,Y    | 3   | 7   | NOP*   | impl     | 1   | 1   |
| $FC | NOP*   | abs,X    | 3   | 4   | NOP*   | abs      | 3   | 4   |
| $FD | SBC    | abs,X    | 3   | 4   | SBC    | abs,X    | 3   | 4   |
| $FE | INC    | abs,X    | 3   | 7   | INC    | abs,X    | 3   | 7   |
| $FF | ISC*   | abs,X    | 3   | 7   | BBS7   | zp,rel   | 3   | 5   |

## Where the two cores bill the same instruction differently

5 opcode(s), found by comparing the two tables rather than by hand.

| Op  | Mnem   | Mode     | NMOS  | 65C02 |
| --- | ------ | -------- | ----- | ----- |
| $1E | ASL    | abs,X    | 7     | 6+    |
| $3E | ROL    | abs,X    | 7     | 6+    |
| $5E | LSR    | abs,X    | 7     | 6+    |
| $6C | JMP    | (abs)    | 5     | 6     |
| $7E | ROR    | abs,X    | 7     | 6+    |

## Instructions the 65C02 adds

59 opcode(s) the assembler can write for the 65C02 and not for the NMOS
core. Another 46 CMOS slot(s) are opcode-map fill that executes as a NOP,
marked `*` in the main table, and 0 opcode(s) the NMOS assembler can write
have no writable 65C02 equivalent.

| Op  | Mnem   | Mode     | Len | Cyc |
| --- | ------ | -------- | --- | --- |
| $04 | TSB    | zp       | 2   | 5   |
| $07 | RMB0   | zp       | 2   | 5   |
| $0C | TSB    | abs      | 3   | 6   |
| $0F | BBR0   | zp,rel   | 3   | 5   |
| $12 | ORA    | (zp)     | 2   | 5   |
| $14 | TRB    | zp       | 2   | 5   |
| $17 | RMB1   | zp       | 2   | 5   |
| $1A | INC    | A        | 1   | 2   |
| $1C | TRB    | abs      | 3   | 6   |
| $1F | BBR1   | zp,rel   | 3   | 5   |
| $27 | RMB2   | zp       | 2   | 5   |
| $2F | BBR2   | zp,rel   | 3   | 5   |
| $32 | AND    | (zp)     | 2   | 5   |
| $34 | BIT    | zp,X     | 2   | 4   |
| $37 | RMB3   | zp       | 2   | 5   |
| $3A | DEC    | A        | 1   | 2   |
| $3C | BIT    | abs,X    | 3   | 4   |
| $3F | BBR3   | zp,rel   | 3   | 5   |
| $47 | RMB4   | zp       | 2   | 5   |
| $4F | BBR4   | zp,rel   | 3   | 5   |
| $52 | EOR    | (zp)     | 2   | 5   |
| $57 | RMB5   | zp       | 2   | 5   |
| $5A | PHY    | impl     | 1   | 3   |
| $5F | BBR5   | zp,rel   | 3   | 5   |
| $64 | STZ    | zp       | 2   | 3   |
| $67 | RMB6   | zp       | 2   | 5   |
| $6F | BBR6   | zp,rel   | 3   | 5   |
| $72 | ADC    | (zp)     | 2   | 5   |
| $74 | STZ    | zp,X     | 2   | 4   |
| $77 | RMB7   | zp       | 2   | 5   |
| $7A | PLY    | impl     | 1   | 4   |
| $7C | JMP    | (abs,X)  | 3   | 6   |
| $7F | BBR7   | zp,rel   | 3   | 5   |
| $80 | BRA    | rel      | 2   | 2   |
| $87 | SMB0   | zp       | 2   | 5   |
| $89 | BIT    | #imm     | 2   | 2   |
| $8F | BBS0   | zp,rel   | 3   | 5   |
| $92 | STA    | (zp)     | 2   | 5   |
| $97 | SMB1   | zp       | 2   | 5   |
| $9C | STZ    | abs      | 3   | 4   |
| $9E | STZ    | abs,X    | 3   | 5   |
| $9F | BBS1   | zp,rel   | 3   | 5   |
| $A7 | SMB2   | zp       | 2   | 5   |
| $AF | BBS2   | zp,rel   | 3   | 5   |
| $B2 | LDA    | (zp)     | 2   | 5   |
| $B7 | SMB3   | zp       | 2   | 5   |
| $BF | BBS3   | zp,rel   | 3   | 5   |
| $C7 | SMB4   | zp       | 2   | 5   |
| $CF | BBS4   | zp,rel   | 3   | 5   |
| $D2 | CMP    | (zp)     | 2   | 5   |
| $D7 | SMB5   | zp       | 2   | 5   |
| $DA | PHX    | impl     | 1   | 3   |
| $DF | BBS5   | zp,rel   | 3   | 5   |
| $E7 | SMB6   | zp       | 2   | 5   |
| $EF | BBS6   | zp,rel   | 3   | 5   |
| $F2 | SBC    | (zp)     | 2   | 5   |
| $F7 | SMB7   | zp       | 2   | 5   |
| $FA | PLX    | impl     | 1   | 4   |
| $FF | BBS7   | zp,rel   | 3   | 5   |

## How these numbers are checked

Tom Harte's SingleStepTests vectors record what each instruction really cost
for that vector's own operands, so every count here is compared against
recorded hardware behavior with the conditional cycles already in it: the
page crossing that happened, the branch that was taken, the decimal `ADC`.
That runs at 200 vectors per opcode on every build and 10,000 on demand,
across the documented, undocumented and Rockwell 65C02 tiers alike. What it
does not pin is WHICH cycle a given bus access lands on; only the total is
kept.

## Where Casso differs from the upstream vectors

Three undefined 65C02 opcodes, where the corpus and the published per-opcode
tables disagree. Casso follows the tables, and the harness carries the
exemption by name rather than quietly passing:

- `$DB` -- a one-byte NOP here, two bytes upstream. Klaus Dormann's
  functional test asserts one byte, so the whole opcode is skipped.
- `$5C` -- 8 cycles here, 4 upstream. Only the cycle comparison is skipped.
- `$CB` -- 1 cycle here, 2 upstream. Only the cycle comparison is skipped.

Everything else about `$5C` and `$CB` -- registers, flags, memory, and how
many bytes the opcode swallows -- is still compared. The reasoning is in the
"Disputed slots" section of `docs/testing.md`.

## References consulted

The timing above was verified against two public references. Neither is
reproduced here, in whole or in part: this document is generated from Casso's
own instruction tables, and the references were read only to check the
result.

- Bruce Clark, "65C02 Opcodes" -- <https://6502.org/tutorials/65c02opcodes.html>
- Graham, 65C02 opcode matrix -- <http://www.oxyron.de/html/opcodes02.html>

Both are published without stated reuse terms, so neither may be copied.
That a given opcode takes a given number of cycles is a fact about the part
and belongs to nobody; the tables above are Casso's own statement of it.
