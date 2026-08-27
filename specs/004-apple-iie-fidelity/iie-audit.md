# Apple //e Emulator Fidelity Audit: Casso vs. Real Hardware

> _Historical (2025-07). This audit drove the //e-fidelity feature in this
> directory (spec 004); all of its CRITICAL/MAJOR findings are implemented, and
> the code cites it by section (`audit §N`, `audit C7`, …). Class and file names
> below predate the later rewrites, e.g. `AuxRamCard` → `Apple2eMmu`,
> `AppleIIeKeyboard` → `Apple2eKeyboard`, `EmulatorShell::RebuildBankingPages` →
> `Apple2eMmu::RebindPageTable`. Kept as this spec's requirements reference, not
> a live action list._

**Scope:** Casso at `C:\Users\relmer\source\repos\relmer\Casso\`  
**Reference revision:** AppleWin `master` SHA `65d0467c` (Memory.cpp) / `2904816e` (LanguageCard.cpp)  
**Date:** 2025-07

---

## Table of Contents

1. [Memory Bus & Soft Switches](#1-memory-bus--soft-switches)
2. [Auxiliary Memory / 80STORE Banking](#2-auxiliary-memory--80store-banking)
3. [Language Card](#3-language-card)
4. [Keyboard](#4-keyboard)
5. [Speaker](#5-speaker)
6. [Video / Character ROM](#6-video--character-rom)
7. [Disk II Controller](#7-disk-ii-controller)
8. [ROM Mapping](#8-rom-mapping)
9. [CPU](#9-cpu)
10. [Reset Semantics](#10-reset-semantics)
11. [Prioritized Action List](#11-prioritized-action-list)

---

## 1. Memory Bus & Soft Switches

### 1.1 Write Switches $C000–$C00F

#### Real //e Behavior

All 16 addresses respond to **writes** (and on the //e, reads at $C00C–$C00F also trigger the switch; see Sather UTAIIe §5-18). The full table (Apple //e Tech Ref Table 4-1, AppleWin `Memory.cpp:1050–1100`):

| Address | Name | Effect |
|---------|------|--------|
| $C000 W | 80STOREOFF | PAGE2 selects video page (normal mode) |
| $C001 W | 80STOREON  | PAGE2 selects aux/main memory |
| $C002 W | RAMRDOFF   | Reads $0200–$BFFF from main |
| $C003 W | RAMRDON    | Reads $0200–$BFFF from aux |
| $C004 W | RAMWRTOFF  | Writes $0200–$BFFF to main |
| $C005 W | RAMWRTON   | Writes $0200–$BFFF to aux |
| $C006 W | INTCXROMOFF | Slot ROMs active $C100–$C7FF |
| $C007 W | INTCXROMON  | Internal ROM active $C100–$CFFF |
| $C008 W | ALTZPOFF   | ZP/stack from main $0000–$01FF |
| $C009 W | ALTZPON    | ZP/stack from aux $0000–$01FF |
| $C00A W | SLOTC3ROMOFF | Internal ROM at $C300–$C3FF |
| $C00B W | SLOTC3ROMON  | Slot 3 ROM at $C300–$C3FF |
| $C00C W/R | 80COLOFF    | Disable 80-col display |
| $C00D W/R | 80COLON     | Enable 80-col display |
| $C00E W/R | ALTCHARSETOFF | Primary character set |
| $C00F W/R | ALTCHARSETTON | Alternate character set |

`$C000`/`$C001` are **write-only** (reads return keyboard data). `$C002`–`$C00B` are **write-only**. `$C00C`–`$C00F` respond to both reads and writes.

#### AppleWin Implementation

`Memory.cpp` `MemSetPaging()` dispatch (case on `addr & 0x0F`):
```
case 0x00: SetMemMode(g_memmode & ~MF_80STORE)   // $C000
case 0x01: SetMemMode(g_memmode |  MF_80STORE)   // $C001
case 0x02: SetMemMode(g_memmode & ~MF_AUXREAD)   // $C002 RAMRDOFF
case 0x03: SetMemMode(g_memmode |  MF_AUXREAD)   // $C003 RAMRDON
case 0x04: SetMemMode(g_memmode & ~MF_AUXWRITE)  // $C004 RAMWRTOFF
case 0x05: SetMemMode(g_memmode |  MF_AUXWRITE)  // $C005 RAMWRTON
case 0x06: SetMemMode(g_memmode & ~MF_INTCXROM)  // $C006
case 0x07: SetMemMode(g_memmode |  MF_INTCXROM)  // $C007
case 0x08: SetMemMode(g_memmode & ~MF_ALTZP)     // $C008
case 0x09: SetMemMode(g_memmode |  MF_ALTZP)     // $C009
case 0x0A: SetMemMode(g_memmode & ~MF_SLOTC3ROM) // $C00A
case 0x0B: SetMemMode(g_memmode |  MF_SLOTC3ROM) // $C00B
```

#### Casso Implementation

- `AppleIIeSoftSwitchBank` covers `$C000`–`$C00F` (via `AppleIIeKeyboard::Write()` forwarding and its own range `$C050`–`$C07F`).
- `$C000`/`$C001` → **80STORE** implemented correctly (`AppleIIeSoftSwitchBank.cpp:107–118`).
- `$C00C`–`$C00F` → **80COL / ALTCHARSET** implemented correctly (`AppleIIeSoftSwitchBank.cpp:41–51`).
- **`AuxRamCard`** covers `$C003`–`$C006` for RAMRD/RAMWRT (`AuxRamCard.h:28–29`).

**`AuxRamCard` address-assignment bug** (`AuxRamCard.cpp:32–46`):
```cpp
case 0xC003: m_readAux = true;   break;   // OK: RAMRDON
case 0xC004: m_readAux = false;  break;   // WRONG: $C004 is RAMWRTOFF, not RAMRDOFF
case 0xC005: m_writeAux = true;  break;   // OK: RAMWRTON
case 0xC006: m_writeAux = false; break;   // WRONG: $C006 is INTCXROMOFF, not RAMWRTOFF
```

**Missing switches**, no device handles:
- `$C002` (RAMRDOFF), RAMRD can be turned **on** but never **off**
- `$C006`/`$C007` (INTCXROM)
- `$C008`/`$C009` (ALTZP)
- `$C00A`/`$C00B` (SLOTC3ROM)

#### Gaps / Bugs

- **[CRITICAL]** `$C002` (RAMRDOFF) not mapped to any device. RAMRD cannot be cleared via software; it can only be turned on (`$C003`) but never off. Source: `AuxRamCard.h:28` range starts at `$C003`.
- **[CRITICAL]** `$C004` incorrectly clears `m_readAux` instead of `m_writeAux`. This means RAMWRTOFF has no effect on writes, and RAMRDOFF ($C004 path) is misrouted. Source: `AuxRamCard.cpp:38`.
- **[CRITICAL]** `$C006` incorrectly clears `m_writeAux`. The real function of $C006 is INTCXROMOFF. Source: `AuxRamCard.cpp:44`.
- **[CRITICAL]** INTCXROM ($C006/$C007) not implemented. Can't switch $C100–$CFFF between slot ROMs and internal ROM. Programs that disable slot ROMs (e.g., 80-col card boot path) will malfunction.
- **[CRITICAL]** ALTZP ($C008/$C009) not implemented. Zero page ($0000–$01FF) and stack cannot be banked to aux RAM. All software using aux ZP (ProDOS, BASIC.SYSTEM, any 80-col firmware) has incorrect memory access.
- **[MAJOR]** SLOTC3ROM ($C00A/$C00B) not implemented. The 80-col firmware at $C300–$C3FF cannot be selected or deselected.

#### Recommended Fixes

Rename `AuxRamCard` range to `$C002`–`$C005`. Add `ALTZP` flag to `AuxRamCard` (or a new `MmuSwitchBank`). Add `INTCXROM` and `SLOTC3ROM` states to `AppleIIeSoftSwitchBank`. Wire ALTZP into `RebuildBankingPages()`.

---

### 1.2 Status Reads $C011–$C01F

#### Real //e Behavior

Each address returns **bit 7** = state of the named flag, **bits 0–6** = "floating bus" (typically the last byte seen on the bus, in practice many programs ignore bits 0–6). Source: Apple //e TRM Table 4-1; Sather UTAIIe §5-18.

| Address | Name | Bit 7 = 1 when |
|---------|------|----------------|
| $C011 R | BSRBANK2   | LC bank 2 selected |
| $C012 R | BSRREADRAM | LC RAM readable ($D000–$FFFF) |
| $C013 R | RDRAMRD    | Aux memory reads active |
| $C014 R | RDRAMWRT   | Aux memory writes active |
| $C015 R | RDINTCXROM | Internal ROM $C100–$CFFF active |
| $C016 R | RDALTZP    | Aux ZP/stack active |
| $C017 R | RDSLOTC3ROM| Slot $C3 ROM active |
| $C018 R | RD80STORE  | 80STORE active |
| $C019 R | RDVBLBAR   | Vertical blank active |
| $C01A R | RDTEXT     | Text mode active |
| $C01B R | RDMIXED    | Mixed mode active |
| $C01C R | RDPAGE2    | Page 2 active |
| $C01D R | RDHIRES    | Hires active |
| $C01E R | RDALTCHAR  | Alt char set active |
| $C01F R | RD80VID    | 80-col mode active |

#### AppleWin Implementation

`MemSetPaging()` returns `MemReadFloatingBus(highbit, nCycles)` where `highbit` is the flag and the low 7 bits come from the NTSC video bus (genuine floating-bus simulation). AppleWin `Memory.cpp` case block at offset `0x01`–`0x09` for `$C011`–`$C019`.

#### Casso Implementation

`AppleIIeKeyboard::Read()` at `AppleIIeKeyboard.cpp:58–99`:
```
$C013 RDRAMRD    ✓ (via m_auxRamSibling)
$C014 RDRAMWRT   ✓ (via m_auxRamSibling)
$C018 RD80STORE  ✓
$C01A RDTEXT     ✓ (returns !IsGraphicsMode)
$C01B RDMIXED    ✓
$C01C RDPAGE2    ✓
$C01D RDHIRES    ✓
$C01E RDALTCHAR  ✓
$C01F RD80VID    ✓
```

Comment in code at line 93: *"Other status bits (BSRBANK2, BSRREADRAM, INTCXROM, ALTZP, SLOTC3ROM, VBL) not yet tracked, return 0 in bit 7."*

#### Gaps / Bugs

- **[MAJOR]** `$C011` (BSRBANK2); always returns 0. Programs that check bank selection before writing to LC RAM will malfunction (e.g., ProDOS BASIC.SYSTEM bank-switching loop).
- **[MAJOR]** `$C012` (BSRREADRAM); always returns 0. Programs that verify LC RAM is read-enabled (e.g., `PRODOS` self-test) will fail.
- **[MAJOR]** `$C015` (RDINTCXROM); always returns 0.
- **[MAJOR]** `$C016` (RDALTZP); always returns 0. Critical for any code that tests whether it's running in aux ZP mode.
- **[MAJOR]** `$C017` (RDSLOTC3ROM); always returns 0.
- **[MAJOR]** `$C019` (RDVBLBAR); always returns 0. Any frame-sync loop using `BIT $C019` spin-waits will never see VBLANK.
- **[MINOR]** Floating-bus bits 0–6 are approximated as the latched keyboard byte (`AppleKeyboard::Read(0xC000) & 0x7F`). This is a close approximation but not a full NTSC-bus model. Most software only checks bit 7 so this is minor.

#### Recommended Fixes

Track `m_bank2Select` and `m_readRam` in `LanguageCard` (already present, just expose them to `AppleIIeKeyboard`). Add ALTZP/INTCXROM/SLOTC3ROM sibling refs to keyboard. Implement a cycle-counter-based VBLANK counter for `$C019`.

---

### 1.3 Display Switches $C050–$C05F

#### Real //e Behavior

Read **or** write to these addresses triggers the switch. $C058–$C05D are annunciators AN0–AN3 (general-purpose I/O). $C05E/$C05F control DHIRES on //e (on //c, gated by IOUDISBL). Source: Apple //e TRM §4; Sather UTAIIe §8-3.

#### Casso Implementation

`AppleSoftSwitchBank::Read()` handles `$C050`–`$C057` correctly (graphics, mixed, page2, hires). `AppleIIeSoftSwitchBank::Read()` adds `$C05E`/$`C05F` (DHIRES). Both devices ignore `$C058`–`$C05D`.

#### Gaps / Bugs

- **[MINOR]** Annunciators AN0–AN3 (`$C058`–`$C05D`) not tracked. AN0/AN1 drive joystick port control lines; AN2/AN3 are general outputs. Ignored for basic emulation but some games use them (e.g., CH Products joystick control, some Softalk games).
- **[NIT]** `AppleIIeSoftSwitchBank::GetEnd()` returns `$C07F` but `$C060`–`$C07D` all return 0 (paddles, tape, etc. absent). A comment would clarify intent.

---

## 2. Auxiliary Memory / 80STORE Banking

### Real //e Behavior

The Apple //e MMU implements three orthogonal banking dimensions (Sather UTAIIe §5-18–5-20, Apple //e TRM Ch. 4):

1. **RAMRD/RAMWRT** (`$C002`–`$C005`): Route reads/writes of `$0200`–`$BFFF` to aux or main.
2. **80STORE** (`$C000`/`$C001`) + **PAGE2** (`$C054`/`$C055`) + **HIRES** (`$C056`/`$C057`): Override RAMRD/RAMWRT for `$0400`–`$07FF` (always) and `$2000`–`$3FFF` (when HIRES also active).
3. **ALTZP** (`$C008`/`$C009`): Route `$0000`–`$01FF` (ZP + stack) to aux.

When **ALTZP=1** and the Language Card is enabled for reading/writing, the LC RAM backing store is also in aux (memaux `$D000`–`$FFFF`).

AppleWin `Memory.cpp` `UpdatePaging()`:
```cpp
// Pages 0x00–0x01 (ZP + stack)
memshadow[loop] = SW_ALTZP ? memaux + (loop << 8) : memmain + (loop << 8);

// Pages 0x02–0xBF ($0200–$BFFF)
memshadow[loop] = SW_AUXREAD ? memaux+(loop << 8) : memmain+(loop << 8);
memwrite[loop]  = same/different based on SW_AUXREAD vs SW_AUXWRITE;

// 80STORE overrides $0400–$07FF and $2000–$3FFF
if (SW_80STORE) {
  if (SW_PAGE2) { map $0400-$07FF to aux; }
  if (SW_PAGE2 && SW_HIRES) { map $2000-$3FFF to aux; }
}
```

### Casso Implementation

`EmulatorShell::RebuildBankingPages()` (`EmulatorShell.cpp:894–958`):

- 80STORE + PAGE2 banking for `$0400`–`$07FF` ✓
- 80STORE + PAGE2 + HIRES banking for `$2000`–`$3FFF` ✓
- RAMRD/RAMWRT banking for `$0200`–`$BFFF`: **NOT IMPLEMENTED**
- ALTZP banking for `$0000`–`$01FF`: **NOT IMPLEMENTED**
- Aux LC RAM (when ALTZP=1): **NOT IMPLEMENTED**

`AuxRamCard` stores `m_readAux`/`m_writeAux` flags but nothing in `RebuildBankingPages()` reads them.

### Gaps / Bugs

- **[CRITICAL]** RAMRD/RAMWRT do not affect the page table. Setting RAMRDON (`$C003`) sets `m_readAux=true` in `AuxRamCard`, but `RebuildBankingPages()` never checks `IsReadAux()` or `IsWriteAux()`. All `$0200`–`$BFFF` reads/writes always go to main. 80-col card, ProDOS, and most //e-specific software will produce wrong results.
- **[CRITICAL]** ALTZP (`$C008`/`$C009`) has no state and no page table mapping. Zero page and stack always come from main. 80-col card firmware patches ZP in aux; any program doing this will corrupt main ZP.
- **[CRITICAL]** Aux Language Card not implemented. When ALTZP=1 and LC RAM is enabled, reads/writes to `$D000`–`$FFFF` must come from `memaux` (aux 48K bank). Casso always uses the single main-side `m_ramBank1`/`m_ramBank2`/`m_ramMain` buffers.
- **[MAJOR]** `AuxRamCard::Reset()` (`AuxRamCard.cpp:108`) zeroes 64 KB of aux RAM on every reset including soft reset. Real hardware preserves aux RAM across soft reset.

### Recommended Fixes

Add `m_altZP` flag to `AuxRamCard` (or extend soft-switch bank). Extend `RebuildBankingPages()` to:
1. Map pages `$00`–`$01` to aux when `m_altZP`.
2. Map pages `$02`–`$BF` to aux read/write based on `m_readAux`/`m_writeAux`.
3. Provide a second set of LC RAM banks in `LanguageCard` (`m_auxRamBank1`, etc.) selected when ALTZP=1.

---

## 3. Language Card

### Real //e Behavior

$C080–$C08F bank-switching (Sather UTAIIe §5-22–5-24):

| Address | Read | Write | BANK | Notes |
|---------|------|-------|------|-------|
| $C080 R | RAM  | n/a | 2 | Read RAM, no write |
| $C081 R | ROM  | pre-W | 2 | Write-enable needs 2 reads |
| $C082 R | ROM  | n/a | 2 | Read ROM |
| $C083 R | RAM  | pre-W | 2 | RAM r/w (2 reads to arm write) |
| $C084–$C087 | mirror | | 2 | |
| $C088–$C08B | same pattern | | **1** | Bank 1 |
| $C08C–$C08F | mirror | | 1 | |

Write enable requires **two consecutive reads** of any odd-addressed switch (any of `$C081`, `$C083`, `$C089`, `$C08B`). A **write** to those addresses resets the pre-write state (can't double-tap with a write). Source: AppleWin `LanguageCard.cpp:129–148`.

Power-on state: `MF_BANK2 | MF_WRITERAM` (bank 2 selected, write-enabled, but ROM readable since `MF_HIGHRAM=0`). AppleWin `LanguageCard.cpp:62`.

On //e soft reset: LC resets to initial state (`LanguageCard.cpp:93–97`).

### AppleWin Implementation

`LanguageCardUnit::IO()` `LanguageCard.cpp:113–150`:
```cpp
memmode &= ~(MF_BANK2 | MF_HIGHRAM);
if (!(uAddr & 8)) memmode |= MF_BANK2;       // bits 8 determines bank
if (((uAddr & 2) >> 1) == (uAddr & 1)) memmode |= MF_HIGHRAM;  // bit pattern decodes read-RAM
if (uAddr & 1) {
    if (!bWrite && pLC->GetLastRamWrite())     // odd address, read, pre-write set
        memmode |= MF_WRITERAM;
}
pLC->SetLastRamWrite((uAddr & 1) && !bWrite); // arm pre-write on any odd-addr read
```

Key: `SetLastRamWrite` is a single boolean; any odd-address read arms it; the **next** odd-address read (any address) fires write-enable.

### Casso Implementation

`LanguageCard::UpdateState()` (`LanguageCard.cpp:76–111`):
```cpp
// Pre-write requires same address accessed twice:
if (m_preWrite && m_lastSwitch == switchAddr)
    m_writeRam = true;
else
    m_preWrite = true;
m_lastSwitch = switchAddr;
```

`LanguageCard::Reset()` (`LanguageCard.cpp:189–200`):
```cpp
fill(m_ramBank1.begin(), ...);   // CLEARS RAM
fill(m_ramBank2.begin(), ...);   // CLEARS RAM
fill(m_ramMain.begin(), ...);    // CLEARS RAM
m_readRam = false;
m_writeRam = false;
m_bank2Select = true;
```

### Gaps / Bugs

- **[CRITICAL]** `LanguageCard::Reset()` **zeroes all three RAM banks** (bank1, bank2, main). On real hardware and in AppleWin, LC RAM content is preserved across soft reset. This destroys any loaded Integer BASIC, Applesoft, or ProDOS stored in LC RAM when the user presses Reset. Source: `LanguageCard.cpp:191–193`.
- **[MAJOR]** Pre-write state machine requires the **same address** twice (`m_lastSwitch == switchAddr`). Real hardware requires any two consecutive odd-address reads. Reading `$C081` then `$C083` (both odd, both arm write-enable on hardware) will NOT enable write in Casso. Source: `LanguageCard.cpp:95`.
- **[MAJOR]** A **write** to an odd-addressed switch (`$C081`, etc.) should reset the pre-write state (Sather UTAIIe §5-23). In `LanguageCard::Write()` (`LanguageCard.cpp:53–58`), writes call `UpdateState()` just like reads, with no distinction. This incorrectly allows a write to start the pre-write countdown.
- **[MAJOR]** Aux LC RAM (`$D000`–`$FFFF` from `memaux` when ALTZP=1) not implemented. Only one set of LC banks exists (always main-side). Source: `LanguageCard.h:50–53`.
- **[MAJOR]** Power-on initial state differs from hardware. AppleWin `kMemModeInitialState = MF_BANK2 | MF_WRITERAM`. Casso starts with `m_writeRam=false` (`LanguageCard.h:56`). On hardware, write-enable is pre-armed at power-on, the very first access to `$C081` (or similar) enables write immediately after one read. Casso requires two reads in all cases.
- **[MINOR]** LC RAM is zeroed rather than randomized at power-on. Real DRAM starts with pseudo-random data.

### Recommended Fixes

1. Change `Reset()` to **not** clear `m_ramBank1/2/main`, preserve content. Only clear soft-switch state.  
2. Change pre-write: a single `bool m_preWrite` armed by any odd-address read (not same address).  
3. Add a `bool bWrite` parameter path: if it's a write to odd-address, clear `m_preWrite` instead of advancing it.  
4. Add `m_auxRamBank1`/`m_auxRamBank2`/`m_auxRamMain` and dispatch based on ALTZP state passed in from `AuxRamCard`.

---

## 4. Keyboard

### Real //e Behavior

| Address | Action | Returns |
|---------|--------|---------|
| $C000 R | Read keyboard latch | key \| bit7=strobe |
| $C010 R | Read + clear strobe | key with old strobe in bit 7 |
| $C010 W | Clear strobe | n/a |
| $C011–$C01F R | Status flags (see §1.2) | flag in bit 7, kbd in bits 0–6 |
| $C061 R | Open Apple / Button 0 | bit 7 = state |
| $C062 R | Closed Apple / Button 1 | bit 7 = state |
| $C063 R | Shift key (//e) | bit 7 = shift state |

Source: Apple //e TRM §4; Sather UTAIIe §7-3.

On //e the keyboard is lowercase-capable. `$C010` read returns the key code with bit 7 = the **pre-clear strobe value** (i.e., whether a key had been pressed).

### AppleWin Implementation

Keyboard handled in `source/Keyboard.cpp`. `$C010` read returns old latch before clearing strobe. `$C063` returns shift key via `GetKeyState(VK_SHIFT)`. Open/Closed Apple mapped to joystick buttons at `$C061`/`$C062` via `JoyReadButton()`.

### Casso Implementation

`AppleIIeKeyboard` (`AppleIIeKeyboard.cpp`):
- `$C061`/`$C062` checks exist in `Read()` at lines 36–45 but **the device range is `$C000`–`$C01F`** (inherited from `AppleKeyboard`, which does not override `GetEnd()`). The bus dispatches `$C061`/`$C062` to `AppleIIeSoftSwitchBank` (`$C050`–`$C07F`), which returns 0.
- `$C063` (Shift key) not implemented anywhere.
- `$C010` read path (`AppleKeyboard.cpp:37–48`): clears strobe via `fetch_and(0x7F)`, then returns `(key & 0x7F) | (anyKeyDown ? 0x80 : 0)`. The bit-7 of return should be the **old strobe value** (whether a key was latched), not the current `anyKeyDown` state.

### Gaps / Bugs

- **[CRITICAL]** Open Apple (`$C061`) and Closed Apple (`$C062`) are **unreachable via the bus**. `AppleIIeKeyboard::GetEnd()` returns `$C01F`; the logic to read these buttons at lines 36–45 is dead code. `AppleIIeSoftSwitchBank` handles `$C061`/`$C062` (within its `$C050`–`$C07F` range) but returns 0. Any game or program using Open/Closed Apple (the most common modifier keys on the //e) sees them as always-unpressed. Source: `AppleIIeKeyboard.h:18` (no GetEnd override), `AppleIIeKeyboard.cpp:36`.
- **[MAJOR]** Shift key (`$C063`) not implemented. Source: not found in any device.
- **[MINOR]** `$C010` return value: uses `m_anyKeyDown` for bit 7 instead of the old strobe value. Should be `return key;` (the old value before `fetch_and`). Source: `AppleKeyboard.cpp:43–48`.
- **[MINOR]** No keyboard auto-repeat. Real //e keyboard encoder generates repeated keystrokes while a key is held. `m_anyKeyDown` is tracked but no repeat timer fires.
- **[NIT]** `KeyPressRaw()` (`AppleIIeKeyboard.cpp:117–121`) calls the base-class `KeyPress()` which **does** uppercase-translate. Since the base `KeyPress()` conditionally uppercases (`AppleKeyboard.cpp:101–106`), and `KeyPressRaw` is intended to bypass that, it should store directly into `m_latchedKey` rather than calling `KeyPress()`.

### Recommended Fixes

Override `GetEnd()` in `AppleIIeKeyboard` to return `$C07F` (or `$C063` at minimum). Move Open/Closed Apple reads from `$C061`/`$C062` there. Add `$C063` Shift-key read. Fix `$C010` return to use old strobe value.

---

## 5. Speaker

### Real //e Behavior

Any access (read **or** write) to `$C030` (also mirrored `$C030`–`$C03F` on some references, though `$C030` is the canonical address) toggles the speaker diaphragm. The click results from the speaker position changing; audio is generated by the cadence of toggles. Source: Sather UTAIIe §9-3.

### AppleWin Implementation

`source/Speaker.cpp`: Reads and writes to `$C030` both toggle. Timestamps are recorded in CPU cycles for delta-sigma reconstruction.

### Casso Implementation

`AppleSpeaker::Read()` and `Write()` both toggle (`AppleSpeaker.cpp:29–63`). Timestamps recorded relative to frame start. Device covers `$C030`–`$C03F`. ✓

### Gaps / Bugs

- **[NIT]** The device range `$C030`–`$C03F` is correct (all 16 addresses mirror the speaker on real hardware). No functional gap identified.
- **[NIT]** Initial speaker state is `–0.25f` (`AppleSpeaker.h:57`). Amplitude of `±0.25` is intentional (avoids clipping at full scale). This is a design choice, not a bug.

---

## 6. Video / Character ROM

### 6.1 40-Column Text

**Real behavior:** 40×24 characters from `$0400`–`$07FF` (page 1) or `$0800`–`$0BFF` (page 2). Characters `$00`–`$3F` = inverse, `$40`–`$7F` = flash (alternates ~1 Hz), `$80`–`$FF` = normal. Source: Sather UTAIIe §8-12.

**Casso:** `AppleTextMode::Render()` (`AppleTextMode.cpp:99–175`). Inverse/flash/normal decode correct. Flash at 16 frames ≈ 60fps/16 ≈ 3.75 Hz (hardware flashes at ~1.87 Hz, 2× too fast). Page address correct.

**Gaps:**
- **[MINOR]** Flash rate `m_frameCount / 16` at 60fps = 3.75 Hz. Real hardware: flash is driven by the 555 timer at ~1 Hz (exact rate: 1.87 Hz per Sather). Fix: divide by 32 (or 30). Source: `AppleTextMode.cpp:110`.
- **[NIT]** Color is hardcoded to green-on-black (`kColorGreen = 0xFF00FF00`). The color/mono mode selection happens at the shell level post-render, so this is not a functional issue but the comment says "ARGB green" when the value `0xFF00FF00` in little-endian BGRA (D3D11 standard) is actually green (B=0, G=FF, R=0, A=FF). Comment is wrong; code is right.

### 6.2 80-Column Text

**Real behavior:** 80×24 characters. Even columns (0, 2, 4, …) from aux `$0400`–`$07FF`; odd columns (1, 3, 5, …) from main `$0400`–`$07FF`. Interleave is per-column within each row. Source: Apple //e TRM §6; Sather UTAIIe §8-29.

**Casso:** `Apple80ColTextMode::Render()` (`Apple80ColTextMode.cpp:64–128`). Correctly alternates aux (even columns) and main (odd columns) per `col % 2`. Aux memory pointer supplied via `SetAuxMemory()`.

**Gaps:**
- **[MAJOR]** Flash not implemented in 80-col mode, `m_frameCount` not maintained in `Apple80ColTextMode`. No `m_flashOn` flag, so flashing characters never flash. `GetGlyphRow()` is called without `m_altCharSet` parameter (`Apple80ColTextMode.cpp:109` uses `GetGlyphRow(charCode, py)`, which defaults `altCharSet=false`).
- **[MINOR]** 80-col mode does not respect ALTCHARSET. `GetGlyphRow()` call at line 109 always passes `altCharSet=false`. Should pass the shell's current ALTCHARSET state.
- **[MINOR]** Mixed mode text overlay always uses 40-col (`m_videoModes[0]`). When 80-col is active with mixed mode, the bottom 4 text rows should use 80-col text. Source: `EmulatorShell.cpp:1779`.

### 6.3 Lo-Res Graphics

**Real behavior:** 40×48 blocks; each text byte encodes two 4-bit colors. Uses same memory as text. Source: Sather UTAIIe §8-6.

**Casso:** `AppleLoResMode::Render()` correct in structure. Block dimensions derived from framebuffer size.

**Gaps:**
- **[MINOR]** The lo-res color values (`kLoResColors`, `AppleLoResMode.cpp:15–33`); the comment labels are wrong in several places. The actual pixel colors rendered in D3D BGRA format should be cross-checked: `0xFF0000DD` in little-endian BGRA = B=0xDD, G=0, R=0 which is a blue color, but the comment says "Magenta (Deep Red)". Apple lo-res color 1 should be Magenta/Deep Red (reddish). Byte ordering may be inverted. This needs verification against a reference framebuffer. Potential widespread palette error.

### 6.4 Hi-Res Graphics

**Real behavior:** 280×192 pixels, 40 bytes/row. Bit 7 of each byte is the "palette" bit (shifts artifact colors). Isolated pixels show artifact color based on column parity and palette bit. Adjacent lit pixels show white. Source: Sather UTAIIe §8-18–8-22.

**Casso:** `AppleHiResMode::Render()` (`AppleHiResMode.cpp:70–153`). Two-pass render: decode pixels + palette bits, then color by adjacency and parity. NTSC color table: `kNtscViolet/Green/Blue/Orange` (`NtscColorTable.h:13–16`).

**Gaps:**
- **[MINOR]** Adjacency-only color model is simplified. Real NTSC coloring involves 4-pixel sliding windows (pairs of adjacent set bits produce white; triplets or longer can show intermediate colors). The current check `leftOn || rightOn` → white collapses all multi-pixel sequences to white, which is the dominant case in practice and acceptable for now.
- **[MINOR]** The scanline address formula (`AppleHiResMode.cpp:35–43`) uses `group = scanline / 64` etc. The correct Apple II hi-res interleave is: `addr = base + 0x400*(scanline/64) + 0x80*(scanline%8) + 0x28*((scanline/8)%8)`. The current formula should be verified against a test pattern. A subtle interleave error could corrupt the image on certain scanlines.

### 6.5 Double Hi-Res

**Real behavior:** 560×192 pixels using 7 bits from each of 4 bytes per 28-pixel group: aux byte at even group, main byte, aux, main… Produces 16-color output. Source: Apple //e TRM §6-16.

**Casso:** `AppleDoubleHiResMode::Render()` (`AppleDoubleHiResMode.cpp:52–88`): labeled *"Fallback: renders as monochrome hi-res until double hi-res is fully wired via AuxRamCard."* Only main memory is read; aux interleave is absent; 16-color decoding not implemented.

**Gaps:**
- **[MAJOR]** Double hi-res is a stub. Programs using DHIRES (e.g., Print Shop //e, many graphics utilities) will display corrupted monochrome output. Tracked as issue #61 in the repo. Source: `AppleDoubleHiResMode.cpp:52`.

### 6.6 Character ROM

**Casso:** `CharacterRomData` handles 2KB (Apple II) and 4KB (//e enhanced) ROM files. `Decode4K()` uses `XOR 0xFF` bit inversion. Alt-char-set (MouseText) supported when 4KB ROM loaded.

**Gaps:**
- **[MINOR]** `Decode4K()` (`CharacterRomData.cpp:205–257`): The primary char set mapping for `chars[0x80..0xFF]` loads from offsets `0x400` and `0x600`. Needs cross-check against the specific Apple2e_Video.rom layout (each real ROM file may differ). If offset calculation is off, the normal character set will be garbled.
- **[NIT]** `GetGlyphRow()` bit order is "bit 0 = leftmost dot." The Apple //e character ROM (4KB) uses bit 0 = leftmost, but the 2KB Apple II ROM uses bit 6 = leftmost (reversed). `Decode2K()` reverses bits; `Decode4K()` does not reverse. Both comment and code should be verified against ROM dumps.

---

## 7. Disk II Controller

### Real //e Behavior

The Disk II controller is a Woz Machine (WM) state-machine-based GCR (6-and-2 encoding) stepper interface. The slot firmware at `$C600`–`$C6FF` bootstraps the system. IOU at `$C0x0`–`$C0xF` (where x = slot) controls stepper phases, motor, drive select, and data shift register. Source: Sather UTAIIe §9-6.

### Casso Implementation

`DiskIIController.cpp`: sector-based approach, nibblizes tracks on demand. Handles .DSK format. Slot 6 configured in `Apple2e.json`. **TODO comment** in header: *"Slot ROM at $Cs00-$CsFF needs a separate device or dual-range registration for boot firmware (Disk2.rom)."*

### Gaps / Bugs

- **[CRITICAL]** The disk II slot ROM at `$C600`–`$C6FF` is **shadowed by the internal ROM** (see §8). Booting from disk is currently impossible without fixing the ROM mapping. Source: `DiskIIController.h:52–54`; `EmulatorShell.cpp` `WireLanguageCard()`.
- **[MAJOR]** No WOZ (nibble-level) format support. .DSK sector images are nibblized on-the-fly using DOS 3.3 interleave only. Copy-protected software requiring precise nibble-level timing will not work.
- **[MAJOR]** Write support in `DiskImage::WriteSector()` exists but Flush (`DiskImage::Flush()`) must be called explicitly. There is no auto-flush on eject, machine switch, or exit. Disk changes during a session may be lost.
- **[MINOR]** Motor-off delay not simulated. Real Disk II keeps motor spinning ~1 second after last access (important for some copy-protection schemes and for correct `$C0x8`/`$C0x9` motor-enable timing).

---

## 8. ROM Mapping

### Real //e Behavior

The //e has a 16 KB ROM at `$C000`–`$FFFF` internal to the MMU/ROM chips. Layout:
- `$C000`–`$C0FF`: I/O space (never ROM-readable)
- `$C100`–`$C7FF`: Slot ROMs or internal ROM (selected by INTCXROM)
- `$C300`–`$C3FF`: 80-col firmware (internal when SLOTC3ROM=0)
- `$C800`–`$CFFF`: Peripheral expansion ROM (selected by INTC8ROM, activated by slot access)
- `$D000`–`$FFFF`: System ROM or Language Card RAM

The `Apple2e.rom` file is 16 KB covering `$C000`–`$FFFF`.

### Casso Implementation

`WireLanguageCard()` (`EmulatorShell.cpp:752–832`):
1. Loads system ROM from `$C000` (16 KB for //e).
2. Extracts `$D000`–`$FFFF` (12 KB) → gives to `LanguageCard`.
3. Re-creates a `lowerRom` for `$C100`–`$CFFF` from the remaining bytes.
4. Adds `lowerRom` to bus.

Slot ROM for Disk II (slot 6) is added at `$C600`–`$C6FF` in `CreateMemoryDevices()` **before** `WireLanguageCard()` runs.

**The shadowing problem:** `AddDevice()` inserts devices sorted by start address. `lowerRom` starts at `$C100`; slot ROM starts at `$C600`. In `FindDevice()`, linear scan finds `lowerRom` first for any address `$C100`–`$CFFF`, including `$C600`. The Disk II slot ROM is **unreachable**. Source: `MemoryBus.cpp:249–259`; `EmulatorShell.cpp:808–825`.

### Gaps / Bugs

- **[CRITICAL]** Slot ROM at `$C600`–`$C6FF` (and any other slot ROM) is shadowed by the lower internal ROM (`$C100`–`$CFFF`). Disk II boot ROM cannot execute. Source: `EmulatorShell.cpp:808–825` and `MemoryBus.cpp:154–163`.
- **[CRITICAL]** INTCXROM not implemented (see §1.1). There is no ability to switch between internal and slot ROMs at runtime. The lower ROM is always active.
- **[MAJOR]** `$C800`–`$CFFF` peripheral expansion ROM (INTC8ROM) not implemented. Slot devices that use `$C800` space (e.g., the Disk II firmware uses `$C800`–`$CFFF` once accessed) will not work correctly.
- **[MAJOR]** SLOTC3ROM not implemented. The 80-col firmware (`$C300`–`$C3FF`) is always served from the internal ROM. No ability to use an external card at slot 3.
- **[MINOR]** `$CFFF` access should disable INTC8ROM and deactivate the `$C800` expansion ROM. Not implemented since `$C800` expansion is absent.

### Recommended Fixes

The most pragmatic fix: make `FindDevice()` use **last-wins** rather than first-wins, or implement device priority. Better: maintain separate slot-ROM and internal-ROM page tables (as AppleWin does with `pCxRomPeripheral` / `pCxRomInternal`) and rebuild on INTCXROM changes.

---

## 9. CPU

### Real //e Behavior

- **Unenhanced //e** (original 1983): NMOS 6502 (same as Apple II+). No extra instructions.
- **Enhanced //e** (1985 revision): **65C02**, adds BIT #imm, BRA, PHX/PHY/PLX/PLY, STZ, TRB, TSB, and CMOS-fixed BCD. Source: Apple //e Tech Ref Supplement.
- The CPU runs at 1.0227 MHz (NTSC) with a 65-cycle horizontal period (17,030 cycles/frame at 262 lines).
- IRQ is used by the 80-col card and Mouse Card. NMI is not used by standard //e hardware.

### Casso Implementation

- `Apple2e.json:3`: `"cpu": "6502"`, NMOS 6502 only.
- `EmuCpu` wraps `Cpu` from `CassoCore`. `Cpu` implements the standard 6502 instruction set (`InitializeGroup00/01/10/Misc`).
- Cycle counting: instruction cycles tracked via `m_lastCycles` + `AddCycles()`. No mid-instruction cycle-accurate bus timing.
- No IRQ handling visible in `EmuCpu` or `EmulatorShell`.

### Gaps / Bugs

- **[MAJOR]** Enhanced //e should use a **65C02** CPU. Missing instructions include `BRA`, `PHX`, `PHY`, `PLX`, `PLY`, `STZ`, `TRB`, `TSB`, `BIT #imm`, and the indexed zero-page indirect addressing fix (`(zp,X)` wrap-around). ProDOS and much //e software uses 65C02 instructions on the Enhanced platform. Source: `Apple2e.json:3`.
- **[MAJOR]** No IRQ support. The 80-col card firmware uses VBL-driven IRQs on some configurations. ProDOS uses IRQs for clock cards and mouse interface.
- **[MINOR]** No NMI support. Not used by standard //e but absence is notable.
- **[MINOR]** `EmuCpu::InitForEmulation()` randomizes main RAM (`$0000`–`$BFFF`) but then **zeroes `$0800`–`$0BFF`** (text page 2) to avoid visual artifacts from 80STORE (`EmuCpu.cpp:113`). This comment admits incomplete 80STORE handling. Once RAMRD/RAMWRT and ALTZP are fixed, this workaround should be removed.
- **[NIT]** Cycle timing is per-instruction, not per-cycle. For accurate VBL timing, speaker audio, and disk read timing, sub-instruction cycle accuracy matters. Current model is acceptable for most software.

---

## 10. Reset Semantics

### Real //e Behavior

On the Apple //e, pressing **Reset** asserts the `/RESET` line. The MMU hardware resets most soft switches to known initial states **before** the CPU begins executing the reset handler. Specifically (Apple //e TRM §4, Sather UTAIIe §5-18):

| Switch | State After /RESET |
|--------|--------------------|
| 80STORE | OFF |
| RAMRD | OFF (main) |
| RAMWRT | OFF (main) |
| INTCXROM | OFF (slot ROMs) |
| ALTZP | OFF (main) |
| SLOTC3ROM | OFF (internal) |
| 80COL | OFF |
| ALTCHARSET | OFF |
| PAGE2 | OFF |
| HIRES | OFF |
| TEXT/GRAPHICS | TEXT (off) |
| MIXED | OFF |
| LC state | Reset to initial (MF_BANK2 \| MF_WRITERAM) on //e |

**RAM content** (main and aux) is **preserved** across soft reset. LC RAM is also preserved (on //e). The ROM reset handler (`$FA62` on enhanced //e) sets up the I/O devices and prints `]` but does **not** clear user RAM.

AppleWin: soft reset calls `MemResetPaging()` → `ResetPaging(FALSE)` → `GetCardMgr().GetLanguageCardMgr().Reset(FALSE)` → resets LC soft state but **not LC RAM**. Source: `LanguageCard.cpp:89–97`.

### Casso Implementation

`EmulatorShell::ProcessCommands()`, `IDM_MACHINE_RESET` case (`EmulatorShell.cpp:1453–1459`):
```cpp
case IDM_MACHINE_RESET:
{
    Word resetVec = m_cpu->ReadWord(0xFFFC);
    m_cpu->SetPC(resetVec);   // ONLY sets PC — nothing else
    break;
}
```

`IDM_MACHINE_POWERCYCLE` case (`EmulatorShell.cpp:1463–1470`):
```cpp
case IDM_MACHINE_POWERCYCLE:
{
    m_memoryBus.Reset();       // calls Reset() on ALL devices
    m_cpu->InitForEmulation(); // re-randomizes RAM, re-fetches reset vector
    break;
}
```

`MemoryBus::Reset()` calls each device's `Reset()`. For `LanguageCard::Reset()`, this **zeroes LC RAM** and resets all LC state. For `AuxRamCard::Reset()`, this **zeroes aux RAM**. For `AppleIIeSoftSwitchBank::Reset()`, this resets display/memory switches.

### Gaps / Bugs

- **[CRITICAL]** Soft reset (`IDM_MACHINE_RESET`) **does not reset any soft switches**. 80COL, ALTCHARSET, 80STORE, RAMRD, RAMWRT, ALTZP, display mode, and LC state all survive the reset unchanged. This is the direct cause of the reported 80-col mode persisting across reset. The //e hardware asserts `/RESET` which the MMU decodes to reset all switches. Source: `EmulatorShell.cpp:1453–1459`.
- **[CRITICAL]** Soft reset does not reset LC state. The language card should return to bank-2, ROM-readable, write-pre-armed (`MF_BANK2 | MF_WRITERAM`) after reset on the //e.
- **[CRITICAL]** Power-cycle (`IDM_MACHINE_POWERCYCLE`) **zeroes LC RAM** via `LanguageCard::Reset()`. On real hardware, power cycling leaves DRAM in an indeterminate (not zero) state. More importantly, `m_memoryBus.Reset()` calls LC Reset which is also called conceptually for a *soft* reset, but the **soft** reset path doesn't call it at all, and the **power** reset path incorrectly clears RAM. Source: `LanguageCard.cpp:191–193`.
- **[MAJOR]** Power-cycle zeroes aux RAM (`AuxRamCard::Reset()`). Real DRAM is random at power-on.
- **[MAJOR]** Soft reset does not reset CPU registers (`SP`, `P` flags, etc.). The 6502 reset sequence sets `I=1` and sets `SP` to some indeterminate value (the ROM handler sets it to `$FF`). Casso's soft reset only sets `PC`. Source: `EmulatorShell.cpp:1456–1459`.

### Why 80-Col Persists / Gets Corrupted Across Reset

The mechanism is now clear:

1. The //e ROM initializes 80-col mode via the 80-col firmware at `$C300` (if present).
2. The ROM reset handler expects 80COL to be hardware-cleared by `/RESET`. On Casso, it's not.
3. After Casso's soft reset, 80COL remains whatever it was. The ROM may try to re-initialize 80-col state but can't reliably predict the current switch state.
4. Additionally, because ALTZP and RAMRD are also not reset, the ROM handler's ZP writes may go to aux instead of main, further corrupting state.

### Recommended Fixes

Create a `SoftReset()` method distinct from `PowerCycle()`. In `SoftReset()`:
1. Reset all soft switches (call device `Reset()` methods except for RAM clearing).
2. Reset LC state (not RAM content); add a separate `ResetSwitches()` method to `LanguageCard`.
3. Set CPU `I=1`, fetch `PC` from `$FFFC`.
4. Rebuild page table (`RebuildBankingPages()`).

In `PowerCycle()`:
1. Call `SoftReset()` (switches + CPU).
2. Additionally randomize main RAM, aux RAM, LC RAM.
3. `InitForEmulation()` already randomizes main RAM ✓; extend to aux and LC RAM.

---

## 11. Prioritized Action List

All gaps consolidated by severity, highest priority first.

### CRITICAL: Functional Correctness Blockers

| # | Gap | Source File(s) |
|---|-----|----------------|
| C1 | Slot ROM ($C600) shadowed by lower internal ROM, disk booting impossible | `EmulatorShell.cpp:808`, `MemoryBus.cpp:249` |
| C2 | Soft reset does not reset soft switches (80COL, RAMRD, ALTZP, etc. persist) | `EmulatorShell.cpp:1453` |
| C3 | Open Apple ($C061) / Closed Apple ($C062) unreachable, keyboard range ends at $C01F | `AppleIIeKeyboard.h:18`, `AppleIIeKeyboard.cpp:36` |
| C4 | RAMRD/RAMWRT do not affect page table, aux $0200–$BFFF banking completely absent | `EmulatorShell.cpp:894`, `AuxRamCard.cpp:30` |
| C5 | ALTZP not implemented, aux ZP/stack banking absent | `AuxRamCard.cpp:28`, `EmulatorShell.cpp:894` |
| C6 | AuxRamCard address bug: $C002 missing, $C004 clears wrong flag, $C006 wrong switch | `AuxRamCard.h:28`, `AuxRamCard.cpp:38,44` |
| C7 | LanguageCard::Reset() zeroes LC RAM, content lost on any device reset | `LanguageCard.cpp:191–193` |
| C8 | INTCXROM ($C006/$C007) not implemented, internal vs slot ROM cannot be switched | Not found; `AppleIIeSoftSwitchBank.cpp` |
| C9 | Soft reset does not reset LC state (should reset to MF_BANK2\|MF_WRITERAM) | `EmulatorShell.cpp:1453` |

### MAJOR: Significant Compatibility Issues

| # | Gap | Source File(s) |
|---|-----|----------------|
| M1 | $C011 (BSRBANK2) and $C012 (BSRREADRAM) always return 0 | `AppleIIeKeyboard.cpp:93` |
| M2 | $C015–$C017 (RDINTCXROM, RDALTZP, RDSLOTC3ROM) always return 0 | `AppleIIeKeyboard.cpp:93` |
| M3 | $C019 (RDVBLBAR) always return 0, frame-sync polling broken | `AppleIIeKeyboard.cpp:93` |
| M4 | SLOTC3ROM ($C00A/$C00B) not implemented | Not found |
| M5 | Aux LC RAM (ALTZP=1 → $D000–$FFFF from memaux) not implemented | `LanguageCard.h:50` |
| M6 | LC pre-write requires same address twice (should be any two odd-addr reads) | `LanguageCard.cpp:95` |
| M7 | LC write to odd-addr switch doesn't reset pre-write state | `LanguageCard.cpp:53` |
| M8 | Double hi-res is a rendering stub: only main memory, monochrome | `AppleDoubleHiResMode.cpp:52` |
| M9 | Enhanced //e should use 65C02 CPU (BRA, PHX/PLX, STZ, TRB, TSB, etc.) | `Apple2e.json:3` |
| M10 | No IRQ support, 80-col card VBL interrupt and ProDOS IRQs absent | `EmuCpu.h` |
| M11 | Soft reset doesn't reset CPU registers (SP, P flags) | `EmulatorShell.cpp:1456` |
| M12 | Mixed-mode text overlay always 40-col; should use 80-col when 80COL active | `EmulatorShell.cpp:1779` |
| M13 | ALTCHARSET ignored in 80-col text render | `Apple80ColTextMode.cpp:109` |
| M14 | Flash not implemented in 80-col text render | `Apple80ColTextMode.cpp` |
| M15 | $C800–$CFFF peripheral expansion ROM (INTC8ROM) not implemented | Not found |
| M16 | Shift key ($C063) not implemented | Not found |

### MINOR: Behavioral Differences

| # | Gap | Source File(s) |
|---|-----|----------------|
| N1 | Flash rate ~3.75 Hz, should be ~1.87 Hz (divide frame counter by 32 not 16) | `AppleTextMode.cpp:110` |
| N2 | $C010 return uses anyKeyDown for bit 7 instead of old strobe value | `AppleKeyboard.cpp:43` |
| N3 | Annunciators AN0–AN3 ($C058–$C05D) not tracked | `AppleSoftSwitchBank.cpp:65` |
| N4 | LC power-on: write-enable not pre-armed (kMemModeInitialState difference) | `LanguageCard.h:56` |
| N5 | LC RAM zeroed at power-on instead of randomized | `LanguageCard.cpp:191` |
| N6 | Aux RAM zeroed at power-on instead of randomized | `AuxRamCard.cpp:108` |
| N7 | Hi-res scanline address formula needs cross-check against Apple II reference | `AppleHiResMode.cpp:35` |
| N8 | Lo-res color palette may have BGRA byte-order error | `AppleLoResMode.cpp:15` |
| N9 | Keyboard auto-repeat not implemented | `AppleKeyboard.h:59` |
| N10 | No motor-off delay for Disk II | `DiskIIController.cpp` |
| N11 | Disk image not auto-flushed on eject/quit | `DiskImage.cpp` |
| N12 | KeyPressRaw calls KeyPress (uppercases) instead of bypassing | `AppleIIeKeyboard.cpp:119` |
| N13 | $C004 sets EmuCpu page 2 text RAM to zero in InitForEmulation (workaround note) | `EmuCpu.cpp:113` |

### NIT: Polish / Accuracy

| # | Gap | Source File(s) |
|---|-----|----------------|
| P1 | Decode4K glyph offsets need cross-check against specific ROM file | `CharacterRomData.cpp:205` |
| P2 | Hi-res NTSC model is adjacency-only, not full 4-pixel phase model | `AppleHiResMode.cpp:120` |
| P3 | No $CFFF peripheral ROM deactivation | Not found |
| P4 | Bus overlap warning at $C600 logged but not resolved | `MemoryBus.cpp:149` |
| P5 | AuxRamCard header comment labels $C003/$C004 incorrectly | `AuxRamCard.h:17` |

---

## References

- **Apple //e Technical Reference Manual** (1987 ed.), Apple Computer Inc.
- **Understanding the Apple IIe** by Jim Sather (Quality Software, 1983), UTAIIe §5-18–5-30, §7-3, §8-6–8-30, §9-3
- **AppleWin** `source/Memory.cpp` SHA `65d0467c`, `source/LanguageCard.cpp` SHA `2904816e`, `source/Memory.h` SHA `ac3ea800`, https://github.com/AppleWin/AppleWin
- **Casso source**: `CassoEmuCore/Devices/`, `CassoEmuCore/Video/`, `Casso/EmulatorShell.cpp`, `CassoEmuCore/Core/`