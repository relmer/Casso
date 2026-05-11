# Data Model: Apple //e Fidelity (spec 004)

Entities, fields, relationships, and state transitions for every component
introduced or materially changed by this feature. (Existing unchanged
entities are referenced but not re-documented.)

---

## CPU layer

### `ICpu` (interface)
**CPU-family agnostic** strategy interface. Nothing in `ICpu` assumes a 6502-shaped register file, vector address, or interrupt model. Implementations: `Cpu6502` (this feature). Future: `Cpu65C02`, `Cpu65C816`, `CpuZ80`, etc. (all out of scope; all drop in via this interface).

| Member | Purpose |
|---|---|
| `Reset()` | Cold reset. Implementation-defined (e.g. `Cpu6502`: I=1, PC=mem[$FFFC..$FFFD], SP/A/X/Y untouched). |
| `Step()` | Execute one instruction. Returns elapsed cycles. |
| `SetInterruptLine(CpuInterruptKind, bool)` | Generic interrupt assertion (kNonMaskable / kMaskable). Mapping to electrical model is implementation-defined. |
| `GetCycleCount()` | Monotonic cycle counter. |

Register-file inspection is NOT on `ICpu`. It lives on family-specific debug interfaces (`I6502DebugInfo`, future `I65816DebugInfo`, etc.). Consumers obtain via `dynamic_cast` and must tolerate cast failure for non-matching CPUs.

### `Cpu6502` (concrete)
Existing 6502 logic, re-homed behind `ICpu` AND `I6502DebugInfo`. Adds:
- `m_irqLine : bool` — current asserted state of the maskable line.
- IRQ check at each instruction boundary: if `m_irqLine && !(P & I)` →
  push PCH, push PCL, push (P with B=0, U=1), set I, PC ← mem[$FFFE..$FFFF].

### `InterruptController`
| Field | Type | Purpose |
|---|---|---|
| `m_assertions` | `uint32_t` | Bitmask of asserted source IDs. |
| `m_cpu` | `ICpu*` | Target CPU. |

| Method | Purpose |
|---|---|
| `RegisterSource() → SourceId` | Allocate a slot for a device. |
| `Assert(SourceId)` | Set bit; recompute aggregate; drive `ICpu::SetInterruptLine(kMaskable, ...)`. |
| `Clear(SourceId)` | Clear bit; recompute; drive line. |

State: agg = (m_assertions != 0). No edge logic; level-sensitive.

---

## Memory layer

### `AppleIIeMmu` (replaces `AuxRamCard`)

Owns all routing state for //e memory.

| Field | Type | Power-on default |
|---|---|---|
| `m_ramrd` | bool | false |
| `m_ramwrt` | bool | false |
| `m_altzp` | bool | false |
| `m_store80` | bool | false |
| `m_intCxRom` | bool | false |
| `m_slotC3Rom` | bool | false |
| `m_intC8Rom` | bool | false |

Inputs read from the soft-switch bank (cached on banking-changed events):
PAGE2, HIRES, TEXT, MIXED.

| Method | Purpose |
|---|---|
| `Set80Store(bool)` etc. (one per flag) | Write switches; recompute page table. |
| `Get80Store() const` etc. | Read switches; consumed by $C011-$C01F status reads. |
| `OnSoftSwitchChanged()` | Recompute MemoryBus page table per region. |
| `OnPowerCycle()` | Reset all flags to power-on defaults. |
| `OnSoftReset()` | Reset per //e MMU rules (see research §7). |

Region resolvers (private helpers, each ≤ 50 lines, ≤ 2 indent levels beyond EHM):

- `ResolveZeroPage()` — $0000-$01FF, gated by ALTZP.
- `ResolveMain02_BF()` — $0200-$BFFF reads via RAMRD, writes via RAMWRT, with
  text-page ($0400-$07FF) and hi-res ($2000-$3FFF) carve-outs that 80STORE
  takes over.
- `ResolveText04_07()` — $0400-$07FF, by 80STORE+PAGE2.
- `ResolveHires20_3F()` — $2000-$3FFF, by 80STORE+HIRES+PAGE2.
- `ResolveCxxx()` — $C100-$CFFF, by INTCXROM, SLOTC3ROM, INTC8ROM.
- `ResolveLcD000_FFFF()` — coordinates with `LanguageCard` and ALTZP.

### `MemoryBus` (modified)

Existing page-table fast path retained. New: pages can be bound to either
"main" or "aux" backing arrays; the MMU rebinds page entries on each
banking-changed callback. No per-access branching added.

### `RamDevice` (modified)
Constructor accepts an optional `Prng &` (default null). When provided, fills
backing memory with PRNG output. `Reset()` is a no-op (RAM contents preserved
across soft reset). `PowerCycle()` re-seeds.

### `Prng` (new)
SplitMix64. Pure C++; deterministic; reproducible across host architectures.

---

## Soft switches and keyboard

### `AppleIIeSoftSwitchBank` (modified)
- $C000-$C00F write switches: forward to `AppleIIeMmu` setters.
- $C011-$C01F status reads: read source data per research §2; bit 7 is the
  status flag, bits 0-6 sourced from the keyboard latch (research §3).
- $C050-$C05F: own existing display switches; on each write, notify the MMU
  via `OnSoftSwitchChanged()` so PAGE2/HIRES/TEXT/MIXED-driven routing is
  re-resolved.

### `AppleIIeKeyboard` (modified)
- `GetEnd()` extended to $C063.
- $C061: bit 7 = Open Apple held. $C062: bit 7 = Closed Apple held. $C063:
  bit 7 = Shift held.
- $C010 read **or** write clears strobe. $C011-$C01F NEVER touch strobe.

### `LanguageCard` (modified)
| Field | Type | Power-on default |
|---|---|---|
| `m_flags` | `uint16_t` (bitfield: BANK2, READRAM, WRITERAM) | `BANK2 \| WRITERAM` |
| `m_preWriteCount` | `uint8_t` | 0 |
| `m_lastReadAddrOdd` | bool | false |

State machine (per access in $C080-$C08F):

```
on read $C08x where x is odd:
    if m_preWriteCount < 2: m_preWriteCount++
    apply switch effects (bank/read/write per x)
on read $C08x where x is even:
    m_preWriteCount = 0
    apply switch effects
on write $C080-$C08F:
    m_preWriteCount = 0      # writes RESET the arming
    apply switch effects
write to $D000-$FFFF (LC region):
    if WRITERAM and m_preWriteCount >= 2: write to LC RAM bank
    else: ignored
```

`OnSoftReset()`: flags = `BANK2 | WRITERAM`; pre-write count cleared.
RAM contents preserved.
`OnPowerCycle()`: same flag reset; LC RAM (all four banks) re-seeded by `Prng`.

ALTZP integration: when `m_mmu->GetAltzp()` is true, the 16 KB LC window
backs to the aux LC RAM banks; otherwise main LC RAM banks.

---

## Disk subsystem

### `IDiskImage` (interface)
| Member | Purpose |
|---|---|
| `GetTrackCount() → int` | 35 / 40 / WOZ-defined. |
| `GetTrackBitCount(track) → size_t` | Bit-stream length per track. |
| `ReadBit(track, bitIndex) → uint8_t` | Per-bit read; controller advances 1 bit per CPU half-cycle. |
| `WriteBit(track, bitIndex, bit)` | Per-bit write from controller. |
| `IsDirty() → bool` | Auto-flush gate. |
| `IsWriteProtected() → bool` | Honors WOZ INFO chunk and host file attributes. |
| `Serialize(outBytes) → HRESULT` | Convert back to original format (WOZ / DSK / DO / PO). |
| `GetSourceFormat() → enum DiskFormat` | For round-trip. |

This matches `contracts/IDiskImage.md` exactly. The bit-stream API is intentional: the nibble engine ticks one bit per CPU half-cycle, so per-bit access is the natural unit.

### `WozLoader`
Parses WOZ v1 + v2 chunks: INFO, TMAP, TRKS, META. Builds `DiskImage`
instances with native bit streams.

### `NibblizationLayer`
- `LoadDsk(bytes) → DiskImage` — apply DOS 3.3 sector ordering (16 sectors)
  → 6+2 GCR encode → nibbles per track.
- `LoadDo(bytes)`, `LoadPo(bytes)` — analogous, with the appropriate sector
  interleave.
- `SaveDsk(diskImage) → bytes` — inverse for save-back.

### `DiskImage`
Concrete `IDiskImage`. Holds a `std::vector<std::vector<uint8_t>>` of bit
streams (one per track). Tracks dirty bits per track for serialization.

### `DiskImageStore`
| Method | Purpose |
|---|---|
| `Mount(slot, drive, path)` | Load image, register for auto-flush. |
| `Eject(slot, drive)` | Flush if dirty, then release. |
| `FlushAll()` | Called on machine switch and on emulator exit. |

### `DiskIIController` (rewritten)
| Field | Type | Notes |
|---|---|---|
| `m_motorOn[2]` | bool | Per-drive. |
| `m_selectedDrive` | int | 0 or 1. |
| `m_q6`, `m_q7` | bool | Latches. |
| `m_phaseState` | uint8_t (4 bits) | Half-track stepping. |
| `m_headHalfTrack[2]` | int | 0..70. |
| `m_writeProtect[2]` | bool | |
| `m_engine[2]` | `DiskIINibbleEngine` | Per-drive bit-stream cursor. |

Soft switches at $C0E0+slot*16 .. $C0EF+slot*16 (slot 6 → $C0E0..$C0EF):
- $C0E0/E1: motor off/on
- $C0E2/E3: drive 1 / drive 2 select
- $C0E4..E7: phases off
- $C0E8..EF: phases 0..3 on/off, Q6/Q7 latches per Sather table
- Read of even / write to odd: latch behavior per LSS spec

### `DiskIINibbleEngine`
| Method | Purpose |
|---|---|
| `Tick(cycles)` | Advance the bit-stream cursor by `cycles / 4` bits (≈250 kbps at 1.023 MHz half-cycle). |
| `ReadLatch() → uint8_t` | Returns the assembled latch value when Q7=0. |
| `WriteLatch(uint8_t)` | When Q7=1 + Q6=1, push bits into the track bit stream. |

State machine: per-clock-tick the cursor reads next bit, shifts the read
latch left, OR-in bit; when MSB set, latch is "full" until consumed (per LSS).
Write path inverse.

---

## Video subsystem

### `IVideoMode` (existing concept, formalized)
Existing mode classes (`AppleTextMode`, `Apple80ColTextMode`, etc.). For this
feature, `Apple80ColTextMode` exposes:
- `RenderFullScreen(framebuffer)`
- `RenderRowRange(firstRow, lastRow, framebuffer)` — used by mixed-mode (FR-017a).

### `VideoOutput` (modified)
Composes per-region rendering. Pseudo-code:

```
for each frame:
    determine top region: (TEXT? text40/text80 : graphics(LORES|HIRES|DHR))
    determine bottom region (rows 20..23 if MIXED): always TEXT;
        if 80COL: Apple80ColTextMode::RenderRowRange(20,24,fb)
        else:     AppleTextMode::RenderRowRange(20,24,fb)
```

### `VideoTiming` (new)
Tracks `m_cycleInFrame : uint32_t`, advanced from the CPU's per-Step cycle
count. NTSC: 65 cycles/scanline × 262 scanlines = 17030 cycles/frame.
- `IsInVblank() → bool` — true when current scanline ≥ 192.
- $C019 (`RDVBLBAR`) reads bit 7 from `IsInVblank()`.

### `Apple80ColTextMode` (modified)
Correct aux/main interleave: even column → aux memory @ ($0400 + offset),
odd column → main memory @ ($0400 + offset). ALTCHARSET selects MouseText
glyphs; flash semantics applied only when ALTCHARSET selects the flashing set.

### `AppleHiResMode` (modified)
NTSC artifact rendering: 4-bit sliding window over the bit stream maps to
one of 16 color codes per quad-pixel; resolved to the //e 6-color palette.
Encapsulated in `GenerateScanline` helper ≤ 50 lines.

### `AppleDoubleHiResMode` (modified)
Correct aux+main interleave per the //e DHR layout: pixels 0-6 from aux
$2000+offset, pixels 7-13 from main $2000+offset, alternating across
the scanline.

---

## Headless test infrastructure

### `IHostShell` (interface)
| Member | Purpose |
|---|---|
| `OpenWindow(width,height)` | GUI: real window. Headless: no-op. |
| `PresentFrame(framebuffer)` | GUI: blit. Headless: cache last frame for hashing. |
| `OpenAudioDevice(format)` | GUI: WASAPI. Headless: returns a `MockAudioSink`. |
| `PollInput() → InputEvent[]` | GUI: real. Headless: returns injected events. |

### `IFixtureProvider` (interface)
| Member | Purpose |
|---|---|
| `OpenFixture(relativePath) → std::vector<uint8_t>` | Restricts to `UnitTest/Fixtures/` only. |

### `HeadlessHost` (test)
- Implements `IHostShell` with no Win32 calls.
- Owns a `LastFrame` buffer; `HashFrame()` returns SHA-256.
- Owns an injected-keystroke queue.
- Provides `HRESULT RunFor(uint64_t cycles)` and `HRESULT RunUntil(predicate, uint64_t maxCycles, bool & outReached)`. The bool out-param is set true if the predicate fired before `maxCycles` was reached, false if the cycle budget was exhausted first. Both methods follow the EHM convention.

### `MockAudioSink`
Counts toggles, drops samples; satisfies the speaker-sink contract.

### `MockIrqAsserter`
Test-only `IInterruptSource` that the test code can assert/clear at will to
verify CPU IRQ dispatch and the controller's aggregation.

---

## Machine assembly (Apple //e)

`Machines/apple2e.json` (modified) declares:

- `Cpu6502` (via `ICpu`)
- `InterruptController`
- `MemoryBus`
- `Prng` (seed configurable)
- `RamDevice` × 2 (main 64K, aux 64K)
- `RomDevice` (apple2e.rom 16K)
- `AppleIIeMmu`
- `AppleSoftSwitchBank` (][ base, layered)
- `AppleIIeSoftSwitchBank` (composed atop)
- `AppleIIeKeyboard`
- `AppleSpeaker`
- `LanguageCard`
- `DiskIIController` (slot 6) + `DiskImageStore`
- `VideoTiming`
- `VideoOutput` with: `AppleTextMode`, `Apple80ColTextMode`, `AppleLoResMode`,
  `AppleHiResMode`, `AppleDoubleHiResMode`

`Machines/apple2.json` and `Machines/apple2plus.json` are NOT modified.

---

## Reset / power-cycle invariants

| Subsystem | SoftReset | PowerCycle |
|---|---|---|
| CPU regs | I=1, PC←$FFFC; SP/A/X/Y untouched | same |
| Main RAM | preserved | re-seeded via Prng |
| Aux RAM | preserved | re-seeded via Prng |
| LC RAM (all 4 banks) | preserved | re-seeded via Prng |
| MMU flags | per //e MMU reset rules (research §7) | power-on defaults |
| LC flags | BANK2 \| WRITERAM | BANK2 \| WRITERAM |
| Soft-switch display flags | TEXT=1, MIXED=0, PAGE2=0, HIRES=0; 80COL preserved; ALTCHARSET preserved | all defaults: TEXT=1 rest 0 |
| InterruptController | assertions cleared | assertions cleared |
| VideoTiming | cycle counter cleared | cycle counter cleared |
| DiskImageStore | drives keep their mounts; auto-flush dirty | unmount all; flush dirty |
