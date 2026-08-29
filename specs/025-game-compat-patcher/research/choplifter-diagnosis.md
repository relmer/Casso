# Choplifter protection — full diagnosis

Source: this was diagnosed from live `--trace` captures during the session that
created this spec. Mirrors the project memory `project-choplifter-enhanced-2e-reboot`.

## Symptom

On the Enhanced //e and the //c, Choplifter (`Apple2/Demos/Choplifter.woz`,
main checkout) cycles forever between its title screen and a near-black text
screen showing a single **"M"** at `$0400`, ~15s period. It works on the ][+
and the unenhanced //e.

## It is the system ROM, not the CPU

The built-in machines confound CPU and ROM (the 6502 machines ship the older
ROMs, the 65C02 machines the newer). Isolated by temporarily rewriting `"cpu"`
in the *deployed* machine JSON (`%LOCALAPPDATA%\Casso\Machines\<Name>\<Name>.json`)
and restoring after:

| | 6502 | 65C02 |
|---|---|---|
| plain //e ROM | works | works |
| Enhanced //e ROM | cycles | cycles |

Both ROM images are genuine (correct `$FBB3`/`$FBC0` signatures, 16 KB, reset
vector `$FA62`).

## The protection is a monitor-ROM integrity check

Fully mapped from the trace (all addresses post-relocation, where the code runs):

1. **Requires reset vector == `$FA62`** (`$62A8 CPY #$62 / EOR #$FA / BNE fail`).
2. **`$6382 LDA $FBB5 / EOR #$78`** selects which stored expected-ROM image to
   verify against: `$78` picks the plain-//e table, anything else picks the
   other. `$FBB5` is a machine-ID byte:
   - plain //e = `$78`, Enhanced //e = `$15`, //c = `$00`
   - (matches exactly which machines fail)
3. A record copier at `$6380-$63A8` builds the expected image near `$6000` from
   tables in the `$63xx` blob; each record header is `[addr_lo][addr_hi][len]`.
4. A generic verifier at **`$62BD-$62E1`** compares the stored image `($00)`
   against LIVE ROM `($02)`, region by region, Y descending, terminated by an
   `addr==$0000` record:
   ```
   $62DA  LDA ($00),Y     ; stored expected byte
   $62DC  CMP ($02),Y     ; live ROM byte
   $62DE  BNE $62F4        ; mismatch -> tamper response
   $62E0  DEY
   $62E1  BPL $62DA
   ```
5. **Twelve regions, ~257 bytes total** are verified, three full passes per
   boot cycle:
   `$FA62`x57 `$FAA3`x3 `$FB2F`x17 `$FB4B`x21 `$FBC1`x47 `$FBFD`x19 `$FC22`x10
   `$FCA8`x12 `$FDED`x12 `$FE84`x42 `$FF3A`x5 `$FBB4`x12
   (these are the reset routine, INIT, VTAB, WAIT, COUT1, SETVID/SETKBD, BELL,
   the machine-ID routine — every I/O hook point).
6. Any mismatch jumps to `$62F4 -> $032F`: stamp `$CD` ('M' high-ASCII) at
   `$0205`, wipe the text page with `$A0`, compute `$C6` from the slot byte,
   write `$C600` into `$03F2/$03F3` and into `$FFFC/$FFFD` (language-card RAM
   via `$C083`), then `$03A0 JMP $C600` — a deliberate anti-tamper reboot. The
   healthy run executes ZERO instructions in `$0390-$03A2`.

## Why patching one byte does not help

The Enhanced //e ROM differs from the plain //e ROM in **40 of the 257 verified
bytes**, across 6 of the 12 regions (`$FA76`, `$FB52`, `$FC24-2B`, `$FDF0-F8`,
`$FEA0+`, `$FBB4-BF`). Neither stored table matches. Patching `$FBB5` alone,
or `$FBB5`+`$FA76`, just moves the failure to the next differing region.
"Passing" the check by substituting bytes would mean running 40 bytes of the
plain ROM — i.e. running the plain ROM. Real Enhanced //e hardware reads the
same 40 bytes and fails identically. **This is faithful emulation of a genuine
by-design incompatibility** (same category as Space Quarks GH #99).

## The defuse

Do not patch the ROM or the compared data. Defeat the *check*. Two options:

1. **NOP the mismatch branch** (Casso-traced): at the verifier, replace the
   `BNE` (`D0 14`) with `EA EA` so a mismatch is ignored and the verify always
   "passes."
   - Signature (verifier body): `B1 00 D1 02 D0 14 88 10 F7`
     (`LDA ($00),Y / CMP ($02),Y / BNE +$14 / DEY / BPL`)
     — replace at offset 4 (`D0 14`) with `EA EA`.
2. **RTS the check's caller** (anti-m's choice — see `antim-reference.md`):
   store `$60` at the call site so the whole verify routine never runs.

Prefer the Casso-traced signature; it is grounded in our own trace and is a
minimal, specific match. Validate whichever is chosen against the
self-checksum hazard (see `antim-reference.md`).

## Reproduction / validation tooling

- Boot: `Casso.exe --machine Apple2eEnhanced --disk1 <repo>\Apple2\Demos\Choplifter.woz`
- Detect the blank+"M" state by lit-pixel count on the raw framebuffer via
  `IDM_EDIT_COPY_SCREENSHOT` (40006) clipboard capture (STA runspace). The
  blank+"M" frame has ~72 non-black pixels in a 10x14 box at top-left; a live
  title/attract screen is hundreds+.
- `--trace <N>` dumps on graceful exit (WM_CLOSE, not kill) to the process cwd.
  Diff two machines' traces by executed-PC set, never instruction-by-instruction
  (the first raw divergence is always a disk-latch poll = rotational phase).
- NOTE: trace operand bytes were only made bank-correct in Casso 1.18.2
  (`fix(trace): read operand bytes from the bank that executed`). Traces from
  before that are misleading on banked memory.
