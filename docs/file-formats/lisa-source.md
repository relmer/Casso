# LISA source files

Lazer's Interactive Symbolic Assembler (LISA), by Randall Hyde, saves its
source in a tokenized form. Casso's `LisaSource` converts the three forms to a
text listing. This document is the specification that code is written from.

Sources of the facts: CiderPress II's LISA notes (`FileConv/Code/LisaAsm-notes.md`)
for the file types and the v2 layout, and CiderPress II's converter for the
v3 and v4/v5 token encodings, which aren't in any other public document.
Only facts are taken: byte layouts, token values and the text of each token.
Tables are data of the format.

All multi-byte numbers are little-endian. "Rest" means the bytes of the line
not yet read.

## File types

| Version | File | Recognized when |
|---|---|---|
| v2 | DOS 3.3 alternate B (ProDOS $F4) | at least 6 bytes; the data length word at +2 is at least 6 and no more than the file |
| v3 | ProDOS INT ($FA), aux type $1000 to $3FFF | the symbol table length is a multiple of 4, no more than 4096 and no more than the file, and 4 + code length + symbol table length is no more than the file |
| v4/v5 | ProDOS INT ($FA), aux type $4000 to $5FFF | the symbol table end is nonzero and no more than the file; the symbol count times 2 is no more than the symbol table end; the three tab columns are 1 to 127, 2 to 127 and 3 to 127 |

## v2

```
+$00 / 2: version (usually $1800)
+$02 / 2: offset just past the last line's data, from the start of the file
+$04 /nn: lines
```

Each line is a length byte, then that many bytes, the last of which is $0D.
A length of $FF marks the end. A length of 0 is invalid and ends the listing.
If the $FF comes with more than 2 bytes left before the data end, the file
ended early.

Columns: label 0, mnemonic 9, operand 13, comment 29.

A line's data:

1. A first byte of `*` or `;`: the whole line is a comment. Its text is the
   bytes before the $0D.
2. A first byte of `A` to `Z` or `^`: a label. Its bytes run to the first byte
   that is $0D or $80 and above. Spaces in it are padding and are dropped; a
   trailing colon is printed. A line whose label is followed by $0D is the
   label alone.
3. Any other byte below $80 at the start is invalid.
4. The mnemonic token, $80 and above, at the mnemonic column. LISA 2.5's
   WRITE command puts a space after every mnemonic, and the listing does too.
5. The operand kind byte. $00 is no operand. $01 to $20 is followed by the
   operand's text, which runs to the first byte that is $0D or $80 and above.
6. $0D ends the line. $BB (a high-bit semicolon) starts a comment: `;` at the
   comment column, then the bytes before the final $0D. Anything else is
   invalid.

## v3

```
+$00 / 2: code length
+$02 / 2: symbol table length
+$04 /nn: symbol table, 8 bytes an entry, at most 512 entries
+$xx /yy: code, the code length in bytes
```

A symbol table entry is 6 bytes of packed name and a 2-byte value that the
listing doesn't use. The 6 bytes are eight 6-bit characters, high bits
first: the first character is the top 6 bits of byte 0, the second the low
2 bits of byte 0 and the top 4 of byte 1, the third the low 4 bits of byte 1
and the top 2 of byte 2, the fourth the low 6 of byte 2, and bytes 3 to 5
likewise for characters 5 to 8. A character of $20 ends the name. One above
$20 is that character; one below $20 is that value plus $40.

Columns: label 0, mnemonic 9, operand 18, comment 40.

Symbol references are a token and an index byte. The token's low bit is bit 8
of the index.

### Lines

A line starts with a flag byte F:

| F | Line |
|---|---|
| $00 | end of the code |
| $01 to $7F | a long line, F bytes including F; see below |
| $80 to $EF | a mnemonic alone, at the mnemonic column |
| $F0 to $F9 | a local label alone: `^`, the digit F - $F0, `:` |
| $FA, $FB | a label alone: the symbol, then `:`; one index byte follows |
| $FC, $FD | a macro call alone: `/` and the symbol at the mnemonic column; one index byte follows |
| $FE | `*`, an empty comment line |
| $FF | `;`, an empty comment line |

A long line's first byte:

| Byte | Meaning |
|---|---|
| $FE, $FF | a comment line: `*` or `;`, then the rest with bit 7 cleared |
| $FC, $FD | a macro call: `/` and the symbol (one index byte) at the mnemonic column; the operand follows, with $FC or $FD as the mnemonic |
| $FA, $FB | a label: the symbol (one index byte) at column 0, no colon; the mnemonic follows |
| $F0 to $F9 | a local label: `^` and the digit; the mnemonic follows |
| other | the mnemonic, with no label |

The mnemonic byte M: $FC or $FD is a macro call as above. Anything else is
the mnemonic table's entry for M at the mnemonic column; a blank entry is an
unknown mnemonic.

### Operand

If M is $FE or above, the rest is a comment.

An address mode byte A comes first when M is below $B0 and is below $11 or
from $29 to $33. Then, at the operand column, `(` when A is $10 to $7F.

The operand alternates terms and operators:

**Term**, a byte V:

| V | Text |
|---|---|
| $0E | `~`, then another term |
| $0F | `-`, then another term |
| $3A | `#`, then another term |
| $3B | `/`, then another term |
| $3D | `@`, then another term |
| $00 to $19 (other than $0E, $0F) | the character $30 + V |
| $1A | the next byte in decimal |
| $1B | the next word in decimal |
| $1C | `$` and the next byte in 2 hex digits |
| $1D | `$` and the next word in 4 hex digits |
| $1E | `%` and the next byte's 8 bits, high bit first |
| $1F | `%` and the next 2 bytes' bits, each high bit first, in the order they are stored |
| $36, $37 | `#` and a symbol (one index byte) |
| $38, $39 | `/` and a symbol (one index byte) |
| $3C | `*`, the location counter |
| $20 to $35, $3E to $49 | `<` and the character V - $10 (`<0` to `<9` for $40 to $49) |
| $4A to $4F | `?` and the digit V - $4A (`?0` to `?5`) |
| $50 to $59 | `>` and the digit V - $50 |
| $5A to $5F | `?` and the character V - $24: `?6` to `?9`, `?:` for $5E and `?#` for $5F; `?:` is followed by another term |
| $60 to $7F | a string; see below |
| $FA, $FB | a symbol (one index byte) |
| $FF | the rest is a comment |
| other | nothing |

A string's length is V's low 5 bits, or the next byte when those are 0. A
length past the line's end is an error. The delimiter is `"` when the first
character byte has bit 7 set and `'` otherwise. Each character is printed
with bit 7 cleared, and a delimiter inside the string is doubled.

**Operator**, a byte O, read after a term when the line has bytes left:

| O | Text |
|---|---|
| $00 to $0D | a space, the operator, a space; then a term. The operators, in order: `+ - * / & \| ^ = < > % <= >= <>` |
| $20 to $2F | `+`, then the term O - $10; then another operator |
| $FF | the rest is a comment |
| other | `,`; O is read again as a term |

When the operand ends, the address mode's closing text: A = $04 `,X`, $08
`,Y`, $10 `)`, $20 `,X)`, $40 `),Y`. Then, if bytes are left, `;` at the
comment column and the rest with bit 7 cleared.

## v4 and v5

```
+$00 / 2: version
+$02 / 2: offset just past the symbol table; the code starts here
+$04 / 2: symbol count
+$06 / 1: mnemonic column
+$07 / 1: operand column
+$08 / 1: comment column
+$09 / 1: CPU
+$0A / 6: reserved
+$10 /nn: symbol table
```

Each symbol is a length byte, which the listing skips, then its characters
to a $00. A character below $80 has bit 5 set; every character is printed
with bit 7 cleared. A symbol missing because the table ended is an error.

The code runs to the end of the file. Symbol references are a 2-byte index.

### Lines

| F | Line |
|---|---|
| $00 | end of the code |
| $01 to $7F | a long line, F bytes including F |
| $80 to $EF | a mnemonic alone, at the mnemonic column |
| $F0 to $F9 | a local label alone: `^` and the digit F - $F0 |
| $FA | a label alone: the symbol (2-byte index) |
| $FB | unknown |
| $FC | a macro call alone: `_` and the symbol (2-byte index) at the mnemonic column |
| $FD | a blank line |
| $FE | `*`, an empty comment line |
| $FF | `;`, an empty comment line |

A long line's first byte:

| Byte | Meaning |
|---|---|
| $FD, $FE, $FF | a full line: `!`, `*` or `;`, then the rest with bit 7 cleared |
| $FC | a macro call: `_` and the symbol (2-byte index) at the mnemonic column; the operand follows, with $FC as the mnemonic |
| $FA | a label: the symbol (2-byte index) at column 0; the mnemonic follows |
| $F0 to $F9, $FB | a local label: `^` and the character byte - $C0; the mnemonic follows |
| other | the mnemonic, with no label |

The mnemonic byte M: $FC and above is a macro call, `_` and the symbol
(2-byte index). Otherwise the table's entry for M, or an error for a blank
one. A mnemonic of $90 or above has no operand: if bytes are left, one byte
(the comment token) is skipped and the rest is the comment.

### Operand

Only for a macro call and for M below $90. At the operand column:

An address mode byte A comes first, for a mnemonic other than a macro call,
when M is below $12 or from $30 to $40. A of 16 or more is an error. The
opening text: A = 9 to 12 `(`, 13 and 14 `[`.

**Term**, a byte V:

- $FA: a symbol (2-byte index), then an operator.
- $FF: the operand ends.
- Other values of $80 and above: `!,` and another term.
- Below $80, with an entry in the term table's first column: that
  character. Then the second column: `!` means a unary operator, so another
  term follows; another character is printed, then an operator; blank, an
  operator.
- $33: a long value. The next byte picks it: 0, a 4-byte decimal; 1, `$` and
  4 bytes of hex; 2, `%` and 4 bytes of binary; 3, a hex string, a count
  byte and that many bytes each as 2 hex digits; 4, a plain string, a count
  byte and that many characters with bit 7 cleared. Any other is an error,
  as is a count past the line. Then an operator.
- $1A, $1B, $34: a 1, 2 or 3-byte decimal value. $1C, $1D, $35: `$` and 1, 2
  or 3 bytes of hex. $1E, $1F, $36: `%` and 1, 2 or 3 bytes of binary. Then
  an operator.
- $37, $38: an optional $FA, then a symbol (2-byte index), then `:A` for
  $37 or `:L` for $38. Then an operator.
- $5E: `?:`, then another term.
- Any other: a string, as in v3. Then an operator.

Hex and binary values print their most significant byte first, each binary
byte high bit first. Hex digits are uppercase.

**Operator**, a byte O, when the line has bytes left:

- $FF: the operand ends.
- Other values of $80 and above: `,`, and O is read again as a term.
- Below $80, the operator table's entry for O:
  - blank: `,`, and O is read again as a term;
  - `!`: `+`, then the term table's second column for O when it has one
    (`+0` to `+9`), followed by another operator; otherwise the numeric term
    O - $10, followed by another operator;
  - a character: a space, that character, the term table's second column
    for O when it has one, and a space; then a term.

When the operand ends: the address mode's closing text, A = 3 to 5 `,X`, 7
`,S`, 8 `,Y`, 9 `),Y`, 10 `,X)`, 11 `)`, 12 `,S),Y`, 13 `]`, 14 `],Y`. Then,
if bytes are left, `;` at the comment column and the rest with bit 7 cleared.

## Tables

A `-` is a blank entry. A `·` is a space.

### v2 mnemonics, tokens $80 to $FF

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $80 | `BGE` | `BLT` | `BMI` | `BCC` | `BCS` | `BPL` | `BNE` | `BEQ` |
| $88 | `BVS` | `BVC` | `BSB` | `BNM` | `BM1` | `BNZ` | `BIZ` | `BIM` |
| $90 | `BIP` | `BIC` | `BNC` | `BRA` | `BTR` | `BFL` | `BRK` | `BKS` |
| $98 | `CLV` | `CLC` | `CLD` | `CLI` | `DEX` | `DEY` | `INX` | `INY` |
| $A0 | `NOP` | `PHA` | `PLA` | `PHP` | `PLP` | `RTS` | `RTI` | `RSB` |
| $A8 | `RTN` | `SEC` | `SEI` | `SED` | `TAX` | `TAY` | `TSX` | `TXA` |
| $B0 | `TXS` | `TYA` | `ADD` | `CPR` | `DCR` | `INR` | `SUB` | `LDD` |
| $B8 | `POP` | `PPD` | `STD` | `STP` | `LDR` | `STO` | `SET` | - |
| $C0 | `ADC` | `AND` | `ORA` | `BIT` | `CMP` | `CPX` | `CPY` | `DEC` |
| $C8 | `EOR` | `INC` | `JMP` | `JSR` | - | `LDA` | `LDX` | `LDY` |
| $D0 | `STA` | `STX` | `STY` | `XOR` | `LSR` | `ROR` | `ROL` | `ASL` |
| $D8 | `ADR` | `EQU` | `ORG` | `OBJ` | `EPZ` | `STR` | `DCM` | `ASC` |
| $E0 | `ICL` | `END` | `LST` | `NLS` | `HEX` | `BYT` | `HBY` | `PAU` |
| $E8 | `DFS` | `DCI` | `...` | `PAG` | `INV` | `BLK` | `DBY` | `TTL` |
| $F0 | `SBC` | - | `LET` | `.IF` | `.EL` | `.FI` | `=··` | `PHS` |
| $F8 | `DPH` | `.DA` | `GEN` | `NOG` | `USR` | - | - | - |

### v3 mnemonics, tokens $00 to $FF

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $00 | `add` | `adc` | `and` | `cmp` | `eor` | `lda` | `ora` | `sbc` |
| $08 | `sta` | `sub` | `xor` | `asl` | `dec` | `inc` | `lsr` | `rol` |
| $10 | `ror` | `.if` | `whl` | `bra` | `bcc` | `bcs` | `beq` | `bfl` |
| $18 | `bge` | `blt` | `bmi` | `bne` | `bpl` | `btr` | `bvc` | `bvs` |
| $20 | `jsr` | `obj` | `org` | `phs` | `.md` | `fzr` | `inp` | `lcl` |
| $28 | `rls` | `bit` | `cpx` | `cpy` | `jmp` | `ldx` | `ldy` | `stx` |
| $30 | `sty` | `trb` | `tsb` | `stz` | `=··` | `con` | `epz` | `equ` |
| $38 | `set` | `.da` | `adr` | `byt` | `csp` | `dby` | `hby` | `anx` |
| $40 | `sbt` | `ttl` | `chn` | `blk` | `dci` | `inv` | `rvs` | `msg` |
| $48 | `str` | `zro` | `dfs` | `hex` | `usr` | `sav` | - | - |
| $50 | - | - | - | - | - | - | - | - |
| $58 | - | - | - | - | - | - | - | - |
| $60 | - | - | - | - | - | - | - | - |
| $68 | - | - | - | - | - | - | - | - |
| $70 | - | - | - | - | - | - | - | - |
| $78 | - | - | - | - | - | - | - | - |
| $80 | - | - | - | - | - | - | - | - |
| $88 | - | - | - | - | - | - | - | - |
| $90 | - | - | - | - | - | - | - | - |
| $98 | - | - | - | - | - | - | - | - |
| $A0 | - | - | - | - | - | - | - | - |
| $A8 | - | - | - | - | - | - | - | - |
| $B0 | `.el` | `.fi` | `.me` | `.we` | `dph` | `if1` | `if2` | `end` |
| $B8 | `exp` | `gen` | `lst` | `nls` | `nog` | `nox` | `pag` | `pau` |
| $C0 | `nlc` | `cnd` | `···` | `asl` | `lsr` | `rol` | `ror` | `dec` |
| $C8 | `inc` | `brk` | `clc` | `cld` | `cli` | `clv` | `dex` | `dey` |
| $D0 | `inx` | `iny` | `nop` | `pha` | `php` | `pla` | `plp` | `rti` |
| $D8 | `rts` | `sec` | `sed` | `sei` | `tax` | `tay` | `tsx` | `txa` |
| $E0 | `txs` | `tya` | `phx` | `phy` | `plx` | `ply` | - | - |
| $E8 | - | - | - | - | - | - | - | - |
| $F0 | - | - | - | - | - | - | - | - |
| $F8 | - | - | - | - | - | - | - | - |

### v4 and v5 mnemonics, tokens $00 to $FF

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $00 | `???` | `add` | `adc` | `and` | `cmp` | `eor` | `lda` | `ora` |
| $08 | `sbc` | `sta` | `sub` | `xor` | `asl` | `dec` | `inc` | `lsr` |
| $10 | `rol` | `ror` | `.if` | `whl` | `.go` | `bra` | `bcc` | `bcs` |
| $18 | `beq` | `bfl` | `bge` | `blt` | `bmi` | `bne` | `bpl` | `btr` |
| $20 | `bvc` | `bvs` | `obj` | `org` | `phs` | `.db` | `pea` | `per` |
| $28 | `brl` | `.md` | `far` | `fdr` | `fzr` | `inp` | `lcl` | `rls` |
| $30 | `bit` | `cpx` | `cpy` | `ldx` | `ldy` | `stx` | `sty` | `trb` |
| $38 | `tsb` | `stz` | `pei` | `rep` | `sep` | `jmp` | `jsr` | `jml` |
| $40 | `jsl` | `mvn` | `mvp` | `=·` | `con` | `epd` | `epz` | `eql` |
| $48 | `equ` | `set` | `.da` | `adr` | `byt` | `csp` | `dby` | `hby` |
| $50 | `bby` | `anx` | `chn` | `icl` | `lib` | `lnk` | `msg` | `psm` |
| $58 | `rlb` | `sbt` | `ttl` | `dci` | `rvs` | `str` | `zro` | `dfs` |
| $60 | `hex` | `usr` | `sav` | `.tf` | `seg` | `cpu` | `.entry` | `.ref` |
| $68 | `.group` | `.deref` | `long` | - | `.assume` | - | - | - |
| $70 | - | - | - | - | - | - | - | - |
| $78 | - | - | - | - | - | - | - | `.table` |
| $80 | - | - | - | - | - | - | - | - |
| $88 | - | - | - | - | - | - | - | - |
| $90 | `.el` | `.fi` | `.me` | `.we` | `.la` | `.lx` | `.sa` | `.sx` |
| $98 | `dph` | `if1` | `if2` | `end` | `exp` | `gen` | `lst` | `nls` |
| $A0 | `nog` | `nox` | `pag` | `pau` | `nlc` | `cnd` | `asl` | `lsr` |
| $A8 | `rol` | `ror` | `dec` | `inc` | `mvn` | `mvp` | `brk` | `clc` |
| $B0 | `cld` | `cli` | `clv` | `dex` | `dey` | `inx` | `iny` | `nop` |
| $B8 | `pha` | `php` | `pla` | `plp` | `rti` | `rts` | `sec` | `sed` |
| $C0 | `sei` | `tax` | `tay` | `tsx` | `txa` | `txs` | `tya` | `phx` |
| $C8 | `phy` | `plx` | `ply` | `cop` | `phb` | `phd` | `phk` | `plb` |
| $D0 | `pld` | `rtl` | `stp` | `swa` | `tad` | `tas` | `tcd` | `tcs` |
| $D8 | `tda` | `tdc` | `tsa` | `tsc` | `txy` | `tyx` | `wai` | `xba` |
| $E0 | `xce` | `.proc` | `.endp` | `.table` | `.endt` | - | - | - |
| $E8 | - | - | - | - | - | - | - | - |
| $F0 | - | - | - | - | - | - | - | - |
| $F8 | - | - | - | - | - | - | - | - |

### v4 term table, first column

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $00 | `+` | `-` | `*` | `/` | `&` | `\|` | `^` | `=` |
| $08 | `<` | `>` | `%` | `<` | `>` | `<` | `~` | `-` |
| $10 | `0` | `1` | `2` | `3` | `4` | `5` | `6` | `7` |
| $18 | `8` | `9` | - | - | - | - | - | - |
| $20 | `+` | `+` | `+` | `+` | `+` | `+` | `+` | `+` |
| $28 | `+` | `+` | - | - | - | - | - | - |
| $30 | - | `<` | `>` | - | - | - | - | - |
| $38 | - | `*` | `@` | `#` | `/` | `^` | `\|` | `\` |
| $40 | `<` | `<` | `<` | `<` | `<` | `<` | `<` | `<` |
| $48 | `<` | `<` | `?` | `?` | `?` | `?` | `?` | `?` |
| $50 | `>` | `>` | `>` | `>` | `>` | `>` | `>` | `>` |
| $58 | `>` | `>` | `?` | `?` | `?` | `?` | - | `?` |
| $60 | - | - | - | - | - | - | - | - |
| $68 | - | - | - | - | - | - | - | - |
| $70 | - | - | - | - | - | - | - | - |
| $78 | - | - | - | - | - | - | - | - |

### v4 term table, second column; also the operators' second character

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $00 | - | - | - | - | - | - | - | - |
| $08 | - | - | - | `=` | `=` | `>` | `!` | `!` |
| $10 | - | - | - | - | - | - | - | - |
| $18 | - | - | - | - | - | - | - | - |
| $20 | `0` | `1` | `2` | `3` | `4` | `5` | `6` | `7` |
| $28 | `8` | `9` | - | - | - | - | - | - |
| $30 | - | `<` | `>` | - | - | - | - | - |
| $38 | - | - | `!` | `!` | `!` | `!` | `!` | `!` |
| $40 | `0` | `1` | `2` | `3` | `4` | `5` | `6` | `7` |
| $48 | `8` | `9` | `0` | `1` | `2` | `3` | `4` | `5` |
| $50 | `0` | `1` | `2` | `3` | `4` | `5` | `6` | `7` |
| $58 | `8` | `9` | `6` | `7` | `8` | `9` | - | `#` |
| $60 | - | - | - | - | - | - | - | - |
| $68 | - | - | - | - | - | - | - | - |
| $70 | - | - | - | - | - | - | - | - |
| $78 | - | - | - | - | - | - | - | - |

### v4 operator table

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $00 | `+` | `-` | `*` | `/` | `&` | `\|` | `^` | `=` |
| $08 | `<` | `>` | `%` | `<` | `>` | `<` | - | - |
| $10 | - | - | - | - | - | - | - | - |
| $18 | - | - | - | - | - | - | - | - |
| $20 | `!` | `!` | `!` | `!` | `!` | `!` | `!` | `!` |
| $28 | `!` | `!` | `!` | `!` | `!` | `!` | `!` | `!` |
| $30 | - | `<` | `>` | - | - | - | - | - |
| $38 | - | - | - | - | - | - | - | - |
| $40 | - | - | - | - | - | - | - | - |
| $48 | - | - | - | - | - | - | - | - |
| $50 | - | - | - | - | - | - | - | - |
| $58 | - | - | - | - | - | - | - | - |
| $60 | - | - | - | - | - | - | - | - |
| $68 | - | - | - | - | - | - | - | - |
| $70 | - | - | - | - | - | - | - | - |
| $78 | - | - | - | - | - | - | - | - |

