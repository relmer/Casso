# Research: Apple //e Fidelity (spec 004)

Decisions resolving every "NEEDS CLARIFICATION" implied by the spec, the user
input to /speckit.plan, and the audit. Format per agent: **Decision /
Rationale / Alternatives considered**.

---

## 1. MMU consolidation: fold AuxRamCard into AppleIIeMmu

- **Decision**: Delete `AuxRamCard` (current location of //e MMU soft switches,
  with the audit-C6 bug at $C003-$C006). Introduce `AppleIIeMmu` that owns
  RAMRD, RAMWRT, ALTZP, 80STORE, INTCXROM, SLOTC3ROM, INTC8ROM, and
  internally tracks PAGE2 / HIRES / TEXT / MIXED inputs (read from the
  soft-switch bank). The MMU consumes a banking-changed callback from the
  soft-switch bank and rebinds the existing `MemoryBus` page table.
  Status reads ($C013-$C018, RD80STORE at $C018) come from the //e
  soft-switch bank but read their values through the MMU.
- **Rationale**: One coherent owner for memory routing eliminates a category
  of cross-device bugs (audit C6). Keeps the page-table fast path (no
  per-access branching). Aligns with FR-001..FR-007 and FR-026..FR-029.
- **Alternatives considered**:
  - *Keep AuxRamCard, fix only $C003-$C006*. Rejected — the cross-device
    coupling and split ownership of $C0xx is the root cause of C6 and similar
    bugs; fixing the symptom leaves the architectural wound.
  - *Per-access dispatch (drop the page-table fast path)*. Rejected on
    perf and on the explicit user direction to extend, not replace, the
    fast path.

## 2. Soft-switch surface ownership

- **Decision**: $C000-$C00F write switches owned by `AppleIIeSoftSwitchBank`
  (it forwards to `AppleIIeMmu`'s setters). $C011-$C01F status reads owned
  by the //e bank (it reads from the appropriate subsystem: LC for BSRBANK2/
  BSRREADRAM; MMU for RDRAMRD/RDRAMWRT/RDINTCXROM/RDALTZP/RDSLOTC3ROM/RD80STORE;
  video timing for RDVBLBAR; soft-switch bank for the display flags).
  $C050-$C05F display switches stay with the //e bank, observable through
  the same path.
- **Rationale**: One read path, one write path, clear ownership. FR-001..FR-004.
- **Alternatives considered**: Distributed ownership (each device handles
  its own status read). Rejected — fragments the floating-bus low-7-bit
  policy and makes the read path harder to audit.

## 3. Floating bus low-7-bit source

- **Decision**: Drive low-7 of $C011-$C01F from the keyboard data latch
  (the bus value most commonly observed by //e software).
- **Rationale**: Matches AppleWin in the common case; we already have the
  keyboard latch surfaced. A cycle-accurate floating-bus simulation (the
  byte the video shifter is fetching at this exact cycle) is out of
  scope for this feature and not required by any FR.
- **Alternatives considered**: True video-shifter floating bus. Deferred.
  Stub-zero. Rejected — explicitly violates audit and FR-003.

## 4. Modifier keys ($C061-$C063): keyboard vs soft-switch bank

- **Decision**: Keep them in `AppleIIeKeyboard` and extend its `GetEnd()`
  range to $C063.
- **Rationale**: Keyboard cohesion. Modifier state lives with the keyboard
  device; routing it through the soft-switch bank would require
  inter-device wiring just to satisfy a 3-byte status surface.
- **Alternatives considered**: Move to soft-switch bank. Rejected as above.

## 5. CPU pluggable strategy

- **Decision**: Introduce `ICpu` as a **CPU-family-agnostic** strategy
  interface (Reset / Step / SetInterruptLine(kind, asserted) / GetCycleCount).
  Nothing 6502-specific in `ICpu`. 6502-shaped register inspection lives
  on a separate `I6502DebugInfo` interface that `Cpu6502` implements
  alongside `ICpu`. Move existing `Cpu` implementation into `Cpu6502`
  implementing both. `EmuCpu` (the engine wrapper) holds
  `unique_ptr<ICpu>`. No 65C02 / 65C816 / Z80 implementation in this feature.
- **Rationale**: FR-030 + FR-041. Family-agnostic `ICpu` lets us add 65C02
  (same family), 65C816 (different register width / reset model), and
  even non-6502 CPUs (Z80 if the //c+ Z80 card or CP/M card is ever
  modeled) without redesigning the interface. Splitting register
  inspection into a family-specific debug interface avoids forcing
  every CPU into a 6502-shaped register struct.
- **Alternatives considered**: Templates over the CPU type. Rejected
  — virtual dispatch cost on a per-Step boundary is negligible vs the
  per-instruction work, and the abstraction is much less viral.

## 6. IRQ infrastructure

- **Decision**: `InterruptController` aggregates per-source assertion
  bits. On any non-zero aggregate it asserts
  `ICpu::SetInterruptLine(kMaskable, true)`; otherwise false. `Cpu6502`
  checks the maskable line at instruction
  boundaries; if asserted and `I` is clear, it pushes PC+P (with B=0,
  U=1), sets I, and jumps via $FFFE/$FFFF.
- **Rationale**: FR-031 + FR-032. Conventional 6502 IRQ semantics.
- **Alternatives considered**: Per-cycle IRQ checking. Rejected as
  unnecessary precision for the in-scope devices (none).

## 7. Reset semantics

- **Decision**: Two distinct paths. `SoftReset()` resets soft switches
  per the //e MMU reset rules (RAMRD=0, RAMWRT=0, ALTZP=0, 80STORE=0,
  80COL preserved, ALTCHARSET preserved, INTCXROM cleared, SLOTC3ROM
  cleared, INTC8ROM cleared, TEXT=1, MIXED=0, PAGE2=0, HIRES=0; LC
  resets to BANK2|WRITERAM; aux RAM and LC RAM contents preserved),
  sets CPU `I=1`, loads PC from $FFFC, leaves SP/A/X/Y untouched (the
  ROM handler reinitializes SP). `PowerCycle()` does everything
  `SoftReset()` does **and** seeds main RAM, aux RAM, all four LC
  banks via `Prng`.
- **Rationale**: FR-034, FR-035. Matches Sather + AppleWin.
- **Alternatives considered**: Single conflated path with a flag.
  Rejected as the existing code is exactly that and is the source of
  reset-related correctness gaps.

## 8. Power-on memory init: deterministic seeded random

- **Decision**: Implement `SplitMix64` in `Core/Prng.{h,cpp}`. Production
  default seed is `time(nullptr) ^ pid`-derived; the headless harness
  pins seed = `0xCA550001`. RAM bytes are filled by streaming the PRNG.
- **Rationale**: Real DRAM is indeterminate; modelling it as zero hides
  bugs that depend on uninitialized memory. Determinism in tests is
  required by FR-038 + SC-005.
- **Alternatives considered**: Zero (current). Rejected — masks bugs.
  System RNG. Rejected — non-deterministic.

## 9. Disk II — full nibble-level rewrite (folds issue #61)

- **Decision**: Implement an LSS-equivalent state machine that reads/writes
  one bit per CPU half-cycle into a per-track nibble bit-stream. Per-slot
  soft switches at $C0E0-$C0EF (slot 6) drive motor on/off, drive 1/2 select,
  Q6/Q7 read/write mode, and head step phases. WOZ images load natively
  (Phase 12 and v2). `.dsk`/`.do`/`.po` images pass through `NibblizationLayer`
  on load (and the inverse on save-back). `DiskImageStore` auto-flushes
  modified images on eject, machine switch, and exit.
- **Rationale**: Closes the entire disk-correctness category in one
  architectural pass. CP titles work as a natural consequence.
  FR-021..FR-025 + audit §7 + issue #61.
- **Alternatives considered**: Incremental sector-level patch + later
  WOZ. Rejected — leaves CP support permanently broken and accumulates a
  second code path that must be deprecated later.

## 10. Video composition (FR-020) and MIXED+80COL (FR-017a)

- **Decision**: `VideoOutput` resolves the active mode set per scanline
  region. For MIXED + 80COL, the bottom 4 rows are rendered by calling
  `Apple80ColTextMode::RenderRowRange(20, 24, ...)` — the same routine
  used by full-screen 80-col. No branched copy.
- **Rationale**: FR-017a explicitly forbids a duplicated path; FR-020
  requires composition.
- **Alternatives considered**: Inline 80-col logic in a mixed-mode
  helper. Rejected as direct duplication.

## 11. NTSC artifact color in HiRes

- **Decision**: Implement an NTSC artifact lookup driven by a sliding
  4-pixel window over the bit pattern, producing the 6-color palette
  observed on real //e composite output. Encapsulated in
  `AppleHiResMode::GenerateScanline`.
- **Rationale**: FR-018; required for software that depends on artifact
  color (which is most //e graphics software).

## 12. Video timing model granularity for VBL

- **Decision**: A scanline-level counter incremented per CPU cycle (each
  scanline = 65 CPU cycles, 262 scanlines/frame for NTSC). VBL is
  asserted for scanlines 192..261. `RDVBLBAR` reads bit 7 from this
  state. Cycle-exact horizontal timing is **not** required by any FR
  in this feature.
- **Rationale**: FR-033. Sufficient for frame-sync software. Cycle-exact
  HBL deferred (out of scope).

## 13. Headless host architecture

- **Decision**: Introduce `IHostShell` (window/input/audio surface). GUI
  binary `Casso/HostShell.cpp` implements it via Win32. Tests bind
  `HeadlessHost` (no window, no audio device, no real input). Disk
  fixture access goes through `IFixtureProvider`, which restricts paths
  to `UnitTest/Fixtures/`.
- **Rationale**: FR-036..FR-038, FR-043, FR-044 + constitution §II.
- **Alternatives considered**: Conditional compilation. Rejected —
  test code paths must exercise production code; only the host edge
  may differ.

## 14. Test fixtures provenance and license

- **Decision**: All fixtures public-domain or project-original. ROMs
  remain shared with the production `ROMs/` directory but accessed
  through `IFixtureProvider`. Document provenance in
  `UnitTest/Fixtures/README.md`. CP sample selected from a known
  public-domain demo / sample disk.
- **Rationale**: Avoid licensing risk; FR-044.

## 15. Performance budget calibration

- **Decision**: Production target is FR-042's ~1% of one host core when
  throttled to 1.023 MHz. The unthrottled `PerformanceTests`
  (Release-only) asserts that 1,000,000 emulated cycles take under a
  budget that corresponds to a steady-state 10x headroom (≥ ~10 MHz
  emulated throughput on a modern host). Threshold lives in a single
  named constant; CI runs the test and fails on regression.
- **Rationale**: FR-042 / SC-007. The 10x-headroom proxy avoids
  flaky-throttling tests while still catching real perf regressions.

## 16. //e ROM mapping details

- **Decision**: $C000-$CFFF ROM region is gated by INTCXROM (when 1, all
  $C100-$CFFF read from the internal ROM image; when 0, slot ROMs are
  visible at $C100-$C7FF and $C300 obeys SLOTC3ROM). $C800-$CFFF tracks
  INTC8ROM with the documented disable-on-read-of-$CFFF rule. $D000-$FFFF
  participates in LC bank/RAM/ROM selection per LC state. The 16 KB
  `apple2e.rom` is loaded into `RomDevice` instances mapped accordingly.
- **Rationale**: FR-026..FR-029, audit §8.

## 17. Backward-compat strategy for ][ and ][+

- **Decision**: //e additions are layered above the ][/][+ device set;
  ][/][+ machine JSONs do not reference the //e MMU, the //e soft-switch
  bank, or the //e keyboard. CPU strategy refactor preserves byte-exact
  behavior for `Cpu6502`. A new `BackwardsCompatTests.cpp` re-asserts
  the pre-feature scenarios so any regression surfaces with a precise
  diagnostic.
- **Rationale**: FR-039, FR-040, Phase 14.

## 18. Out-of-scope architectural seams (kept open, not implemented)

- 65C02 → another `ICpu` impl.
- //c → another machine JSON + a small extra device set composed atop //e.
- //e Enhanced → swap to `Cpu65C02` and update the //e ROM image.
- Clock card → asserts IRQ via `InterruptController`.
- Mouse card → same.
- Mockingboard → same; uses two AY-3-8910 instances.

No code lands in this feature for any of the above.
