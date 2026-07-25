# Casso Architecture

How Casso is built, how the pieces fit at runtime, and — because it emulates a
1 MHz machine on a modern host and has to stay cheap doing it — **why the hot
paths are shaped the way they are**. If you are about to touch the memory bus,
the CPU fetch path, the device tick loop, or the render pipeline, read the
relevant section first; the performance model is load-bearing, not incidental.

For code style, EHM conventions, and build/merge gates see
[`.github/copilot-instructions.md`](.github/copilot-instructions.md). For the
per-feature design history see `specs/`. For the //e hardware-fidelity rationale
see [`docs/iie-audit.md`](docs/iie-audit.md) (historical — the work it drove is
done; the code cites it by section number).

---

## Contents

1. [Projects and layering](#1-projects-and-layering)
2. [Threading model](#2-threading-model)
3. [The memory model](#3-the-memory-model) — the centerpiece
4. [CPU fetch / execute](#4-cpu-fetch--execute)
5. [Devices and the per-instruction tick](#5-devices-and-the-per-instruction-tick)
6. [Video and the render / present pipeline](#6-video-and-the-render--present-pipeline)
7. [Audio](#7-audio)
8. [Performance decisions log](#8-performance-decisions-log)
9. [Roads not taken](#9-roads-not-taken)
10. [Where to look](#10-where-to-look)

---

## 1. Projects and layering

Five projects in `Casso.sln`, layered so the CPU/assembler core knows nothing
about the emulator, and the emulator core knows nothing about Win32/D3D:

| Project | Kind | Contents |
|---|---|---|
| **CassoCore** | static lib | 6502/65C02 CPU, microcode/opcode tables, assembler, parser |
| **CassoEmuCore** | static lib | Apple II devices, memory bus, MMU, video modes, audio generators |
| **Casso** | Win32 GUI | the emulator app — D3D11 render, WASAPI audio, Dxui chrome, shell |
| **CassoCli** | console | AS65-compatible assembler CLI (+ a `run` subcommand) |
| **UnitTest** | DLL | MS Native CppUnitTest; links CassoCore + CassoEmuCore |

The dependency arrows only point downward: `Casso → {CassoEmuCore, CassoCore}`,
`CassoEmuCore → CassoCore`, `CassoCore → nothing`. This is why the CPU can be
driven headless by tests and by the CLI, and it is the reason one specific
optimization (the inline read fast path, §4) is careful **not** to leak
emulator types back up into `CassoCore` — see [Roads not taken](#9-roads-not-taken).

The runtime object graph in the GUI:

```
EmulatorShell ── owns ── MachineManager ── builds ── the machine
     │                                                   │
     ├── EmuCpu (ICpu) ── MemoryBusCpu (Cpu6502/Cpu65C02 ← Cpu)
     │                          │
     │                          └── MemoryBus ── devices (via the bus)
     │                                             + Apple2eMmu (coordinator)
     ├── CpuManager (the emulation thread)
     └── D3DRenderer / DxuiHwndSource (present) + Dxui panel tree (chrome)
```

`EmuCpu` is the `ICpu` wrapper the shell holds; underneath it is a
`MemoryBusCpu` (a `Cpu6502`/`Cpu65C02` strategy that routes memory through the
`MemoryBus` instead of a flat array). Tests substitute a flat-memory `TestCpu` /
`TestCpu65C02` at the same seam.

---

## 2. Threading model

Two threads matter:

- **The emulation (CPU) thread** — `CpuManager::ThreadProc`. It drains a command
  queue (`DrainCommandQueue`), runs CPU slices (`ExecuteCpuSlices`), ticks the
  cycle-driven devices, and produces the video framebuffer. Everything that
  touches CPU / bus / device state runs here.
- **The UI thread** — `EmulatorShell::RunMessageLoop`. It pumps Win32 messages,
  paints the chrome (the Dxui panel tree), and presents via D3D. The Dxui panel
  tree is **single-threaded and UI-thread-affine** (enforced by
  `DxuiAssertUiThread`, ~154 call sites), so anything that mutates a panel or
  measures text must run here.

**Command routing** (get this wrong and you trip `DxuiAssertUiThread`):

- UI-layout commands → `PostMessage(WM_COMMAND)` → handled on the UI thread.
- Emulation / audio commands → `PostCommand` → the CPU thread's
  `DispatchCpuCommand` (machine switch, reset, power-cycle, step, disk ops).

Because `SwitchMachine` runs on the CPU thread, any UI work it needs is marshaled
back to the UI thread with a posted message (`WM_APP_DXUI_UPDATE_TITLE` → title
refresh + `ReflowChromeForMachineChange`). This is the canonical pattern for
"CPU thread needs to touch chrome."

**Frame handoff.** The CPU thread renders the guest screen into a framebuffer and
signals the UI thread (`m_frameReadyEvent` / `PublishFramebuffer`); the UI thread
uploads and presents. The CPU thread does **not** touch D3D or the panel tree.

---

## 3. The memory model

This is where most of the performance lives, and the design mirrors the //e
hardware more closely than it first looks.

### 3.1 What the hardware actually does

There is no lookup table in the machine. The //e's **MMU** and **IOU** chips
decode the 16 address lines plus the current **soft-switch latch state**,
combinationally, every cycle, to drive exactly one chip's select line:

- `$0000–$BFFF` → main or aux DRAM (per RAMRD/RAMWRT/ALTZP/80STORE/PAGE2/HIRES)
- `$C000–$CFFF` → I/O strobes (IOU/MMU soft switches) or slot/internal ROM
- `$D000–$FFFF` → motherboard ROM or language-card RAM (per the LC latches)

The soft switches are latches; the decode is a pure function of
`(address, latches)`. Some accesses *also* toggle a latch — that is the "side
effect," and it is just the chip reacting to being addressed.

### 3.2 The software model: two lanes, memoizing the decode

Running the full decode on every one of the millions of accesses per second
would be too slow. But the decode result only changes when a latch changes, so
Casso **precomputes and caches it**, re-deriving only on soft-switch writes. That
cache is the page table, and it splits along a real hardware distinction:

| Lane | Answers | Structure | For |
|---|---|---|---|
| **Page table** | "where is the byte?" → `Byte*`, one load, no call | `m_readPage[0x100]` / `m_writePage[0x100]` (per-page) | passive storage chips (RAM, ROM) |
| **Device map** | "which chip do I call?" → `MemoryDevice*`, then a virtual `Read`/`Write` | `m_ioDeviceMap` (byte-granular, `$C000–$FFFF`) | reactive chips (I/O, computed values) |

`MemoryBus::ReadByte` is uniform: `page = m_readPage[addr>>8]; if (page) return
page[addr&0xFF]; else dispatch to the device`. Writes are the same with
`m_writePage`. The read path has no `address <` special-casing — a page is either
a pointer (fast) or null (fall through to the device).

The two granularities are not a wart — they are faithful. **Memory decode is
coarse** (RAM/ROM are page/bank aligned → a 256-entry page table suffices).
**I/O decode is fine** (page `$C0` packs several overlapping sub-page devices —
keyboard `$C000–$C063`, speaker `$C030–$C03F`, soft switches `$C050–$C07F`, LC
control `$C080–$C08F`, disk `$C0E0–$C0EF` — so the device map must be
byte-granular). The device map resolves overlaps **first-match-wins** (entries
sorted by start), which is why it is precomputed from the device list rather than
scanned per access (`BuildIoDeviceMap`, rebuilt only on `AddDevice`/`RemoveDevice`).

### 3.3 Passive vs reactive pages — what is mapped where

A page gets a **page-table pointer** iff the chip that answers there is passive
storage; it stays on the **device handler** iff the chip reacts to being
addressed (side effects, computed reads):

| Range | Nature | Lane |
|---|---|---|
| `$0000–$BFFF` | main/aux RAM | page table (read + write) |
| `$C000–$CFFF` | I/O: soft switches, floating bus, disk, slots | **device map** (side effects) |
| `$C100–$CFFF` (//c) | static internal firmware, *except* `$C3xx`/`$CFFF` | page table (read); `$C3`/`$CF` stay device (INTC8ROM side effects) |
| `$D000–$FFFF` | ROM or LC RAM | page table (read); writes stay device |

Notes:

- **Writes to ROM/LC (`$D000–$FFFF`, `$C1xx`)** are left on the device path. The
  language card's WRITERAM gating and two-read pre-write arm are stateful and
  belong in the device; reads and writes still resolve to the *same* buffer, so
  they stay coherent (read-ROM/write-RAM works because the read page points at
  ROM while the write goes to the hidden RAM).
- The reactive `$Cxxx` pages (`$C3xx` latches INTC8ROM, `$CFFF` clears it) keep
  the handler even on the //c where the effect is inert — modeled faithfully as
  "reactive page → handler."

### 3.4 Re-pointing: keeping the cache correct

Because the page table is a *cache* of the decode, it must be rebuilt whenever a
latch that affects it changes. `Apple2eMmu` owns this:

- **RAMRD/RAMWRT/80STORE** → `RebindPageTable` re-resolves `$00–$BF`
  (`ResolveZeroPage` / `ResolveMain02_BF` / `ResolveText04_07` / `ResolveHires20_3F`).
- **ALTZP** → re-resolves zero page **and** re-points the LC window (aux↔main).
- **LC bank/read switches (`$C08x`)** → `LanguageCard::RebindWindow` re-points
  `$D0–$FF` read pages (bank1/bank2 × main/aux, or ROM).
- **//c internal ROM attach / `$C028` bank flip** → `RebindCxxxInternalRom`
  re-points `$C1–$CF`, and `RebindWindow` re-points `$D0–$FF`. Both matter
  because `SetInternalRom` move-reassigns its buffer, so stale pointers would
  dangle.

Miss a re-point and you serve stale bytes; that is the one real hazard of this
design, so the trigger set above is the thing to preserve when changing banking.

---

## 4. CPU fetch / execute

`Cpu::StepOne` fetches an opcode, indexes the microcode table, and runs the
addressing-mode + operation. The hot part is the memory read.

**`Cpu::ReadByte` is a non-virtual inline fast path.** It indexes an optional
page table directly and only falls through a virtual `ReadByteSlow` hook for
null pages:

```cpp
Byte ReadByte (Word address) {
    if (m_readPages != nullptr) {
        Byte* page = m_readPages[address >> 8];
        if (page != nullptr) return page[address & 0xFF];  // RAM + ROM/LC
    }
    return ReadByteSlow (address);   // I/O ($C000–$CFFF) + unmapped
}
```

`m_readPages` is a `Byte* const*` pointing at the bus's `m_readPage[256]` array.
Crucially it is a bare pointer-to-pointers — `CassoCore` knows nothing about
`MemoryBus`. `MemoryBusCpu` overrides `ReadByteSlow` to route the slow path
through the bus (I/O device dispatch). The standalone base `Cpu` leaves
`m_readPages` null and always takes the slow path into its flat `memory[]`.

`ReadByteSlow` (for I/O) calls `UpdateBusCycle`, which refreshes the
sub-instruction bus-cycle estimate the Disk II controller samples at `$C0Ex`.
It is **absolute** (`m_busCycle = m_totalCycles + (lastCycles-1)`), re-derived at
each access — so RAM and ROM fetches, which take the fast path and skip it, cause
no drift (the `$C0Ex` access itself, always a slow-path I/O read, refreshes it).

**65C02.** `Cpu65C02` shares the dispatch and adds the CMOS instructions
(`BRA`, `PHX/PHY/PLX/PLY`, `STZ`, `TRB`, `TSB`, `BIT #imm`, fixed BCD). Both cores
pass Klaus Dormann's functional tests and Tom Harte's SingleStepTests (10,000
vectors/opcode) including the stable undocumented NMOS opcodes; those suites are
the gate for any change on this path.

---

## 5. Devices and the per-instruction tick

Devices implement `MemoryDevice` (`Read`/`Write`/`GetStart`/`GetEnd`) and register
on the `MemoryBus`. Cycle-driven devices are ticked **once per instruction** from
the CPU thread with the instruction's cycle count:

```cpp
diskController->Tick (cpu->GetLastInstructionCycles());
mockingboard  ->Tick (...);
keyboard      ->Tick (...);
// AppleMouse is an ICycleSink wired via SetCycleSink (same cadence)
```

`Apple2eMmu` is not a bus device — it is a **coordinator** that owns the aux
RAM and re-points the page table on banking changes (§3.4). It owns the
`CxxxRomRouter` (which it registers on the bus).

**The tick is a hot loop, so idle work is gated.** The pattern: do the expensive
thing only when it can matter.

- **Mockingboard** — `Ay8910::GenerateSample` early-outs when all amplitude
  registers are zero (`IsSilent`); a silent PSG does no synthesis.
- **`AppleMouse::Tick`** (the //c's biggest single cost before gating) — drains
  host motion behind a **relaxed atomic load** so the common idle tick pays no
  locked read-modify-write, and only calls `UpdateIrqLines` when a latch actually
  changed this tick.

Interrupts aggregate through `InterruptController` (level-sensitive sources:
Mockingboard VIA, mouse X/Y + VBL, etc.), which drives the CPU IRQ line.

---

## 6. Video and the render / present pipeline

**Frame production (CPU thread).** Video modes (`AppleTextMode`,
`Apple80ColTextMode`, `AppleLoResMode`, `AppleHiResMode`,
`AppleDoubleHiResMode`) rasterize the guest screen from the display pages into a
framebuffer. Flash and mode timing come from the cycle-driven `VideoTiming`, not
from the render.

**Dirty tracking — don't re-rasterize an unchanged screen.** The `MemoryBus`
marks the display pages "watched"; a write that actually *changes a displayed
byte* raises `m_videoDirty`. Two refinements keep an idle DOS prompt from
re-rendering: **screen-hole exclusion** (the `$78–$7F` bytes of each 128-byte
block are undisplayed scratch that firmware hammers) and a **same-value compare**
(a re-store of the same byte is not dirty). A banking change also raises dirty,
since it can swap which buffer the renderer reads with no write landing.

**Present gating (GPU) — present on change.** `D3DRenderer::NeedsPresent` returns
false (skip both the CRT post-process and the swap-chain `Present`) when the
framebuffer is clean, CRT params are unchanged, and the persistence trail has
settled. So a static screen costs ~no GPU. When a present *is* needed,
`UploadAndComposite` maps the framebuffer into a texture and `RenderCrtFrame`
runs the CRT post-process into the back buffer.

**Chrome.** The drive band, `//c` switch bar, buttons, and letterbox are painted
by the Dxui panel tree on the UI thread — immediate-mode, re-tessellated each
presented frame (`DxuiPainter::PushQuad`). This is the current largest CPU render
cost and the target of the deferred retained-mode initiative (see below).

---

## 7. Audio

WASAPI output on the UI/audio side; the generators (speaker delta-sigma, Disk II
mechanical, Mockingboard PSG) produce PCM from cycle-timestamped events on the
CPU thread. `WasapiAudio::SubmitFrame` is **non-blocking** — it drops rather than
blocks, capped at a 3-frame backlog — so audio buffer pressure never throttles
the emulation thread. (Emulation speed is governed by the frame pacing in the
CPU-thread loop, not by audio.)

---

## 8. Performance decisions log

Each entry: the problem → the fix → *why this shape*. Newest first. Rationale
also lives in the commit messages; this is the durable summary.

| Area | Problem | Fix | Why |
|---|---|---|---|
| **`$C100–$CFFF` fetch (//c)** | mouse firmware runs from `$C700` through `CxxxRomRouter::Read`'s virtual dispatch every byte | page-map the passive internal-ROM pages into `m_readPage`; keep `$C3`/`$CF` on the handler | reactive pages need the handler; passive ROM wants a pointer |
| **`$D000–$FFFF` fetch** | ROM/LC fetches (e.g. //e monitor keyboard poll at `$FDxx`) paid `LanguageCardBank::Read`'s virtual dispatch every byte | page-map the LC window; re-point on bank/read/ALTZP/`$C028` changes | it's memory with no read side effects — belongs in the fast lane |
| **`$Cxxx` routing** | `Apple2eMmu` getters (`GetIntCxRom`/`GetSlotC3Rom`) were virtual, called per `$Cxxx` access | mark `Apple2eMmu` `final` (devirtualize + inline); add a //c no-slots fast path in the router | trivial getters shouldn't be indirect calls on a hot path |
| **//c mouse tick** | two atomic RMW drains + `UpdateIrqLines` every instruction (~9.7% of the //c) | relaxed-load guard on the drain; skip `UpdateIrqLines` when nothing latched | host input arrives at ≤1 kHz, not 1 MHz — don't pay per instruction |
| **Device dispatch** | `FindDevice` linearly scanned the device list on every `$C000+` access | precompute a byte-granular `address→device` map, rebuilt on device add/remove | overlaps make it byte-granular; first-match-wins is baked in at build time |
| **CPU reads** | `ReadByte` was virtual (indirect call per fetch) | non-virtual inline page-table fast path + virtual `ReadByteSlow` hook for I/O | fetches dominate; the fast path must not be a call |
| **Video render** | framebuffer was FNV-hashed each frame to detect change | watched-page dirty tracking with screen-hole + same-value filtering | a hash still reads the whole buffer; a write hook is O(changes) |
| **Idle / present** | GPU + CPU ran full-tilt on a static screen | `NeedsPresent` present-on-change; bounded idle message wait | a still screen should cost ~nothing |

Measurement notes for anyone re-profiling: numbers are noisy (Debug ~10× Release;
same commit varied run-to-run), so only same-session A/B is meaningful. Traces
were VS CPU Usage `.diagsession` files, extracted with `xperf -a stack -butterfly`
against the Release PDBs (symbol cache in `c:\symbols`). `.diagsession` files are
**not** checked in.

---

## 9. Roads not taken

Decisions we deliberately did **not** make, so they aren't re-litigated:

- **A 64 KB byte map returning bytes for ROM.** The device map removes the
  *scan*, not the *dispatch*; for pure-memory fetches the virtual call is the
  cost. A map that returns bytes-by-pointer *is* the page table — so ROM went
  into the page table, not a fancier device map.
- **Merging the page table and device map into one per-page `{read, write,
  handler}` table.** Blocked by two load-bearing facts: (1) the CPU's inline fast
  path needs `m_readPage` to stay a standalone contiguous `Byte*[256]` so
  `CassoCore` can index it as `Byte* const*` without depending on a
  `CassoEmuCore` struct; (2) page `$C0`'s overlapping sub-page devices force the
  device lane to be byte-granular. A literal single table would cost either the
  CPU decoupling or I/O precision, for a cosmetic gain on the hottest path.
- **Consolidating the `$C0` soft-switch devices into one IOU-style handler.**
  This is the *only* path to a truly unified per-page table (it would make the
  device lane page-granular), and it is hardware-faithful — but it's a
  device-architecture change (new class, rewired switches, changed
  first-match-wins semantics, many tests), not a table merge. Left as a possible
  future initiative on its own branch.
- **Deferred, still open:** chrome **retained-mode** rendering (cache the
  tessellated geometry, rebuild per-panel on change — blocked by the
  single-threaded panel tree) and CPU **dynarec / threaded dispatch** (the
  interpreter's per-instruction overhead is the largest remaining CPU cluster,
  but a major undertaking with self-modifying-code + cycle-accuracy constraints).

---

## 10. Where to look

| Concern | Files |
|---|---|
| CPU core, microcode, fast-path `ReadByte` | `CassoCore/Cpu.{h,cpp}`, `Cpu6502.*`, `Cpu65C02.*`, `Microcode.*`, `CpuOperations.cpp` |
| Bus routing, page table, device map | `CassoEmuCore/Core/MemoryBus.{h,cpp}`, `MemoryBusCpu.*` |
| Banking / MMU / ROM routing | `CassoEmuCore/Devices/Apple2eMmu.*`, `LanguageCard.*`, `CxxxRomRouter.*`, `Apple2cRomBank.*` |
| Devices | `CassoEmuCore/Devices/` (keyboard, speaker, soft switches, Disk2, Mockingboard, game port, ACIA, mouse) |
| Video modes + timing | `CassoEmuCore/Video/`, `VideoTiming.*` |
| Threading, frame pump, commands | `Casso/Shell/CpuManager.*`, `Casso/EmulatorShell.cpp`, `Casso/Shell/MachineManager.cpp` |
| Render / present / CRT | `Casso/D3DRenderer.cpp`, `Casso/CrtPostProcess.cpp`, `Casso/DxuiHwndSource*` |
| Audio | `Casso/WasapiAudio.cpp`, `CassoEmuCore/Audio/`, `CassoEmuCore/Devices/Mockingboard/` |
| Machine definitions | `Resources/Machines/*/*.json` |

---

*Keep this current when a hot path or a banking trigger changes — the
performance model is the part most easily broken by a well-meaning refactor.*
