# Commodore roadmap: VIC-20, C64, C128D

**Written:** 2026-09-08. **Base:** `master` @ `18b857b8`.

What it would take to add the Commodore line to Casso, ordered by what the
existing architecture already answers and what it does not. This is a survey,
not a commitment: nothing here is scheduled, and no spec has been opened.

The short version is that the Commodore machines split cleanly along one line.
Everything **below** the video chip is a good fit -- in places a better fit than
the Apple II, because the C64's address decoding is closer to what Casso's
memory model was built to memoize. Everything **at and above** the video chip
breaks assumptions that the render path and the device tick are built on.

The VIC-20 is the machine that sits entirely on the good side of that line,
which is why it leads the order below rather than trailing the C64 as an
afterthought.

---

## Contents

1. [Chips, for readers without the Commodore vocabulary](#1-chips-for-readers-without-the-commodore-vocabulary)
2. [What the architecture already gives us](#2-what-the-architecture-already-gives-us)
3. [The four walls](#3-the-four-walls)
4. [Why the VIC-20 goes first](#4-why-the-vic-20-goes-first)
5. [The tiers](#5-the-tiers)
6. [The C128D is its own program](#6-the-c128d-is-its-own-program)
7. [Hazards](#7-hazards)
8. [Decisions not made](#8-decisions-not-made)

---

## 1. Chips, for readers without the Commodore vocabulary

| Chip | What it is |
|---|---|
| **PLA** (82S100) | The C64's address decoder. Combinational logic over the address lines, three banking bits from the 6510's on-chip port at `$01` (LORAM / HIRAM / CHAREN), and GAME / EXROM from the cartridge port. Drives exactly one chip select per cycle: RAM, BASIC ROM, KERNAL ROM, character ROM, or I/O. No state of its own. |
| **VIC-I** (6560 / 6561) | The VIC-20's video and sound chip. 22x23 characters, roughly 176x184 pixels. No sprites, no raster interrupt (there is a readable raster register at `$9004`, which programs poll), and the audio is three square waves plus noise and a master volume. |
| **VIC-II** (6567 / 6569) | The C64's video chip. 320x200 plus border, eight hardware sprites, raster interrupts, and a per-scanline register interface that ordinary games rewrite mid-frame. Steals CPU cycles: a badline halts the processor for around 40 cycles to fetch character data, and each active sprite steals more. |
| **SID** (6581 / 8580) | The C64's sound chip. Three oscillators (saw, triangle, pulse, noise), ADSR per voice, ring modulation and hard sync between voices, then one **analog** multimode filter with resonance shared across all three. The filter's cutoff curve depends on on-die capacitors and varies audibly between individual chips; the 6581 also distorts and carries a DC offset that software exploited for sample playback. |
| **CIA** (6526) | The C64's I/O chip, two of them. Timers, a time-of-day clock, two 8-bit ports, a serial shift register, and an interrupt line. CIA1 drives IRQ and scans the keyboard matrix; CIA2 drives NMI and the serial bus, and its port A selects which 16K bank the VIC-II sees. |
| **VIA** (6522) | The VIC-20's I/O chip, two of them, and the predecessor of the CIA. **Casso already has one** -- `CassoEmuCore/Devices/Mockingboard/Via6522.h`, written to know nothing about the card it sits on. |
| **VDC** (8563) | The C128's *second* video chip, driving 80-column RGBI from its own private video RAM that is not on the CPU bus and is reached through two registers. |

---

## 2. What the architecture already gives us

**A machine is data.** `Resources/Machines/<Name>/<Name>.json` carries the CPU,
clock, RAM regions, ROMs, internal devices, ports, video modes and keyboard
type; `ComponentRegistry::RegisterBuiltinDevices` maps type strings to
factories. `MachineScanner` finds any machine directory, the picker lists it,
and `SwitchMachine` swaps to it on the CPU thread. A new machine is a JSON file
plus new registered device types, and touches no machine plumbing.

**The page table is already a PLA.** [`ARCHITECTURE.md` section 3](../ARCHITECTURE.md)
describes the memory model as a memoized decode: page-granular pointers for
passive storage chips, byte-granular device dispatch for reactive ones,
re-derived when a latch changes. The C64's banking is page-aligned by
construction -- BASIC at `$A000`, I/O or character ROM or RAM at `$D000`,
KERNAL at `$E000` -- so a `C64Pla` coordinator is `Apple2eMmu` with different
triggers, a write to `$01` standing in for RAMRD / RAMWRT / ALTZP. Even RAM
under ROM, where writes reach DRAM while reads see ROM, is the language card's
read-ROM / write-RAM split that `MemoryBus` already resolves through separate
read and write page tables.

**Clock derivation already covers them.** `MachineConfig` derives every timing
constant from the 14.31818 MHz crystal over 14 rather than writing a rounded
figure, on the grounds that one oscillator is where all the timing comes from.
The NTSC VIC-20 and the NTSC C64 both run at that same 1,022,727 Hz. PAL needs
its own crystal constants -- 985,248 Hz at 63 cycles per line for the C64,
1,108,404 Hz for the VIC-20 -- and `VideoStandard::PAL` is already parsed out of
the config, though today only the scanline count consumes it.

**The 6522 is done.** `Via6522` is a standalone 16-register chip with two ports,
two timers, a shift register and interrupt flags, deliberately independent of
the Mockingboard. The VIC-20 has two of them and needs nothing else. The 6526
CIA is a different chip and does not derive from it, but it is the same kind of
device and follows the same wiring into `InterruptController`.

**The CPU is nearly there.** The microcode table is a full NMOS 6502 including
the stable undocumented opcodes, gated by Klaus Dormann's functional tests and
Tom Harte's SingleStepTests. C64 software uses those opcodes; most Apple II
emulators never need them. The VIC-20's 6502A runs on the existing core with no
change at all, and the 6510 is that core plus one on-chip I/O port.

**Audio has the right precedent.** The Mockingboard path is a register-driven
chip ticked per instruction whose generator produces samples into the WASAPI
mixer alongside the speaker and the drives. Both the VIC-I's sound section and
the SID's digital half land in that path unchanged.

---

## 3. The four walls

**3.1 The video chip is a bus master, not a bus device.** `VideoOutput::Render`
takes a flat `const Byte * videoRam` and `IVideoMode` renders whole frames or
row ranges out of it. Both Commodore video chips see a *different* address space
than the CPU does: a 16K window with the character ROM shadowed into it, and, on
the C64, a separate 1K of 4-bit color RAM at `$D800` that is not part of the 64K
at all. That is a second decoder over the same DRAM, and neither interface has
room to express it.

**3.2 Whole-frame rendering cannot run C64 software.** Raster interrupts and
mid-frame writes to `$D011` / `$D016` / `$D018` are not a demo-scene edge case;
they are how ordinary games do split screens and smooth scrolling. Rendering has
to move to per-scanline, and sprite-to-sprite and sprite-to-background collision
registers push toward per-cycle. This is the largest single change and it lands
in the hot path the performance model in `ARCHITECTURE.md` is built around.

**3.3 The tick is per-instruction; badlines are sub-instruction.**
`EmulatorShell::ExecuteCpuSlices` steps one instruction, then ticks the
cycle-driven devices with that instruction's cycle count. The comment there
argues, correctly, that slice granularity was too coarse for the Disk II. The
VIC-II makes the same argument one level further down: a badline stalls the CPU
mid-instruction, and sprite DMA steals cycles one at a time. Supporting it means
cycle-stepping the CPU, or at minimum an explicit stall-and-steal hook that the
video chip can drive.

**3.4 The framebuffer is a compile-time constant.**
`ChromeMetrics::kFramebufferWidthPx` is 560 and `kFramebufferHeightPx` is 384,
and those constants are baked into the D3D upload, screenshot capture, the
mouse-to-pixel mapping, and the theme preview. `MachineConfig::VideoConfig`
already carries per-machine width and height that nothing downstream honors. A
VIC-20 is roughly 176x184 and a C64 with borders is roughly 384x272, so that
constant has to become a per-machine value threaded through the present path.

**Two smaller ones.** The keyboard model does not extend: `AppleKeyboard` is an
ASCII latch at `$C000`, while both Commodore machines scan a matrix through an
I/O chip, so the matrix keyboard is a sibling rather than a subclass. And the
character encoding is PETSCII plus screen codes, which reaches the paste path,
`TextEncoding.cpp`, the assembler, and disk cataloging.

---

## 4. Why the VIC-20 goes first

It is not a smaller C64. It misses every wall in section 3 except 3.1 and 3.4:

| | VIC-20 | C64 |
|---|---|---|
| CPU | 6502A -- the existing core, unchanged | 6510: 6502 plus the `$01` port |
| Banking | None. Expansion RAM is static configuration | PLA, driven by the `$01` port |
| I/O | Two 6522 VIAs -- **already implemented** | Two 6526 CIAs -- new chip |
| Sprites | None | Eight, with DMA and collision registers |
| Raster interrupt | None; programs poll `$9004` | Yes, and everything uses it |
| CPU cycle stealing | None | Badlines and sprite DMA |
| Sound | Three squares plus noise, in the VIC-I | SID, with an analog filter |

What it *does* force is exactly the infrastructure the C64 needs afterward:
per-machine framebuffer dimensions, a matrix keyboard, PETSCII and screen codes,
the serial bus and the 1541, cartridge loading at `$A000`, and a video chip with
its own view of memory. Its own quirk -- the memory expansion cartridges that
move the BASIC start address -- is static configuration, so it lands in
`MachineConfig`'s RAM regions rather than in any banking logic.

So it buys the whole shared Commodore foundation at a fraction of the cost, and
it reaches a BASIC prompt without anyone touching the render cadence.

---

## 5. The tiers

| Tier | Work | Gate |
|---|---|---|
| 0 | CLI only: PRG output, D64, PETSCII, headless `run` | No GUI exposure |
| 1 | **VIC-20** end to end | Boots to a prompt, runs cartridges |
| 2 | 1541 -- D64 fast path, then the real drive | Shared from here on |
| 3 | C64 memory: 6510 port, PLA, 6526 CIAs | Boots to a prompt |
| 4 | VIC-II: per-scanline render, badlines, sprites, raster IRQ | The big one |
| 5 | SID | |
| 6 | Chrome: 1702 monitor, cases, 1541 mesh | |
| 7+ | C128D, staged separately | See section 6 |

**Tier 0 -- the CLI.** `CassoCli` already assembles 6502 and already has a
dialect mechanism and a `disk` subcommand. Adding PRG output is a variant of the
existing span logic with a two-byte load address in front; D64 is a new
`DiskFormat` enumerator with a sector geometry rather than a new subsystem; and
the undocumented opcodes are already in the tables, currently `assemblerHidden`,
so a Commodore dialect can simply expose them. This ships value with zero
exposure to the video work, and it exercises the assembler and the CPU before
anything else depends on them.

**Tier 1 -- the VIC-20.** Two `Via6522` instances, a matrix keyboard behind the
existing `InputEventRing`, PETSCII and screen codes, a VIC-I video mode, the
VIC-I sound section into the existing audio path, and cartridge images mapped as
ROM. The framebuffer constant (3.4) has to fall here, because 176x184 breaks it
just as hard as 384x272 would. Per-scanline rendering is worth doing here even
though nothing forces it -- programs poll the raster register and change colors
-- and doing it against a chip with no badlines is a far cheaper place to learn
the render cadence than against the VIC-II.

**Tier 2 -- the 1541.** Two credible stopping points. A `disk-1541` device that
answers the serial bus against a D64 image runs most games and nearly all
productivity software. Full drive emulation -- its own 6502, two VIAs, and a GCR
bit stream -- is what fast loaders and copy-protected titles require, and it is
the only version that is honest about what a 1541 is, which is a whole computer.
Casso's nibble engine, head position model and WOZ-era bit-cell thinking are the
right foundation for the second; Commodore GCR is 4-to-5 rather than Apple's
6-and-2, so the codec differs but the layer does not.

**Tier 3 -- C64 memory.** `Cpu6510` off `Cpu6502`, the PLA coordinator, and two
6526 CIAs. The one friction point is that `Cpu::ReadByte` is a non-virtual
inline fast path over a page-granular table, so the 6510's port at `$00` / `$01`
cannot be device-dispatched without pushing all of zero page onto the slow path.
The likely answer is a write-through shadow: `WriteByte` is already virtual, so
the write updates the banking *and* pokes the visible value back into the zero
page buffer, leaving reads on the fast path. The input bits (the datassette
sense line) then have to be refreshed when that state changes rather than on
read. Worth prototyping before committing, because getting it wrong costs every
zero-page access on every machine.

**Tier 4 -- the VIC-II.** Walls 3.1, 3.2 and 3.3 together. This tier decides
whether the C64 support is real, and it should be planned as its own spec rather
than folded into tier 3.

**Tier 5 -- SID.** The register interface and the digital half drop into the
Mockingboard's audio path. The analog filter is the multi-week item, and see
section 7.

**Tier 6 -- chrome.** A 1702 monitor mesh, VIC-20 and breadbin cases, a 1541
mesh, and `MonitorCatalog` entries. Note that the desk scene's composition
assumption does not carry: the Apple stack is a monitor sitting on drives, while
these machines *are* the keyboard, with the drive beside them.

---

## 6. The C128D is its own program

Not a C64 variant. It is three machines in one case.

- **8502** -- a 6510 with a 2 MHz mode. Cheap.
- **MMU 8722** -- 128K banked, with common-RAM windows. The page table handles
  it; more banks, the same pattern.
- **VDC 8563** -- a second video chip with private VRAM off the CPU bus. The
  shell has one framebuffer and one monitor mesh; real C128D owners ran two
  displays or a switch.
- **Z80B** -- a second CPU for CP/M. `ICpu` is a 6502-family abstraction and
  `CpuManager` assumes one CPU, so this is a new core plus bus arbitration, not
  an extension.
- **1571** -- built into the case, with burst mode and double-sided MFM, on top
  of everything tier 2 does.
- **Three modes** -- C128, C64 and CP/M. `MachineConfig` has no concept of a
  machine that reconfigures into a different machine.

Order within it: C128 running in C64 mode, then native 40-column, then the VDC's
80 columns, then the 1571, then Z80 and CP/M if ever.

---

## 7. Hazards

**reSID is GPL-2, and so is VICE.** They are the obvious things to reach for and
either would poison the license. The clean-room rule already in force for the
Mockingboard applies here with more weight: write from datasheets and published
measurements, and consult an existing implementation for observed *behavior*
only while debugging, never for code. This applies to the SID filter above all,
because that is precisely where the temptation is strongest.

**Do not run this on a long-lived branch.** `CLAUDE.md` records four sweeping
accessor renames on master, where textual conflicts badly understate the work --
renames merge cleanly and then fail to compile, one round of call sites at a
time. A multi-tier port is exactly the branch that gets hurt. Merge master
weekly at minimum, and run `scripts/CheckStyle.ps1 -Mode Tree` before each one.

**The framebuffer constant has a long tail.** Grep before estimating: it reaches
the D3D uploader, the screenshot path, the mouse mapping and the theme preview,
and each of those has its own aspect-ratio arithmetic around it.

---

## 8. Decisions not made

- Whether the VIC-20 ships as a public machine or as scaffolding for the C64.
  The tiering above assumes public, which means the machine picker carries
  Commodore entries before the C64 is playable.
- Whether tier 2 stops at the D64 fast path or goes to full 1541 emulation. The
  fast path is most of the value; the full drive is most of the work.
- Whether PAL is a first-class option per machine or a fixed property of each
  machine definition.
- Whether the C128D's Z80 is ever worth building.
- `README.md` currently lists "Commodore VIC-20, 64" under *On the horizon* with
  no ordering. If VIC-20-first is adopted, that line should say so.
