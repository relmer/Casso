# Apple /// Business BASIC programs

Apple's Business BASIC ran on the Apple ///. It saves a program tokenized,
ProDOS file type $09 (BA3). Casso's `BusinessBasic` lists one as text. This
document is the specification that code is written from.

Sources of the facts: CiderPress II's BASIC notes
(`FileConv/Code/BASIC-notes.md`) for the layout, and CiderPress II's lister
for the keyword tables. Only facts are taken: the layout and the keyword of
each token.

## Layout

All numbers are little-endian.

```
+$00 / 2: the file's length, not counting this word; some files have 0
+$02 /nn: lines
+$xx / 2: $0000, missing from some files
```

Each line:

```
+$00 / 1: the offset from this byte to the next line
+$01 / 2: the line number
+$03 /nn: literal characters and tokens
+$xx / 1: $00, the end of the line
```

A line whose offset byte is 0 ends the program.

A byte below $80 is a literal character. Numbers are stored as their
characters. A byte of $80 or above is a keyword token from the first table.
$FF is an extended token: the next byte's low 7 bits pick its keyword from
the second table. A blank entry is not a keyword.

After `REM` ($C0), the rest of the line is the remark's text.

## Tables

### Keywords, tokens $80 to $FF

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $80 | `END` | `FOR` | `NEXT` | `INPUT` | `OUTPUT` | `DIM` | `READ` | `WRITE` |
| $88 | `OPEN` | `CLOSE` | - | `TEXT` | - | `BYE` | - | - |
| $90 | - | - | - | `WINDOW` | `INVOKE` | `PERFORM` | - | - |
| $98 | `FRE` | `HPOS` | `VPOS` | `ERRLIN` | `ERR` | `KBD` | `EOF` | `TIME$` |
| $A0 | `DATE$` | `PREFIX$` | `EXFN.` | `EXFN%.` | `OUTREC` | `INDENT` | - | - |
| $A8 | - | - | - | - | - | `POP` | `HOME` | - |
| $B0 | `SUB$(` | `OFF` | `TRACE` | `NOTRACE` | `NORMAL` | `INVERSE` | `SCALE(` | `RESUME` |
| $B8 | - | `LET` | `GOTO` | `IF` | `RESTORE` | `SWAP` | `GOSUB` | `RETURN` |
| $C0 | `REM` | `STOP` | `ON` | - | `LOAD` | `SAVE` | `DELETE` | `RUN` |
| $C8 | `RENAME` | `LOCK` | `UNLOCK` | `CREATE` | `EXEC` | `CHAIN` | - | - |
| $D0 | - | `CATALOG` | - | - | `DATA` | `IMAGE` | `CAT` | `DEF` |
| $D8 | - | `PRINT` | `DEL` | `ELSE` | `CONT` | `LIST` | `CLEAR` | `GET` |
| $E0 | `NEW` | `TAB` | `TO` | `SPC(` | `USING` | `THEN` | - | `MOD` |
| $E8 | `STEP` | `AND` | `OR` | `EXTENSION` | `DIV` | - | `FN` | `NOT` |
| $F0 | - | - | - | - | - | - | - | - |
| $F8 | - | - | - | - | - | - | - | `*extend*` |

### Extended keywords, the byte after $FF, $80 to $FF

| | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
|---|---|---|---|---|---|---|---|---|
| $80 | `TAB(` | `TO` | `SPC(` | `USING` | `THEN` | - | `MOD` | `STEP` |
| $88 | `AND` | `OR` | `EXTENSION` | `DIV` | - | `FN` | `NOT` | - |
| $90 | - | - | - | - | - | - | - | - |
| $98 | - | - | - | - | `AS` | `SGN(` | `INT(` | `ABS(` |
| $A0 | - | `TYP(` | `REC(` | - | - | - | - | - |
| $A8 | - | - | - | - | - | `PDL(` | `BUTTON(` | `SQR(` |
| $B0 | `RND(` | `LOG(` | `EXP(` | `COS(` | `SIN(` | `TAN(` | `ATN(` | - |
| $B8 | - | - | - | - | - | - | - | - |
| $C0 | - | - | - | `STR$(` | `HEX$(` | `CHR$(` | `LEN(` | `VAL(` |
| $C8 | `ASC(` | `TEN(` | - | - | `CONV(` | `CONV&(` | `CONV$(` | `CONV%(` |
| $D0 | `LEFT$(` | `RIGHT$(` | `MID$(` | `INSTR(` | - | - | - | - |
| $D8 | - | - | - | - | - | - | - | - |
| $E0 | - | - | - | - | - | - | - | - |
| $E8 | - | - | - | - | - | - | - | - |
| $F0 | - | - | - | - | - | - | - | - |
| $F8 | - | - | - | - | - | - | - | - |

