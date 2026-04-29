# Feature Specification: Apple II Platform Emulator

**Feature Branch**: `003-apple2-platform-emulator`  
**Created**: 2025-07-22  
**Status**: Draft  
**Input**: User description: "Add a GUI-based Apple II platform emulator (Casso65Emu) to the Casso65 6502 emulator project"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot Apple II+ to BASIC Prompt (Priority: P1)

A user launches Casso65Emu with an Apple II+ machine configuration and the appropriate ROM image. The emulator window opens, the CPU executes the ROM boot sequence, and the familiar 40-column text display appears with the Applesoft BASIC `]` prompt. The user can type BASIC commands using their PC keyboard and see the output on screen.

**Why this priority**: This is the foundational "first visible output" that proves the entire architecture works end-to-end — CPU execution, memory bus routing, ROM loading, keyboard input, text video rendering, and Win32 display. Every subsequent feature builds on this.

**Independent Test**: Can be fully tested by launching `Casso65Emu.exe --machine apple2plus` with a valid ROM file and verifying the `]` prompt appears. The user can type `PRINT "HELLO"` and see the output. Delivers a working text-mode Apple II+ emulator.

**Acceptance Scenarios**:

1. **Given** a valid Apple II+ ROM file is present in the configured ROM directory, **When** the user runs `Casso65Emu.exe --machine apple2plus`, **Then** a window opens displaying 40-column green-on-black text with the Applesoft BASIC `]` prompt within 3 seconds
2. **Given** the emulator is running at the `]` prompt, **When** the user types `PRINT "HELLO WORLD"` and presses Enter, **Then** the text `HELLO WORLD` appears on the next line followed by a new `]` prompt
3. **Given** the emulator is running, **When** the user presses Ctrl+Reset, **Then** the emulator resets and returns to the `]` prompt
4. **Given** the machine config specifies a CPU clock speed of 1.023 MHz, **When** the emulator runs, **Then** the emulation speed closely approximates real Apple II timing (cursor blink rate and keystroke responsiveness feel authentic)

---

### User Story 2 - Speaker Audio Output (Priority: P2)

A user runs a BASIC program that produces sound (e.g., `FOR I=1 TO 100: S=PEEK(49200): NEXT`) and hears audio output through their PC speakers. The speaker toggle at $C030 produces square-wave audio at the correct pitch and duration.

**Why this priority**: Sound provides immediate user feedback and demonstrates the soft-switch framework is working correctly. It is a small, self-contained feature that enhances the experience significantly.

**Independent Test**: Can be tested by running a BASIC program that toggles the speaker address and verifying audible output. Delivers audio capability to the text-mode emulator.

**Acceptance Scenarios**:

1. **Given** the emulator is running at the BASIC prompt, **When** the user runs a program that reads address $C030 repeatedly, **Then** audible sound is produced through the PC speakers
2. **Given** the speaker is producing sound, **When** the toggle rate changes, **Then** the audio pitch changes correspondingly

---

### User Story 3 - Lo-Res Graphics Display (Priority: P3)

A user runs a BASIC program that uses lo-res graphics commands (`GR`, `COLOR=`, `PLOT`, `HLIN`, `VLIN`) and sees a 40×48 grid of colored blocks on the emulator display. All 16 Apple II colors render correctly.

**Why this priority**: Lo-res graphics is the simplest graphics mode, sharing memory with the text page. It proves the video mode switching (soft switches $C050–$C053) works and establishes the pattern for hi-res mode later.

**Independent Test**: Can be tested by entering `GR` at the BASIC prompt and running plotting commands. Delivers lo-res graphics capability.

**Acceptance Scenarios**:

1. **Given** the emulator is at the BASIC prompt, **When** the user types `GR`, **Then** the display switches to lo-res graphics mode with a 4-line text window at the bottom
2. **Given** lo-res mode is active, **When** the user runs `COLOR=1: PLOT 0,0`, **Then** a magenta block appears at the top-left corner
3. **Given** lo-res mode is active, **When** the user draws with all 16 color values (0–15), **Then** each color is visually distinct and matches standard Apple II colors

---

### User Story 4 - Language Card and DOS/ProDOS Loading (Priority: P4)

A user boots with a disk image containing DOS 3.3 or ProDOS. The Language Card bank-switching logic allows the operating system to load into the upper 16KB of RAM, and the `CATALOG` command lists files on the disk.

**Why this priority**: Bank-switching and disk access unlock the ability to run real Apple II software, which is the primary use case for most emulator users. This transforms the emulator from a BASIC interpreter into a platform that runs historical software.

**Independent Test**: Can be tested by launching with `--disk1 dos33.dsk` and verifying that DOS boots, then running `CATALOG` to list disk contents.

**Acceptance Scenarios**:

1. **Given** a DOS 3.3 system disk image, **When** the user runs `Casso65Emu.exe --machine apple2plus --disk1 dos33.dsk`, **Then** DOS 3.3 boots and displays the `]` prompt with the DOS greeting
2. **Given** DOS is running, **When** the user types `CATALOG`, **Then** the files on the disk image are listed correctly
3. **Given** a .dsk file with a BASIC program, **When** the user types `RUN PROGRAM`, **Then** the program loads from disk and executes
4. **Given** a write-protected disk image, **When** a program attempts to write to disk, **Then** the emulator reports a disk write-protection error consistent with real Apple II behavior

---

### User Story 5 - Hi-Res Graphics with Color (Priority: P5)

A user runs a program that uses hi-res graphics (e.g., `HGR`, `HPLOT`, or a game loaded from disk). The 280×192 display renders with NTSC color artifacting simulation, producing the characteristic Apple II color fringes.

**Why this priority**: Hi-res graphics are essential for running most Apple II games and productivity software. NTSC color artifacting is a defining visual characteristic of the platform.

**Independent Test**: Can be tested by entering `HGR` and using `HPLOT` commands or loading a hi-res image from disk.

**Acceptance Scenarios**:

1. **Given** the emulator is at the BASIC prompt, **When** the user types `HGR`, **Then** the display switches to hi-res mode showing a 280×192 graphics area with 4 lines of text at the bottom
2. **Given** hi-res mode is active, **When** a program draws alternating pixel patterns, **Then** NTSC color artifacting produces visible color fringes consistent with real Apple II output
3. **Given** hi-res mode is active, **When** the palette bit (bit 7) is toggled in a byte, **Then** the color set shifts between violet/green and blue/orange groups

---

### User Story 6 - Apple IIe Extended Features (Priority: P6)

A user selects the Apple IIe machine configuration and gets access to 128KB RAM, 80-column text mode, double hi-res graphics, lowercase keyboard input, and Open/Closed Apple keys. Programs that require IIe-specific features run correctly.

**Why this priority**: The Apple IIe was the most popular model and is needed to run a large portion of the Apple II software library. It demonstrates the extensibility of the data-driven machine configuration architecture.

**Independent Test**: Can be tested by launching with `--machine apple2e`, verifying 80-column text mode with `PR#3`, and testing lowercase input.

**Acceptance Scenarios**:

1. **Given** the Apple IIe machine config, **When** the user runs `Casso65Emu.exe --machine apple2e`, **Then** the emulator boots with the Apple IIe ROM and supports lowercase keyboard input by default
2. **Given** IIe mode, **When** the user activates 80-column mode (e.g., `PR#3`), **Then** the display switches to 80×24 character mode
3. **Given** IIe mode with 80-column active, **When** a program uses double hi-res graphics, **Then** 560×192 resolution graphics render with 16 colors
4. **Given** IIe mode, **When** the user presses the keys mapped to Open Apple and Closed Apple, **Then** the corresponding button state is readable by software at the standard addresses

---

### User Story 7 - Data-Driven Machine Configuration (Priority: P7)

A developer creates a new machine configuration JSON file and registers new device components in the factory registry. The emulator loads the new configuration without any changes to the core emulator shell or existing machine configs.

**Why this priority**: The extensible architecture is a core design goal, enabling future machines (C64, NES, etc.) to be added. While it's tested implicitly by the Apple II configurations, an explicit validation of the plug-in architecture ensures maintainability.

**Independent Test**: Can be tested by creating a minimal custom machine config JSON with RAM, ROM, and a text display, and verifying it loads and runs.

**Acceptance Scenarios**:

1. **Given** a valid machine config JSON file with a unique machine name, **When** the user references it via `--machine` on the command line, **Then** the emulator loads the configuration and instantiates the specified devices
2. **Given** a machine config references a device type name that exists in the component registry, **When** the emulator initializes, **Then** the device is created and wired to the memory bus at the configured address range
3. **Given** a machine config references a device type name that does not exist in the registry, **When** the emulator initializes, **Then** a clear error message names the unknown device type and the emulator exits gracefully

---

### Edge Cases

- What happens when the user specifies `--machine` with a config name that does not exist? → The emulator displays a helpful error listing available machine configurations and exits
- What happens when the ROM file referenced in the machine config is missing? → The emulator reports which ROM file is missing, its expected location, and exits with a clear error message
- What happens when the user provides a .dsk image that is corrupted or an unrecognized format? → The emulator reports the disk error and continues running (the drive is simply empty)
- What happens when the emulator window is resized? → The display scales proportionally, maintaining the correct aspect ratio, or the window has a fixed size
- What happens when the user closes the emulator window while a disk write is in progress? → The emulator completes or discards the pending operation and exits cleanly without corrupting the disk image
- What happens when two devices are configured with overlapping address ranges? → The emulator detects the conflict at startup and reports which devices and addresses overlap, then exits
- What happens when the CPU executes an illegal/undefined opcode (NMOS 6502)? → The emulator treats it as a NOP of the appropriate byte/cycle length (non-crashing behavior), consistent with common emulator practice
- What happens when a program accesses the $C000–$CFFF I/O space at an address not claimed by any device? → The MemoryBus returns a floating bus value (typically the last value on the data bus) rather than crashing

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a separate GUI executable (`Casso65Emu`) that links against the existing Casso65Core static library without modifying Casso65Core's public API
- **FR-002**: System MUST load machine configurations from JSON files that define memory map, ROM file paths, CPU type, clock speed, device wiring, video geometry, and keyboard type
- **FR-003**: System MUST implement a component registry where named device types are registered as C++ classes and referenced by name in machine config files
- **FR-004**: System MUST route all CPU memory access through a MemoryBus that delegates reads and writes to the appropriate MemoryDevice based on address
- **FR-005**: System MUST implement RAM and ROM as MemoryDevice components that can be placed at configurable address ranges
- **FR-006**: System MUST implement Apple II soft switches as MemoryDevice components that respond to reads/writes at their configured addresses to toggle video modes, read keyboard state, and control other hardware functions
- **FR-007**: System MUST render Apple II text mode (40×24 characters) with the correct interleaved memory layout ($0400–$07FF) including the non-sequential row addressing
- **FR-008**: System MUST render Apple II lo-res graphics mode (40×48, 16 colors) from the text page memory, correctly decoding each byte as two 4-bit color nybbles
- **FR-009**: System MUST render Apple II hi-res graphics mode (280×192) from $2000–$3FFF, decoding 7 pixels per byte with palette bit control and simulating NTSC color artifacting
- **FR-010**: System MUST display the video output in a Win32 window using Direct3D 11 rendering, at a resolution of 560×384 (2× scaling of 280×192)
- **FR-011**: System MUST translate PC keyboard input to Apple II keyboard codes and make them available through the keyboard soft switch at $C000, with the strobe cleared on read of $C010
- **FR-012**: System MUST produce audio output by toggling the speaker state on reads of $C030, generating a square-wave signal through the Windows audio system
- **FR-013**: System MUST implement Language Card bank-switching (soft switches $C080–$C08F) to support 16KB of switchable RAM in the $D000–$FFFF address range
- **FR-014**: System MUST emulate the Disk II controller in slot 6, supporting reading and writing .dsk (DOS-order 140KB) disk images
- **FR-015**: System MUST support Apple IIe extensions including 128KB memory (64KB auxiliary RAM), 80-column text mode, double hi-res graphics, lowercase keyboard, and Open/Closed Apple keys — all configured through the Apple IIe machine config file
- **FR-016**: System MUST accept command-line arguments: `--machine <name>` (required), `--disk1 <path>` (optional), `--disk2 <path>` (optional)
- **FR-017**: System MUST NOT depend on any third-party libraries; only the Windows SDK and C++ Standard Library are permitted
- **FR-018**: System MUST validate machine config files at startup and report clear, actionable errors for missing files, unknown device types, overlapping address ranges, and malformed JSON
- **FR-019**: System MUST preserve all existing Casso65 project functionality — the existing 577+ unit tests must continue to pass with no changes to Casso65Core's public API
- **FR-020**: System MUST integrate with the existing Casso65Core `Cpu` class by subclassing it (e.g., `EmuCpu`) and overriding the memory access methods (`ReadByte`, `WriteByte`, `ReadWord`, `WriteWord`) to route through the MemoryBus instead of the flat `memory[]` array. The base `Cpu` methods must be made `virtual` (a non-breaking change to the protected interface — no public API change). The existing `PeekByte`/`PokeByte` public accessors and all unit tests remain unaffected.
- **FR-021**: System MUST support the 65C02 instruction set (required for Apple IIe) by extending the opcode table with 65C02-specific opcodes (e.g., `PHX`, `PHY`, `PLX`, `PLY`, `STZ`, `BRA`, `TRB`, `TSB`, new addressing modes). This can be an `EmuCpu` initialization option selected by the `"cpu": "65c02"` config field. The NMOS 6502 instruction set used by existing unit tests is unchanged.
- **FR-022**: System MUST display a Win32 window with a title bar showing the machine name and emulation state (e.g., "Casso65 — Apple II+ [Running]"), a menu bar (see Menu Hierarchy below), and a fixed-size client area of 560×384 pixels. No window resizing is supported in the initial implementation.

### Menu Hierarchy

```
File
├── Open Machine Config...     Ctrl+O
├── ─────────────────────
├── Recent Machines           ►
├── ─────────────────────
└── Exit                       Alt+F4

Machine
├── Reset                      Ctrl+R
├── Power Cycle                Ctrl+Shift+R
├── ─────────────────────
├── Pause                      Pause
├── Step (1 instruction)       F11          (when paused)
├── ─────────────────────
├── Speed: 1× (authentic)
├── Speed: 2×
├── Speed: Maximum
├── ─────────────────────
└── Machine Info...

Disk
├── Drive 1: Insert...         Ctrl+1
├── Drive 1: Eject             Ctrl+Shift+1
├── Drive 1: Write Protect
├── ─────────────────────
├── Drive 2: Insert...         Ctrl+2
├── Drive 2: Eject             Ctrl+Shift+2
└── Drive 2: Write Protect

View
├── Fullscreen                 Alt+Enter
├── ─────────────────────
├── Filter: Nearest Neighbor   (crisp pixels)
├── Filter: CRT Shader         (future — scanlines/bloom)
├── ─────────────────────
├── Color Mode: Color
├── Color Mode: Green Mono
├── Color Mode: Amber Mono
└── Color Mode: White Mono

Help
├── Keyboard Map...            F1
└── About Casso65...
```

Menu items that depend on unimplemented features (e.g., CRT Shader) are grayed out until the feature is available. Speed and Color Mode items use radio-button check marks to show the current selection.
- **FR-023**: System MUST run the emulation loop synchronized to real-time speed by executing the correct number of CPU cycles per video frame (1,023,000 Hz ÷ ~60 Hz ≈ 17,050 cycles per frame), rendering the framebuffer, and sleeping for the remainder of the frame period. The loop runs on the main thread integrated with the Win32 message pump.
- **FR-024**: System MUST generate audio from speaker toggles by accumulating toggle timestamps during each frame's CPU execution, converting them to a PCM waveform, and submitting audio buffers via the Windows `waveOut` API. The audio buffer size should target low-latency output (≤50 ms).
- **FR-025**: System MUST map slot-based devices to both their I/O range ($C080+slot×16 through $C08F+slot×16 → e.g., slot 6 maps to $C0E0–$C0EF) and their slot ROM range ($Cs00–$CsFF where s is the slot number → e.g., slot 6 maps to $C600–$C6FF). The Disk II controller's slot ROM contains the boot code that the CPU executes when booting from disk.
- **FR-026**: System MUST support an original Apple II machine configuration that is identical to the Apple II+ except with the Integer BASIC ROM instead of the Applesoft BASIC ROM. The `--machine apple2` argument selects this configuration.

### Key Entities

- **MachineConfig**: Represents a complete machine definition loaded from JSON — includes CPU type, clock speed, memory layout, device list, video configuration, and keyboard type
- **MemoryBus**: Central address-space router that maps address ranges to MemoryDevice instances; handles read/write dispatch and address conflict detection
- **MemoryDevice**: Interface for any component on the address bus (RAM, ROM, soft switches, slot cards) — supports Read(address) and Write(address, value) operations
- **ComponentRegistry**: Factory registry mapping string device type names to C++ class constructors; used by the machine config loader to instantiate devices
- **VideoOutput**: Interface for video renderers that read video RAM and produce RGBA pixel framebuffers; each video mode (text, lo-res, hi-res, double hi-res) is a separate implementation
- **DiskImage**: Represents a mounted .dsk file with sector read/write capability; manages file I/O and protects against corruption on unexpected exit
- **EmuCpu**: Subclass of the existing Casso65Core `Cpu` that overrides `ReadByte`/`WriteByte`/`ReadWord`/`WriteWord` to route through the MemoryBus. Constructed with a reference to the MemoryBus. Supports both NMOS 6502 and 65C02 instruction sets based on the machine config's `cpu` field.
- **EmulatorShell**: The main application class that owns the Win32 window, MemoryBus, EmuCpu, video renderers, and input/audio subsystems. Runs the emulation loop and coordinates frame timing.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can boot an Apple II+ emulation to the Applesoft BASIC prompt and interact with it within 3 seconds of launching the application
- **SC-002**: Users can type and execute BASIC programs with keyboard response latency indistinguishable from a real Apple II (under 50 milliseconds perceived delay)
- **SC-003**: Users can boot DOS 3.3 or ProDOS from a .dsk image and run CATALOG, LOAD, and RUN commands successfully
- **SC-004**: All 16 Apple II lo-res colors display correctly and are visually distinguishable on a modern monitor
- **SC-005**: Hi-res graphics programs produce color output consistent with NTSC artifacting behavior documented in Apple II technical references
- **SC-006**: Emulation runs at 100% real-time speed (1.023 MHz effective) on any modern Windows PC without frame drops during normal operation
- **SC-007**: All existing 577+ Casso65 unit tests continue to pass after adding the new project
- **SC-008**: A developer can add support for a new machine type by creating a JSON config file and writing device component classes, without modifying the emulator shell, MemoryBus, or any existing machine configs
- **SC-009**: Apple IIe emulation supports 80-column text mode and double hi-res graphics, running software that requires these features
- **SC-010**: The emulator exits cleanly when encountering configuration errors, providing error messages that identify the specific problem (missing ROM, unknown device, invalid JSON) and suggest corrective action
- **SC-011**: The emulator window displays a menu bar that allows inserting and ejecting disk images, resetting the machine, and pausing/resuming emulation without using the command line
- **SC-012**: Apple II (original) boots to the Integer BASIC `>` prompt using the same emulator binary and architecture, differing only in the machine config JSON and ROM file

## Technology Decisions

These are binding architectural choices made during design. They constrain all downstream planning and implementation.

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Rendering API** | Direct3D 11 | Windows SDK built-in (not third-party). Enables future CRT shader effects (scanlines, bloom, curvature) via HLSL pixel shaders. Simpler than GDI for scaled framebuffer output. |
| **Window framework** | Raw Win32 (`CreateWindowEx`, message pump, `CreateMenu`) | Zero dependencies. The window is primarily a D3D11 viewport with a menu bar — minimal UI. Can be upgraded to a richer framework later without affecting the rendering or emulation layers. |
| **Audio API** | Windows `waveOut` (or `WASAPI` if latency requires it) | Built-in, no dependencies. Speaker emulation is a simple square wave toggle. |
| **Third-party libraries** | None | Consistent with Casso65 project policy. Only Windows SDK + C++ STL. |
| **Machine configuration** | Data-driven JSON files + component registry | New machines added via config + device components. No machine-specific subclasses in the emulator shell. |
| **Threading model** | Single-threaded | CPU execution, video rendering, audio generation, and Win32 message pump all on the main thread. Sufficient for 1 MHz Apple II emulation on modern hardware. |

## Assumptions

- Users provide their own Apple II ROM images (these are copyrighted and will not be distributed with the project); ROM files are gitignored
- The emulator targets Windows 10/11 on x64 and ARM64 architectures, consistent with existing Casso65 platform support
- Direct3D 11 (part of the Windows SDK, not third-party) is used for rendering. A D3D11 device, swap chain, and single textured quad provide the display pipeline, enabling future CRT shader effects (scanlines, bloom, curvature) via HLSL. This approach is sufficient for the 1 MHz Apple II's display refresh requirements; no GPU acceleration is needed
- The existing Casso65Core 6502 CPU emulation is cycle-accurate enough to run Apple II software correctly; if timing adjustments are needed they will be addressed as bugs, not as spec changes
- Disk II emulation supports the standard 140KB .dsk format (DOS-order, 16 sectors × 35 tracks); other formats (e.g., .nib, .woz) are out of scope for the initial implementation
- Audio output uses the Windows waveOut or similar built-in audio API; low-latency audio is desirable but not a hard requirement for initial delivery
- The Apple II+ and original Apple II differ only in ROM content (Applesoft vs. Integer BASIC); both use the same machine config structure with different ROM file references
- The focus is on accurate functional emulation, not cycle-exact hardware reproduction; minor timing differences that don't affect software compatibility are acceptable
- The emulation loop is single-threaded — CPU execution, video rendering, and audio generation all happen on the main thread, interleaved with Win32 message processing. This is sufficient for the 1 MHz Apple II target on modern hardware.
- Making `Cpu::ReadByte`/`WriteByte`/`ReadWord`/`WriteWord` virtual is a safe change to the protected (non-public) interface — it does not affect the existing Casso65 CLI or unit tests, which continue to use the base `Cpu` class with its flat memory array.
- The original Apple II (Integer BASIC) is a lower-priority configuration — most users will use Apple II+ or IIe. It is included for completeness but shares all components with the Apple II+ except the ROM file.

## Apple II Hardware Memory Maps

These maps define the physical address space for each target machine. Every address range must be covered by a `MemoryDevice` on the `MemoryBus`. The JSON config files encode these maps; this section is the authoritative hardware reference.

### Apple II / Apple II+ Memory Map (48KB + Language Card)

| Address Range | Size | Device | Notes |
|---------------|------|--------|-------|
| `$0000–$00FF` | 256B | RAM | Zero page — fast addressing |
| `$0100–$01FF` | 256B | RAM | Stack |
| `$0200–$02FF` | 256B | RAM | Input buffer |
| `$0300–$03FF` | 256B | RAM | Free / DOS vectors |
| `$0400–$07FF` | 1KB | RAM | **Text Page 1 / Lo-Res Page 1** — interleaved row layout |
| `$0800–$0BFF` | 1KB | RAM | **Text Page 2 / Lo-Res Page 2** |
| `$0C00–$1FFF` | 5KB | RAM | Free |
| `$2000–$3FFF` | 8KB | RAM | **Hi-Res Page 1** |
| `$4000–$5FFF` | 8KB | RAM | **Hi-Res Page 2** |
| `$6000–$BFFF` | 24KB | RAM | Free (programs, variables) |
| `$C000–$C00F` | 16B | Keyboard | `$C000` = key data (bit 7 = strobe), `$C010` = strobe clear |
| `$C010–$C01F` | 16B | Keyboard | Strobe clear (any read), button inputs |
| `$C020–$C02F` | 16B | I/O | Cassette, utility strobe |
| `$C030–$C03F` | 16B | Speaker | **Any read toggles speaker** |
| `$C040–$C04F` | 16B | I/O | Game I/O (paddles, buttons) |
| `$C050` | 1B | Soft Switch | Graphics mode ON |
| `$C051` | 1B | Soft Switch | Text mode ON |
| `$C052` | 1B | Soft Switch | Full-screen (no mixed) |
| `$C053` | 1B | Soft Switch | Mixed mode (4 lines text at bottom) |
| `$C054` | 1B | Soft Switch | Display Page 1 |
| `$C055` | 1B | Soft Switch | Display Page 2 |
| `$C056` | 1B | Soft Switch | Lo-Res mode |
| `$C057` | 1B | Soft Switch | Hi-Res mode |
| `$C058–$C05F` | 8B | Soft Switch | Annunciator outputs |
| `$C060–$C06F` | 16B | I/O | Paddle/button read |
| `$C070–$C07F` | 16B | I/O | Paddle timer reset |
| `$C080–$C08F` | 16B | Language Card | **Bank-switch controls** — selects RAM/ROM in $D000–$FFFF |
| `$C090–$C0FF` | 112B | Slot I/O | `$C0n0–$C0nF` = slot _n_ device I/O (n=1–7) |
| `$C100–$C7FF` | 1.75KB | Slot ROM | `$Cn00–$CnFF` = 256B ROM per slot (n=1–7) |
| `$C800–$CFFF` | 2KB | Slot Expansion | Shared expansion ROM space (active slot selected by `$Cn00` access) |
| `$D000–$D7FF` | 2KB | ROM or RAM | ROM: Applesoft (II+) / Integer BASIC (II). RAM: Language Card bank 2 |
| `$D800–$DFFF` | 2KB | ROM or RAM | ROM: Applesoft continued. RAM: Language Card |
| `$E000–$FFFF` | 8KB | ROM or RAM | ROM: Applesoft + Monitor. RAM: Language Card |
| `$FFFC–$FFFD` | 2B | ROM | **Reset vector** — CPU reads this on power-on |

### Apple IIe Additions

The IIe extends the II+ map with auxiliary memory and additional soft switches:

| Address Range | Device | Notes |
|---------------|--------|-------|
| `$C000` | Keyboard | Same as II+, but supports **lowercase** and modifier keys |
| `$C003` / `$C004` | Aux RAM Card | Read main RAM / Read aux RAM for `$0200–$BFFF` |
| `$C005` / `$C006` | Aux RAM Card | Write aux RAM / Write main RAM for `$0200–$BFFF` |
| `$C00C` / `$C00D` | Soft Switch | 40-column / 80-column mode |
| `$C00E` / `$C00F` | Soft Switch | Character set: primary / alternate (MouseText) |
| `$C054` / `$C055` | Soft Switch | Page 1 / Page 2 (also selects main/aux for $0400–$07FF and $2000–$3FFF) |
| `$C05E` / `$C05F` | Soft Switch | Double hi-res ON / OFF |
| `$C061` | Input | Open Apple button state |
| `$C062` | Input | Closed Apple button state |
| `$C07E` / `$C07F` | Soft Switch | IOU disable / enable |

The IIe has a **separate 64KB auxiliary RAM bank**. When active, reads and/or writes to `$0200–$BFFF` are redirected to auxiliary memory instead of main RAM. The video hardware reads from both banks simultaneously for 80-column text and double hi-res modes.

### Text Mode Interleaved Row Layout

The Apple II text/lo-res display maps 24 rows to non-contiguous memory. This applies to all three machines:

| Screen Row | Memory Address (Page 1) | Memory Address (Page 2) |
|------------|------------------------|------------------------|
| 0 | `$0400` | `$0800` |
| 1 | `$0480` | `$0880` |
| 2 | `$0500` | `$0900` |
| 3 | `$0580` | `$0980` |
| 4 | `$0600` | `$0A00` |
| 5 | `$0680` | `$0A80` |
| 6 | `$0700` | `$0B00` |
| 7 | `$0780` | `$0B80` |
| 8 | `$0428` | `$0828` |
| 9 | `$04A8` | `$08A8` |
| 10 | `$0528` | `$0928` |
| 11 | `$05A8` | `$09A8` |
| 12 | `$0628` | `$0A28` |
| 13 | `$06A8` | `$0AA8` |
| 14 | `$0728` | `$0B28` |
| 15 | `$07A8` | `$0BA8` |
| 16 | `$0450` | `$0850` |
| 17 | `$04D0` | `$08D0` |
| 18 | `$0550` | `$0950` |
| 19 | `$05D0` | `$09D0` |
| 20 | `$0650` | `$0A50` |
| 21 | `$06D0` | `$0AD0` |
| 22 | `$0750` | `$0B50` |
| 23 | `$07D0` | `$0BD0` |

Formula: `base + 128*(row%8) + 40*(row/8)` where base is `$0400` (page 1) or `$0800` (page 2). Hi-res uses the same pattern with base `$2000`/`$4000` and 8 pixel-rows per text row (192 total).

### Hi-Res Color Encoding

Each byte in hi-res memory encodes 7 pixels plus a palette selector:

```
Bit 7: Palette select (0 = violet/green group, 1 = blue/orange group)
Bits 6–0: 7 pixels, LSB = leftmost

Palette 0: even-column ON = violet, odd-column ON = green
Palette 1: even-column ON = blue,   odd-column ON = orange
Adjacent ON pixels merge to white.
Adjacent OFF pixels remain black.
```

### Lo-Res Color Palette

Each byte encodes two vertically-stacked 4-bit color blocks (top nybble, bottom nybble):

| Value | Color | Value | Color |
|-------|-------|-------|-------|
| 0 | Black | 8 | Brown |
| 1 | Magenta | 9 | Orange |
| 2 | Dark Blue | 10 | Grey 1 |
| 3 | Purple | 11 | Pink |
| 4 | Dark Green | 12 | Green |
| 5 | Grey 2 | 13 | Yellow |
| 6 | Medium Blue | 14 | Aqua |
| 7 | Light Blue | 15 | White |

## Machine Configuration File Schema

Machine configs are JSON files stored in `Casso65Emu/machines/`. Each file defines the complete hardware configuration for one computer model. The emulator loads the config at startup based on the `--machine` argument.

### Config Fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Human-readable machine name (e.g., "Apple II+") |
| `cpu` | string | CPU variant: `"6502"`, `"65c02"`, `"nes2a03"` |
| `clockSpeed` | int | Clock speed in Hz (e.g., 1023000 for Apple II) |
| `memory` | array | RAM and ROM region definitions |
| `memory[].type` | string | `"ram"` or `"rom"` |
| `memory[].start` | string | Hex start address (e.g., `"0x0000"`) |
| `memory[].end` | string | Hex end address (e.g., `"0xBFFF"`) |
| `memory[].file` | string | ROM filename (rom type only), resolved relative to roms directory |
| `devices` | array | Named device instances wired to the address bus |
| `devices[].type` | string | Component registry name (e.g., `"apple2-keyboard"`) |
| `devices[].address` | string | Single address for point-mapped devices |
| `devices[].start` / `devices[].end` | string | Address range for range-mapped devices |
| `devices[].slot` | int | Slot number for slot-based devices (Apple II slots 1–7) |
| `video` | object | Video output configuration |
| `video.modes` | array | List of video mode component names (e.g., `["apple2-text40", "apple2-lores", "apple2-hires"]`) |
| `video.width` | int | Display framebuffer width in pixels |
| `video.height` | int | Display framebuffer height in pixels |
| `keyboard` | object | Keyboard input configuration |
| `keyboard.type` | string | Keyboard component name (e.g., `"apple2-uppercase"`, `"apple2e-full"`) |

### Example: Apple II+ Config

```json
{
    "name": "Apple II+",
    "cpu": "6502",
    "clockSpeed": 1023000,
    "memory": [
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF" },
        { "type": "rom",  "file": "apple2plus.rom", "start": "0xD000", "end": "0xFFFF" }
    ],
    "devices": [
        { "type": "apple2-keyboard",      "address": "0xC000" },
        { "type": "apple2-speaker",       "address": "0xC030" },
        { "type": "apple2-softswitches",  "start": "0xC050", "end": "0xC05F" },
        { "type": "language-card",        "start": "0xC080", "end": "0xC08F" },
        { "type": "disk-ii",             "slot": 6 }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-lores", "apple2-hires"],
        "width": 560,
        "height": 384
    },
    "keyboard": {
        "type": "apple2-uppercase"
    }
}
```

### Example: Apple IIe Config (differences from II+)

```json
{
    "name": "Apple IIe",
    "cpu": "65c02",
    "clockSpeed": 1023000,
    "memory": [
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF" },
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF", "bank": "aux" },
        { "type": "rom",  "file": "apple2e.rom",      "start": "0xC100", "end": "0xFFFF" },
        { "type": "rom",  "file": "apple2e-char.rom",  "start": "0x0000", "end": "0x07FF", "target": "chargen" }
    ],
    "devices": [
        { "type": "apple2e-keyboard",     "address": "0xC000" },
        { "type": "apple2-speaker",       "address": "0xC030" },
        { "type": "apple2e-softswitches", "start": "0xC050", "end": "0xC05F" },
        { "type": "aux-ram-card",         "start": "0xC003", "end": "0xC005" },
        { "type": "language-card",        "start": "0xC080", "end": "0xC08F" },
        { "type": "disk-ii",             "slot": 6 }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-text80", "apple2-lores", "apple2-hires", "apple2-doublehires"],
        "width": 560,
        "height": 384
    },
    "keyboard": {
        "type": "apple2e-full"
    }
}
```

## Component Registry Architecture

The component registry maps string type names from the config to C++ factory functions. This is the boundary between data-driven configuration and code-driven behavior.

### Components that require code (behavioral logic)

These components contain state machines, hardware protocols, or rendering algorithms that cannot be expressed as data.

| Config type string | C++ class | Behavior requiring code |
|--------------------|-----------|------------------------|
| `"apple2-keyboard"` | `AppleKeyboard` | Host-to-Apple key mapping, ASCII encoding, strobe latch at $C000/$C010 |
| `"apple2e-keyboard"` | `AppleIIeKeyboard` | Adds lowercase, modifier keys (Open/Closed Apple), auto-repeat |
| `"apple2-speaker"` | `AppleSpeaker` | Toggle state on $C030 read, audio sample generation |
| `"apple2-softswitches"` | `AppleSoftSwitchBank` | Video mode toggles ($C050–$C057), mixed mode, page select — state machine driving video mode selection |
| `"apple2e-softswitches"` | `AppleIIeSoftSwitchBank` | Extends with 80-col, aux RAM bank select, IIe-specific switches |
| `"language-card"` | `LanguageCard` | Bank-switching state machine: RAM/ROM select, write-enable sequencing at $C080–$C08F |
| `"aux-ram-card"` | `AuxRamCard` | IIe auxiliary 64KB RAM bank switching at $C003/$C005 |
| `"disk-ii"` | `DiskIIController` | Stepper motor state machine, read/write head, nibble encoding/decoding, .dsk sector translation |
| `"apple2-text40"` | `AppleTextMode` | Interleaved row address decoding, character ROM lookup, cursor blink |
| `"apple2-text80"` | `Apple80ColTextMode` | Interleaved main/aux memory for 80-column display |
| `"apple2-lores"` | `AppleLoResMode` | Nybble-to-color decoding, 16-color palette |
| `"apple2-hires"` | `AppleHiResMode` | 7-pixels-per-byte decoding, palette bit, NTSC color artifacting algorithm |
| `"apple2-doublehires"` | `AppleDoubleHiResMode` | 560×192 from interleaved main/aux memory, 16-color palette |

### Components that are purely data-driven

These are generic and fully configured by the JSON — no device-specific C++ class needed.

| Config type string | C++ class | Configuration |
|--------------------|-----------|---------------|
| `"ram"` | `RamDevice` | Start/end address, optional bank name |
| `"rom"` | `RomDevice` | Start/end address, file path |

### Registry initialization

At program startup, all device factories are registered:

```cpp
void RegisterBuiltinDevices (ComponentRegistry & registry)
{
    registry.Register ("ram",                   RamDevice::Create);
    registry.Register ("rom",                   RomDevice::Create);
    registry.Register ("apple2-keyboard",       AppleKeyboard::Create);
    registry.Register ("apple2e-keyboard",      AppleIIeKeyboard::Create);
    registry.Register ("apple2-speaker",        AppleSpeaker::Create);
    registry.Register ("apple2-softswitches",   AppleSoftSwitchBank::Create);
    registry.Register ("apple2e-softswitches",  AppleIIeSoftSwitchBank::Create);
    registry.Register ("language-card",         LanguageCard::Create);
    registry.Register ("aux-ram-card",          AuxRamCard::Create);
    registry.Register ("disk-ii",              DiskIIController::Create);
}
```

### Adding a future machine (e.g., Commodore 64)

1. Write new device classes: `VicII`, `Sid`, `Cia`, `C64Keyboard`
2. Register them in `RegisterBuiltinDevices`
3. Create `machines/c64.json` with the C64 memory map, ROM paths, and device wiring
4. No changes to the emulator shell, MemoryBus, or existing machine configs

### ROM sourcing

ROM images are copyrighted and not distributed with the project. The `roms/` directory is gitignored. A helper script `scripts/FetchRoms.ps1` can download ROMs from the AppleWin project's repository (https://github.com/AppleWin/AppleWin) for convenience, following the same pattern as `scripts/RunDormannTest.ps1`.

## Apple II Model Differences

The three supported Apple II models share the same fundamental architecture but differ in specific capabilities. The data-driven config system handles these differences through different JSON files that wire different components.

### Apple II (original) vs. Apple II+

These two machines are architecturally identical — same 6502 CPU, same 48KB RAM, same peripheral slot layout, same video modes, same keyboard (uppercase only). The **only** difference is the system ROM:

| Aspect | Apple II | Apple II+ |
|--------|----------|-----------|
| System ROM | Integer BASIC + Monitor | Applesoft BASIC + Monitor |
| ROM file | `apple2.rom` | `apple2plus.rom` |
| BASIC dialect | Integer BASIC (faster, integer-only) | Applesoft BASIC (floating-point, standard) |
| Config name | `apple2` | `apple2plus` |

Both use the `"cpu": "6502"` setting, the `"apple2-keyboard"` (uppercase-only) device, and the same video modes. The config files are identical except for the `"name"` and the ROM `"file"` path.

### Apple II+ vs. Apple IIe

The Apple IIe is a significant hardware upgrade. These differences require distinct device components (not just config changes):

| Aspect | Apple II / II+ | Apple IIe |
|--------|----------------|-----------|
| CPU | NMOS 6502 | CMOS 65C02 (additional opcodes) |
| Main RAM | 48KB ($0000–$BFFF) | 64KB ($0000–$BFFF + $C100–$FFFF bank area) |
| Auxiliary RAM | None | 64KB (128KB total) via aux RAM card |
| Text mode | 40 columns, uppercase only | 40 or 80 columns, full upper/lowercase |
| Hi-res graphics | 280×192, 6 colors | 280×192 plus double hi-res 560×192, 16 colors |
| Keyboard | Uppercase ASCII only | Full upper/lowercase, auto-repeat, modifier keys |
| Special keys | None | Open Apple, Closed Apple (active-low at $C061/$C062) |
| Soft switches | Basic set ($C050–$C05F) | Extended set (adds 80-col, aux bank select, etc.) |
| Character ROM | Not separate (built into video circuit) | Separate character generator ROM (`apple2e-char.rom`) for MouseText and lowercase |
| ROM | Single 12KB ROM ($D000–$FFFF) | Larger ROM with $C100–$CFFF built-in, plus $D000–$FFFF |

### Example: Apple II (original) Config

```json
{
    "name": "Apple II",
    "cpu": "6502",
    "clockSpeed": 1023000,
    "memory": [
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF" },
        { "type": "rom",  "file": "apple2.rom", "start": "0xD000", "end": "0xFFFF" }
    ],
    "devices": [
        { "type": "apple2-keyboard",      "address": "0xC000" },
        { "type": "apple2-speaker",       "address": "0xC030" },
        { "type": "apple2-softswitches",  "start": "0xC050", "end": "0xC05F" },
        { "type": "language-card",        "start": "0xC080", "end": "0xC08F" },
        { "type": "disk-ii",             "slot": 6 }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-lores", "apple2-hires"],
        "width": 560,
        "height": 384
    },
    "keyboard": {
        "type": "apple2-uppercase"
    }
}
```

## CPU Integration with Casso65Core

The existing `Cpu` class in Casso65Core uses a flat 64KB `memory[]` vector and non-virtual `ReadByte`/`WriteByte` methods. To support the MemoryBus architecture without breaking existing functionality:

1. **Make memory access methods virtual**: Change `ReadByte`, `WriteByte`, `ReadWord`, `WriteWord` from non-virtual to virtual in `Cpu.h`. These are `protected` methods — this is not a public API change. All existing code (assembler CLI, unit tests) continues to use the base `Cpu` class with its flat memory array, completely unaffected.

2. **Create `EmuCpu` subclass**: A new class in the Casso65Emu project that overrides the four memory access methods to delegate to the `MemoryBus`. The `EmuCpu` constructor takes a `MemoryBus&` reference.

3. **65C02 support**: `EmuCpu` extends the instruction set when the config specifies `"cpu": "65c02"`. The additional opcodes are registered in the existing `instructionSet` table during initialization. This adds approximately 27 new opcodes and several new addressing modes (zero-page indirect, etc.) The base 6502 opcode table is unchanged.

4. **Cycle counting**: The `EmuCpu` tracks cycles executed per `StepOne()` call so the emulation loop can synchronize to real-time speed. The base `Cpu` class does not currently expose cycle counts; `EmuCpu` adds this internally.

## Emulation Loop Architecture

The emulation runs on a single thread, integrated with the Win32 message pump. There is no separate CPU thread — this avoids synchronization complexity and is sufficient for the 1 MHz Apple II's performance requirements on modern hardware.

### Frame-Based Execution

```
┌─────────────────────────────────────────────────┐
│                  Main Loop                       │
│                                                  │
│  1. Process Win32 messages (PeekMessage)         │
│  2. Execute ~17,050 CPU cycles (one frame)       │
│     - CPU steps, MemoryBus routes reads/writes   │
│     - Speaker toggles accumulate timestamps      │
│     - Soft switch changes update video mode       │
│  3. Render video framebuffer from current mode    │
│  4. Upload framebuffer to D3D11 texture, draw textured quad, Present swap chain      │
│  5. Submit audio buffer (waveOut)                 │
│  6. Sleep for remaining frame time (~16.6 ms)    │
│                                                  │
│  Target: ~60 frames/second (matching Apple II    │
│  NTSC vertical refresh rate)                     │
└─────────────────────────────────────────────────┘
```

### Timing

- Apple II clock: 1,023,000 Hz (14.31818 MHz master ÷ 14)
- NTSC frame rate: ~60.05 Hz (actually 262 scanlines × 65 cycles = 17,030 cycles/frame)
- Cycles per frame: ~17,030 (the exact value depends on NTSC timing; 17,050 is a reasonable approximation)
- Frame period: ~16.6 ms
- The loop uses `QueryPerformanceCounter` for high-resolution timing and sleeps (`Sleep` or spin-waits) to maintain real-time speed

### Speed Synchronization

When the host PC is faster than the emulated clock speed (which it always will be), the emulation loop inserts idle time after each frame. If the host is too slow (should not happen for a 1 MHz target), frames are dropped (video rendering is skipped but CPU execution continues).

## GUI Application Structure

### Project Layout

Casso65Emu is a new Win32 application project added to the `Casso65.sln` solution. It links against the Casso65Core static library.

```
Casso65.sln
├── Casso65Core/       Static library — CPU, assembler (existing, unchanged)
├── Casso65/           Console application — assembler CLI (existing, unchanged)
├── UnitTest/          Test DLL (existing, unchanged)
└── Casso65Emu/        NEW — Win32 GUI application
    ├── Casso65Emu.vcxproj
    ├── machines/      JSON machine config files
    │   ├── apple2.json
    │   ├── apple2plus.json
    │   └── apple2e.json
    ├── roms/          ROM images (gitignored)
    └── (source files)
```

### Window Specification

- **Window type**: Win32 `HWND` created with `CreateWindowEx`, `WS_OVERLAPPEDWINDOW` style but with resize disabled (`WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`)
- **Client area**: Fixed 560×384 pixels (2× the Apple II 280×192 hi-res resolution)
- **Title bar**: `"Casso65 — {machine name} [{state}]"` where state is Running, Paused, or Stopped (e.g., `"Casso65 — Apple II+ [Running]"`)
- **Menu bar**:
  - **File**: Open Disk 1… | Open Disk 2… | Eject Disk 1 | Eject Disk 2 | — | Exit
  - **Machine**: Reset | — | Pause / Resume
  - **Help**: About
- **Keyboard shortcuts**:
  - `Ctrl+1` / `Ctrl+2`: Open Disk 1 / Disk 2 dialog
  - `Ctrl+R`: Reset (simulates Apple II power-on reset)
  - `Pause` key: Toggle pause/resume
  - `Scroll Lock`: Toggle full-speed mode (run as fast as possible, no frame sync)
  - `F12` or `Alt+F4`: Exit
- **Disk insertion**: Uses standard Win32 `GetOpenFileName` dialog filtered to `.dsk` files
- **No status bar** in the initial implementation (future enhancement)

### Rendering Pipeline

1. A 560×384 RGBA pixel buffer is allocated in system memory
2. The active `VideoOutput` renderer reads Apple II video RAM and writes pixels into this buffer
3. Each frame, the buffer is copied to a Win32 DIB section
4. `BitBlt` (or `StretchDIBits`) transfers the DIB to the window's device context
5. Direct3D 11 is used for display (Windows SDK built-in). No third-party graphics libraries — (no OpenGL, Vulkan, SDL, etc.)

## Slot-Based Device Mapping

Apple II expansion slots (1–7) each have two address ranges on the bus:

| Range | Purpose | Example (Slot 6) |
|-------|---------|-------------------|
| `$C0s0–$C0sF` (where s = slot + 8) | Device I/O registers | `$C0E0–$C0EF` — Disk II stepper motor phases, read/write data, write mode, drive select |
| `$Cs00–$CsFF` | Slot ROM (256 bytes) | `$C600–$C6FF` — Disk II boot ROM (executed by `PR#6` or `C600G`) |

When a machine config specifies `"slot": 6` for a device, the MemoryBus automatically maps the device to both ranges. The slot ROM is typically embedded in the device's data or loaded from a separate ROM file.

### Disk II Slot 6 Details

The Disk II controller in slot 6 uses:
- **I/O range $C0E0–$C0EF**: 16 soft switches controlling stepper motor phases (0–3), motor on/off, drive select (1/2), read/write mode, and data latch
- **Slot ROM $C600–$C6FF**: 256-byte boot ROM that reads track 0, sector 0 into $0800 and jumps to it
- **.dsk format**: 140KB (35 tracks × 16 sectors × 256 bytes), DOS-order sector interleaving. The controller translates logical sectors to physical nibble-encoded sectors using the standard 6-and-2 encoding scheme.
