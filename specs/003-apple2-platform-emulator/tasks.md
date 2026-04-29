# Tasks: Apple II Platform Emulator

**Input**: Design documents from `/specs/003-apple2-platform-emulator/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Thorough unit test coverage required for all production code (constitution: Testing Discipline). Test tasks are integrated into each phase below and collected in Phase 11.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

**Note**: 65C02 support is out of scope (FR-021). All three target machines use NMOS 6502. Research.md R-005 (65C02 extensions) is intentionally ignored.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the Casso65Emu Win32 project, configure build system, establish folder structure

- [ ] T001 Create `Casso65Emu` Win32 application project (`Casso65Emu/Casso65Emu.vcxproj`) with WINDOWS subsystem, link against Casso65Core static library, configure x64 and ARM64 platforms, add d3d11.lib/dxgi.lib/ole32.lib link dependencies
- [ ] T002 Create precompiled header `Casso65Emu/Pch.h` and `Casso65Emu/Pch.cpp` including `<windows.h>`, `<d3d11.h>`, `<dxgi.h>`, `<mmdeviceapi.h>`, `<audioclient.h>`, `<string>`, `<vector>`, `<memory>`, `<unordered_map>`
- [ ] T003 Create project directory structure: `Casso65Emu/Core/`, `Casso65Emu/Devices/`, `Casso65Emu/Video/`, `Casso65Emu/Audio/`, `Casso65Emu/Shell/`, `Casso65Emu/Resources/`, `Casso65Emu/machines/`, `Casso65Emu/roms/`, `Casso65Emu/shaders/`
- [ ] T004 [P] Add `Casso65Emu/roms/` to `.gitignore` to exclude user-supplied ROM images from source control
- [ ] T005 [P] Update `Casso65.sln` to include the new `Casso65Emu` project with correct project dependencies (depends on Casso65Core)
- [ ] T006 Make `ReadByte`, `WriteByte`, `ReadWord`, `WriteWord` virtual in `Casso65Core/Cpu.h` (lines 85–88, protected interface only — no public API change)
- [ ] T007 Verify all 577+ existing unit tests still pass after the virtual keyword change in T006

**Checkpoint**: Casso65Emu project compiles (empty WinMain), links against Casso65Core, all existing tests pass

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core architecture that ALL user stories depend on — MemoryBus, MemoryDevice interface, ComponentRegistry, EmuCpu, JsonParser, MachineConfig loader, and D3D11/Win32 shell

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Core Architecture

- [ ] T008 [P] Implement `MemoryDevice` interface in `Casso65Emu/Core/MemoryDevice.h` — abstract base with `Read(Word)`, `Write(Word, Byte)`, `GetStart()`, `GetEnd()`, `Reset()` pure virtual methods
- [ ] T009 [P] Implement `ComponentRegistry` in `Casso65Emu/Core/ComponentRegistry.h/.cpp` — `Register(string, FactoryFunc)` and `Create(string, DeviceConfig, MemoryBus&)` with error reporting for unknown types
- [ ] T010 [P] Implement hand-written recursive descent JSON parser in `Casso65Emu/Core/JsonParser.h/.cpp` — lexer producing tokens, parser producing `JsonValue` variant type (object, array, string, number, bool, null), line/column error reporting per research.md R-006
- [ ] T011 Implement `MemoryBus` in `Casso65Emu/Core/MemoryBus.h/.cpp` — `ReadByte(Word)`, `WriteByte(Word, Byte)` with linear scan of `BusEntry` vector sorted by start address, floating bus value for unmapped I/O in $C000–$CFFF, `Validate()` for overlap detection
- [ ] T012 Implement `EmuCpu` in `Casso65Emu/Core/EmuCpu.h/.cpp` — subclass of Casso65Core `Cpu`, override `ReadByte`/`WriteByte`/`ReadWord`/`WriteWord` to route through `MemoryBus&`, add `cyclesThisStep` and `totalCycles` cycle tracking fields. No 65C02 extensions (FR-021).
- [ ] T013 Implement `MachineConfig` loader in `Casso65Emu/Core/MachineConfig.h/.cpp` — parse JSON via `JsonParser`, validate all fields per machine-config-schema.md contract (required fields, address format, address ordering, ROM file existence, component registry lookup, address overlap, slot range, device mapping), produce `MachineConfig` struct with `MemoryRegion`, `DeviceConfig`, `VideoConfig` sub-structs

### Data-Driven Device Creation

- [ ] T014 [P] Implement `RamDevice` in `Casso65Emu/Devices/RamDevice.h/.cpp` — `MemoryDevice` with configurable start/end, `Read`/`Write`/`Reset` (zero-fill), static `Create` factory
- [ ] T015 [P] Implement `RomDevice` in `Casso65Emu/Devices/RomDevice.h/.cpp` — `MemoryDevice` loaded from ROM file at startup, `Read` returns data, `Write` ignored, static `Create` factory with file validation
- [ ] T016 Register `RamDevice` and `RomDevice` factories in a `RegisterBuiltinDevices()` function in `Casso65Emu/Core/ComponentRegistry.cpp`

### Win32 Application Shell

- [ ] T017 [P] Create HLSL vertex shader in `Casso65Emu/shaders/VertexShader.hlsl` and pixel shader in `Casso65Emu/shaders/PixelShader.hlsl` for a full-window textured quad with `D3D11_FILTER_MIN_MAG_MIP_POINT` sampling. Add FXC custom build rules in `.vcxproj` to compile `.hlsl` → `.cso` at build time
- [ ] T018 [P] Create Win32 resource file `Casso65Emu/Resources/resource.h` and `Casso65Emu/Resources/Casso65Emu.rc` with menu IDs, keyboard accelerator table (Ctrl+R, Ctrl+Shift+R, Pause, F11, Alt+Enter, Ctrl+D, Ctrl+1, Ctrl+2, Ctrl+O, F1), icon, version info
- [ ] T019 Implement `D3DRenderer` in `Casso65Emu/Shell/D3DRenderer.h/.cpp` — `D3D11CreateDeviceAndSwapChain()`, create `D3D11_USAGE_DYNAMIC` texture (560×384 RGBA), static vertex/index buffers for quad, load `.cso` shaders, `UploadAndPresent(uint32_t* framebuffer)` via `Map`/`Unmap`, fullscreen toggle via borderless window per research.md R-001
- [ ] T020 Implement `MenuSystem` in `Casso65Emu/Shell/MenuSystem.h/.cpp` — create Win32 menu bar per FR-022 menu hierarchy (File, Machine, Disk, View, Help), radio-button check marks for Speed and Color Mode items, gray out CRT Shader, dispatch `WM_COMMAND` messages to callbacks
- [ ] T021 Implement `EmulatorShell` in `Casso65Emu/Shell/EmulatorShell.h/.cpp` — owns `MemoryBus`, `EmuCpu`, `D3DRenderer`, `MenuSystem`, video renderers vector, 560×384 RGBA framebuffer; `Initialize(MachineConfig)` wires all devices via ComponentRegistry; frame loop: `PeekMessage` → execute ~17,050 CPU cycles → `Render()` → `UploadAndPresent()` → sleep for remaining frame time via `QueryPerformanceCounter`
- [ ] T022 Implement `WinMain` in `Casso65Emu/Main.cpp` — parse `--machine`, `--disk1`, `--disk2` CLI arguments per cli-contract.md, load `MachineConfig`, initialize `EmulatorShell`, run message pump with emulation loop, handle all error exit codes per contract
- [ ] T023 Implement command-line argument parsing and error reporting in `Casso65Emu/Main.cpp` — validate `--machine` is provided, resolve `machines/<name>.json` path, validate `--disk1`/`--disk2` file existence and .dsk size (143,360 bytes), display clear error messages per cli-contract.md error behavior table

**Checkpoint**: Foundation ready — `Casso65Emu.exe --machine apple2plus` loads config, creates MemoryBus with RAM/ROM devices, opens a D3D11 window with menu bar. User story implementation can now begin.

---

## Phase 3: User Story 1 — Boot Apple II+ to BASIC Prompt (Priority: P1) 🎯 MVP

**Goal**: Launch `Casso65Emu.exe --machine apple2plus`, boot ROM, display 40-column text with Applesoft BASIC `]` prompt, accept keyboard input

**Independent Test**: Run `Casso65Emu.exe --machine apple2plus` with valid ROM → `]` prompt appears, type `PRINT "HELLO"` → output appears. Delivers a working text-mode Apple II+ emulator.

### Implementation for User Story 1

- [ ] T024 [P] [US1] Implement `AppleKeyboard` in `Casso65Emu/Devices/AppleKeyboard.h/.cpp` — uppercase-only keyboard device mapped at $C000, host-to-Apple ASCII key mapping, strobe latch (bit 7 of $C000), strobe clear on any read of $C010, `WM_KEYDOWN`/`WM_CHAR` translation, static `Create` factory
- [ ] T025 [P] [US1] Implement `AppleSoftSwitchBank` in `Casso65Emu/Devices/AppleSoftSwitchBank.h/.cpp` — video mode toggles at $C050–$C057 (graphicsMode, mixedMode, page2, hiresMode flags), annunciator outputs $C058–$C05F, `Read`/`Write` toggle state per even/odd address convention, static `Create` factory
- [ ] T026 [P] [US1] Embed Apple II/II+ character generator glyphs as `const Byte[]` array (2KB, 96 characters) in `Casso65Emu/Video/CharacterRom.h` per FR-028
- [ ] T027 [US1] Implement `AppleTextMode` in `Casso65Emu/Video/AppleTextMode.h/.cpp` — `VideoOutput` implementation for 40×24 text rendering, interleaved row address calculation (`base + 128*(row%8) + 40*(row/8)`), character ROM lookup from `CharacterRom.h`, normal/inverse/flash character modes, cursor blink, `Render(videoRam, framebuffer, 560, 384)` writing 2×-scaled RGBA pixels, `GetActivePageAddress(page2)` returning $0400 or $0800
- [ ] T028 [US1] Implement `VideoOutput` interface in `Casso65Emu/Video/VideoOutput.h` — abstract base with `Render(const Byte*, uint32_t*, int, int)` and `GetActivePageAddress(bool)` pure virtual methods
- [ ] T029 [US1] Create `Casso65Emu/machines/apple2plus.json` machine config per spec.md example — name "Apple II+", cpu "6502", clockSpeed 1023000, RAM $0000–$BFFF, ROM $D000–$FFFF from `apple2plus.rom`, keyboard/speaker/softswitches/language-card/disk-ii devices, video modes text40/lores/hires, keyboard type apple2-uppercase
- [ ] T030 [US1] Register `AppleKeyboard`, `AppleSoftSwitchBank` factories and `AppleTextMode` video mode in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T031 [US1] Wire keyboard input from `WM_KEYDOWN`/`WM_CHAR` Win32 messages through `EmulatorShell` to `AppleKeyboard` device — translate PC key codes to Apple II ASCII, handle Ctrl+Reset (warm reset via reset vector per FR-032), integrate into frame loop
- [ ] T032 [US1] Implement emulation timing in `EmulatorShell` — execute ~17,050 CPU cycles per frame via `EmuCpu::StepOne()` loop, select active `VideoOutput` based on soft switch state, call `Render()` to fill framebuffer, `UploadAndPresent()`, `QueryPerformanceCounter`-based frame timing at ~60 fps, speed synchronization with idle time insertion
- [ ] T033 [US1] Implement window title bar updates in `EmulatorShell` — display `"Casso65 — Apple II+ [Running]"` per FR-022, update state to Paused/Stopped when applicable
- [ ] T034 [US1] Implement Pause/Resume (Pause key) and Step (F11 when paused) in `EmulatorShell` per FR-032 — pause halts CPU execution but keeps window responsive, step executes one `StepOne()` call

**Checkpoint**: User Story 1 complete — Apple II+ boots to `]` prompt, keyboard works, text displays correctly. This is the MVP.

---

## Phase 4: User Story 2 — Speaker Audio Output (Priority: P2)

**Goal**: Speaker toggle at $C030 produces audible square-wave audio through PC speakers via WASAPI

**Independent Test**: At BASIC prompt, run `FOR I=1 TO 100: S=PEEK(49200): NEXT` → audible tone. Change loop count → pitch changes.

### Implementation for User Story 2

- [ ] T035 [P] [US2] Implement `AppleSpeaker` in `Casso65Emu/Devices/AppleSpeaker.h/.cpp` — `MemoryDevice` at $C030, `Read` toggles `speakerState` between +0.3f and -0.3f and records CPU cycle timestamp in `toggleTimestamps` vector, `Reset` clears state, static `Create` factory
- [ ] T036 [P] [US2] Implement `WasapiAudio` in `Casso65Emu/Audio/WasapiAudio.h/.cpp` — WASAPI shared-mode pull-mode initialization per research.md R-002 (float32 mono 44.1kHz, ~33ms buffer), `SubmitFrame(vector<uint32_t> toggleTimestamps, uint32_t totalCycles)` converts toggle timestamps to PCM samples, `GetCurrentPadding`/`GetBuffer`/`ReleaseBuffer` per-frame pattern, graceful fallback if `AUDCLNT_E_UNSUPPORTED_FORMAT` (retry with `GetMixFormat`), non-fatal failure per cli-contract.md
- [ ] T037 [US2] Register `AppleSpeaker` factory in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T038 [US2] Integrate `WasapiAudio` into `EmulatorShell` frame loop — initialize WASAPI at startup, call `SubmitFrame()` with speaker toggle timestamps after CPU execution each frame, clear timestamps for next frame

**Checkpoint**: User Story 2 complete — Speaker produces correct audio from BASIC programs and ROM beep.

---

## Phase 5: User Story 3 — Lo-Res Graphics Display (Priority: P3)

**Goal**: `GR` command switches to 40×48 16-color lo-res graphics mode, `COLOR=`/`PLOT`/`HLIN`/`VLIN` render correctly

**Independent Test**: At BASIC prompt, type `GR` → screen clears to lo-res mode with 4-line text window. Run `COLOR=1: PLOT 0,0` → magenta block at top-left. All 16 colors visually distinct.

### Implementation for User Story 3

- [ ] T039 [US3] Implement `AppleLoResMode` in `Casso65Emu/Video/AppleLoResMode.h/.cpp` — `VideoOutput` implementation for 40×48 16-color lo-res graphics, read from text page memory ($0400/$0800), decode each byte as two 4-bit color nybbles (low nybble = top block, high nybble = bottom block), use lo-res 16-color RGBA palette from spec, render each block as a scaled rectangle in 560×384 framebuffer, support mixed mode (4 text rows at bottom when `mixedMode` flag set)
- [ ] T040 [US3] Register `AppleLoResMode` video mode in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T041 [US3] Update video mode selection logic in `EmulatorShell` to switch between `AppleTextMode` and `AppleLoResMode` based on soft switch state — `graphicsMode && !hiresMode` selects lo-res, `!graphicsMode` selects text, handle mixed mode flag for 4-line text overlay

**Checkpoint**: User Story 3 complete — Lo-res graphics display works with all 16 colors, mixed mode, and page switching.

---

## Phase 6: User Story 4 — Language Card and DOS/ProDOS Loading (Priority: P4)

**Goal**: Boot DOS 3.3 from `--disk1`, Language Card bank-switching enables OS loading, `CATALOG` lists files

**Independent Test**: Run `Casso65Emu.exe --machine apple2plus --disk1 dos33.dsk` → DOS 3.3 boots, greeting displays, `CATALOG` lists files. Write-protected disk rejects writes.

### Implementation for User Story 4

- [ ] T042 [P] [US4] Implement `LanguageCard` in `Casso65Emu/Devices/LanguageCard.h/.cpp` — `MemoryDevice` at $C080–$C08F, 4KB `ramBank2` ($D000–$DFFF), 8KB `ramMain` ($E000–$FFFF), state machine for `readRam`/`writeRam`/`preWrite`/`bank2Select` flags driven by bits 0-3 of switch address, write-enable latch requiring two consecutive reads per data-model.md, `Read` delegates to RAM or ROM based on `readRam`, `Write` delegates to RAM if `writeRam` enabled, `Reset` clears RAM and resets state, static `Create` factory
- [ ] T043 [P] [US4] Implement `DiskImage` in `Casso65Emu/Devices/DiskIIController.h` (or separate header) — load 143,360-byte .dsk file into memory, `ReadSector(track, logicalSector)` with DOS 3.3 interleave table (logical 0→physical 0, 1→7, 2→14, 3→5...), `WriteSector(track, logicalSector, data)` with dirty tracking, `IsWriteProtected()`, `Flush()` for buffer-and-flush mode, copy-on-write delta sidecar file support per FR-027
- [ ] T044 [P] [US4] Implement 6-and-2 nibble encoding/decoding in `Casso65Emu/Devices/DiskIIController.cpp` — 256 source bytes → 342 six-bit values (86 secondary + 256 primary) → XOR encoding → 343 nibbles + checksum, write translate table (64-entry, values ≥$96), read translate table (inverse 256-entry), track nibblization producing ~6,400 bytes per track with gap/address/data field structure per research.md R-003
- [ ] T045 [US4] Implement `DiskIIController` in `Casso65Emu/Devices/DiskIIController.h/.cpp` — `MemoryDevice` for slot-based device (slot 6: I/O $C0E0–$C0EF, ROM $C600–$C6FF), stepper motor state machine (4 phases, quarter-track position 0–139), Q6/Q7 data path state machine (read/sense/write/load modes), motor on/off, drive select (1/2), shift register for nibble read/write, pre-nibblized track buffer (~6,400 bytes), static `Create` factory per research.md R-003
- [ ] T046 [US4] Implement slot-based device auto-mapping in `MemoryBus` or `MachineConfig` — when config specifies `"slot": N`, map device to I/O range `$C080+N×16` through `$C08F+N×16` AND slot ROM `$CN00–$CNFF` per FR-025
- [ ] T047 [US4] Register `LanguageCard` and `DiskIIController` factories in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T048 [US4] Integrate disk image mounting into `EmulatorShell` — handle `--disk1`/`--disk2` CLI arguments to pre-mount .dsk images at boot, implement File Open dialog for runtime disk insertion (Ctrl+1, Ctrl+2), eject (Ctrl+Shift+1, Ctrl+Shift+2), write-protect toggle via Disk menu
- [ ] T049 [US4] Implement disk write mode selection in `MenuSystem` and `DiskIIController` — buffer-and-flush (default: changes in memory, written to .dsk on eject/exit) and copy-on-write (original .dsk never modified, changes to .delta sidecar) per FR-027, flush on clean exit and eject

**Checkpoint**: User Story 4 complete — DOS 3.3 boots from disk, `CATALOG`/`LOAD`/`RUN` work, write modes functional, Language Card bank-switching operational.

---

## Phase 7: User Story 5 — Hi-Res Graphics with Color (Priority: P5)

**Goal**: `HGR` command switches to 280×192 hi-res graphics with NTSC color artifact simulation

**Independent Test**: At BASIC prompt, type `HGR` → hi-res mode with 4-line text window. `HPLOT` commands produce colored pixels. Alternating patterns produce NTSC artifact colors (violet/green/blue/orange).

### Implementation for User Story 5

- [ ] T050 [P] [US5] Pre-compute `NtscColorTable` in `Casso65Emu/Video/NtscColorTable.h` — 512-entry lookup table indexed by `(palette_bit << 8) | byte_value`, each entry contains 14 RGBA pixels (7 logical × 2× horizontal doubling), using standard Apple II colors (Black, White, Violet, Green, Blue, Orange) per research.md R-004
- [ ] T051 [US5] Implement `AppleHiResMode` in `Casso65Emu/Video/AppleHiResMode.h/.cpp` — `VideoOutput` implementation for 280×192 hi-res graphics from $2000–$3FFF (page 1) or $4000–$5FFF (page 2), interleaved row addressing (same formula as text but with 8 pixel-rows per text row = 192 scanlines), 7 pixels per byte (bits 0–6, LSB=leftmost) with bit 7 palette select, `NtscColorTable` lookup per byte, cross-byte adjacency white merging, mixed mode support (4 text rows at bottom), `Render` and `GetActivePageAddress`
- [ ] T052 [US5] Register `AppleHiResMode` video mode in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T053 [US5] Update video mode selection logic in `EmulatorShell` — `graphicsMode && hiresMode` selects hi-res, integrate with existing text/lo-res mode switching based on soft switch state

**Checkpoint**: User Story 5 complete — Hi-res graphics render with NTSC color artifacts, page switching, mixed mode.

---

## Phase 8: User Story 6 — Apple IIe Extended Features (Priority: P6)

**Goal**: `--machine apple2e` boots with 128KB RAM, 80-column text, double hi-res, lowercase keyboard, Open/Closed Apple keys

**Independent Test**: Run `Casso65Emu.exe --machine apple2e` → boots with lowercase support. `PR#3` activates 80-column mode. Double hi-res programs render at 560×192 with 16 colors.

### Implementation for User Story 6

- [ ] T054 [P] [US6] Implement `AppleIIeKeyboard` in `Casso65Emu/Devices/AppleIIeKeyboard.h/.cpp` — extends keyboard with lowercase input, modifier key support (Shift, Ctrl), Open Apple ($C061) and Closed Apple ($C062) button state as active-low reads, auto-repeat, static `Create` factory
- [ ] T055 [P] [US6] Implement `AuxRamCard` in `Casso65Emu/Devices/AuxRamCard.h/.cpp` — `MemoryDevice` for IIe auxiliary 64KB RAM bank, soft switches at $C003/$C004 (read main/aux RAM for $0200–$BFFF) and $C005/$C006 (write main/aux RAM), redirect reads/writes to auxiliary memory bank when active, static `Create` factory
- [ ] T056 [P] [US6] Implement `AppleIIeSoftSwitchBank` in `Casso65Emu/Devices/AppleIIeSoftSwitchBank.h/.cpp` — extends `AppleSoftSwitchBank` with IIe-specific switches: 80-column mode ($C00C/$C00D), character set select ($C00E/$C00F), double hi-res ($C05E/$C05F), page 1/2 also selects main/aux for $0400/$2000 regions, IOU disable/enable ($C07E/$C07F), static `Create` factory
- [ ] T057 [P] [US6] Implement `Apple80ColTextMode` in `Casso65Emu/Video/Apple80ColTextMode.h/.cpp` — `VideoOutput` for 80×24 text, interleaved main/aux memory reading (even columns from aux, odd from main or vice versa), Apple IIe character ROM lookup (loaded from `apple2e-char.rom` file per config, includes MouseText and lowercase), `Render` to 560×384 framebuffer
- [ ] T058 [P] [US6] Implement `AppleDoubleHiResMode` in `Casso65Emu/Video/AppleDoubleHiResMode.h/.cpp` — `VideoOutput` for 560×192 double hi-res from interleaved main/aux hi-res memory, 16-color palette, 7 bits per byte from alternating aux/main bytes across each scanline, `Render` and `GetActivePageAddress`
- [ ] T059 [US6] Create `Casso65Emu/machines/apple2e.json` machine config per spec.md example — name "Apple IIe", cpu "6502", RAM $0000–$BFFF main + aux bank, ROM from apple2e.rom ($C100–$FFFF) + apple2e-char.rom (chargen target), devices: apple2e-keyboard, speaker, apple2e-softswitches, aux-ram-card, language-card, disk-ii slot 6, video modes: text40/text80/lores/hires/doublehires, keyboard type apple2e-full
- [ ] T060 [US6] Register `AppleIIeKeyboard`, `AuxRamCard`, `AppleIIeSoftSwitchBank`, `Apple80ColTextMode`, `AppleDoubleHiResMode` factories in `RegisterBuiltinDevices()` in `Casso65Emu/Core/ComponentRegistry.cpp`
- [ ] T061 [US6] Update `EmulatorShell` video mode selection to handle IIe-specific modes — 80-column text when `80colMode` soft switch active, double hi-res when `doubleHiRes` and `hiresMode` both active, ensure `AuxRamCard` bank switching interacts correctly with video renderers reading from aux memory

**Checkpoint**: User Story 6 complete — Apple IIe boots with all extended features: 80-column, double hi-res, lowercase, Open/Closed Apple keys.

---

## Phase 9: User Story 7 — Data-Driven Machine Configuration (Priority: P7)

**Goal**: All three machine configs work interchangeably, unknown device types produce clear errors, architecture is validated as extensible

**Independent Test**: Create a minimal custom machine config JSON with just RAM + ROM + text display. Launch with `--machine custom` → emulator loads and runs. Launch with an unknown device type → clear error message.

### Implementation for User Story 7

- [ ] T062 [US7] Create `Casso65Emu/machines/apple2.json` machine config for original Apple II — identical to apple2plus.json except name "Apple II" and ROM file `apple2.rom` (Integer BASIC) per FR-026 and spec.md Apple II config example
- [ ] T063 [US7] Implement machine config discovery in `Main.cpp` — scan `machines/` directory for `.json` files, list available machine names in error messages when `--machine` argument is invalid or missing per cli-contract.md
- [ ] T064 [US7] Validate complete config error reporting path — test all error conditions from cli-contract.md: missing `--machine`, unknown machine name, JSON parse error with line/column, missing ROM file with expected path, invalid .dsk file size, overlapping device addresses naming both devices, unknown device type listing registered types
- [ ] T065 [US7] Validate component registry extensibility — ensure a new device type can be registered and referenced from a config file without modifying EmulatorShell, MemoryBus, or existing machine configs per SC-008

**Checkpoint**: User Story 7 complete — All three machines boot correctly, error reporting is comprehensive, architecture is proven extensible.

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories — diagnostics, display modes, reset, speed control

- [ ] T066 [P] Implement `DebugConsole` in `Casso65Emu/Shell/DebugConsole.h/.cpp` — in-app window opened via Ctrl+D (Help menu), displays `DEBUGMSG`/`OutputDebugString` output, machine config summary, device wiring status, unhandled soft switch accesses per FR-029
- [ ] T067 [P] Implement four display color modes in `EmulatorShell` per FR-030 — Color (default, NTSC artifacts), Green Monochrome, Amber Monochrome, White Monochrome. Monochrome modes convert RGBA framebuffer to luminance (`L = 0.299R + 0.587G + 0.114B`) then tint: green `(0,L,0)`, amber `(L,L×0.75,0)`, white `(L,L,L)`. Radio-button selection in View menu.
- [ ] T068 [P] Implement speed control in `EmulatorShell` — Speed 1× (authentic ~1.023 MHz), Speed 2× (double cycles per frame), Speed Maximum (no frame timing sleep). Radio-button selection in Machine menu per FR-023.
- [ ] T069 Implement warm reset (Ctrl+R) and cold boot/power cycle (Ctrl+Shift+R) in `EmulatorShell` per FR-032 — warm reset fetches reset vector preserving RAM; cold boot clears all RAM, calls `Reset()` on all devices, restarts CPU from reset vector
- [ ] T070 Implement fullscreen toggle (Alt+Enter) in `EmulatorShell` and `D3DRenderer` per FR-031 — borderless fullscreen window via `SetWindowLong(GWL_STYLE, WS_POPUP)` + `SetWindowPos()` per research.md R-001, maintain aspect ratio with letterboxing, toggle back to windowed mode
- [ ] T071 Implement Machine Info dialog in `MenuSystem` — display machine name, CPU type, clock speed, memory map summary, device list, ROM file paths
- [ ] T072 Implement Keyboard Map dialog accessible via F1 (Help menu) — show PC-to-Apple II key mappings
- [ ] T073 [P] Create `scripts/FetchRoms.ps1` helper script to download ROM images from AppleWin project repository per quickstart.md
- [ ] T074 Verify all 577+ existing unit tests pass with the complete Casso65Emu project integrated into the solution per FR-019
- [ ] T075 Run quickstart.md validation — verify all build commands, run commands, and test scenarios documented in quickstart.md work correctly end-to-end

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **User Stories (Phase 3–9)**: All depend on Foundational phase completion
  - User stories can proceed sequentially in priority order (P1 → P2 → ... → P7)
  - Some stories can overlap (see parallel opportunities below)
- **Polish (Phase 10)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (P1)**: Can start after Phase 2 — no dependencies on other stories. **MVP delivery point.**
- **US2 (P2)**: Can start after Phase 2 — independent of US1 (speaker is a separate device)
- **US3 (P3)**: Can start after Phase 2 — independent (lo-res uses existing soft switch framework)
- **US4 (P4)**: Can start after Phase 2 — independent but best after US1 (needs working CPU loop for boot testing)
- **US5 (P5)**: Can start after Phase 2 — independent (hi-res is a separate video mode renderer)
- **US6 (P6)**: Depends on US1 (text mode), US3 (lo-res pattern), US5 (hi-res pattern) — extends all video modes for IIe
- **US7 (P7)**: Depends on US1 + US6 — validates all three machine configs work

### Within Each User Story

- Models/devices before services/integration
- Video renderers before shell integration
- Device registration before config usage
- Core implementation before menu/UI integration

### Parallel Opportunities

- **Phase 2**: T008, T009, T010 can run in parallel (independent files). T014, T015 in parallel. T017, T018 in parallel.
- **US1**: T024, T025, T026 can run in parallel (independent device files)
- **US2**: T035, T036 can run in parallel (speaker device + WASAPI audio)
- **US4**: T042, T043, T044 can run in parallel (Language Card + DiskImage + nibble encoding)
- **US5**: T050 can run in parallel with other story work (static lookup table)
- **US6**: T054, T055, T056, T057, T058 can all run in parallel (independent IIe device/video files)
- **US1+US2+US3**: These three stories have no cross-dependencies and can be worked in parallel after Phase 2

---

## Parallel Example: User Story 1

```
# Launch all independent device implementations together:
Task: "Implement AppleKeyboard in Casso65Emu/Devices/AppleKeyboard.h/.cpp"
Task: "Implement AppleSoftSwitchBank in Casso65Emu/Devices/AppleSoftSwitchBank.h/.cpp"
Task: "Embed character generator glyphs in Casso65Emu/Video/CharacterRom.h"

# Then sequentially:
Task: "Implement AppleTextMode in Casso65Emu/Video/AppleTextMode.h/.cpp" (needs CharacterRom)
Task: "Register factories and wire into EmulatorShell"
Task: "Integrate keyboard input and frame timing"
```

## Parallel Example: User Story 6

```
# Launch all 5 IIe device/video implementations together:
Task: "Implement AppleIIeKeyboard in Casso65Emu/Devices/AppleIIeKeyboard.h/.cpp"
Task: "Implement AuxRamCard in Casso65Emu/Devices/AuxRamCard.h/.cpp"
Task: "Implement AppleIIeSoftSwitchBank in Casso65Emu/Devices/AppleIIeSoftSwitchBank.h/.cpp"
Task: "Implement Apple80ColTextMode in Casso65Emu/Video/Apple80ColTextMode.h/.cpp"
Task: "Implement AppleDoubleHiResMode in Casso65Emu/Video/AppleDoubleHiResMode.h/.cpp"

# Then sequentially:
Task: "Create apple2e.json machine config"
Task: "Register all IIe factories"
Task: "Update EmulatorShell for IIe video mode selection"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T007)
2. Complete Phase 2: Foundational (T008–T023)
3. Complete Phase 3: User Story 1 (T024–T034)
4. **STOP and VALIDATE**: Boot Apple II+ to `]` prompt, type BASIC commands
5. This delivers a working text-mode Apple II+ emulator

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. Add US1 → Boot to BASIC prompt (MVP!)
3. Add US2 → Audio output
4. Add US3 → Lo-res graphics
5. Add US4 → Disk boot, DOS/ProDOS
6. Add US5 → Hi-res graphics with color
7. Add US6 → Apple IIe extended features
8. Add US7 → All three machines validated
9. Polish → Diagnostics, color modes, speed control, fullscreen
10. Each story adds value without breaking previous stories

### Parallel Team Strategy

With multiple developers after Phase 2:
- Developer A: US1 (text mode + keyboard + frame loop)
- Developer B: US2 + US3 (speaker audio + lo-res graphics)
- Developer C: US4 (Language Card + Disk II controller)
- Once US1/US3/US5 done → Developer D: US6 (IIe extensions)

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable (except US6 which extends prior modes)
- 65C02 instruction set extensions (research.md R-005) are explicitly excluded per FR-021
- All three target machines use NMOS 6502 — EmuCpu has no `is65C02` flag or opcode patching
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently

---

## Phase 11: Unit Tests

Thorough unit test coverage for all new production code. Tests go in `UnitTest/EmuTests/` and link against both Casso65Core and Casso65Emu code.

### MemoryBus Tests (`UnitTest/EmuTests/MemoryBusTests.cpp`)

- [ ] **T076** [P] Write MemoryBus tests: device registration, read/write dispatch to correct device by address, overlapping address detection at startup, unmapped address returns floating bus default ($FF), device at single address vs range, remove device. Cover FR-004, FR-033.

### JSON Parser Tests (`UnitTest/EmuTests/JsonParserTests.cpp`)

- [ ] **T077** [P] Write JSON parser tests: parse strings, numbers (int + hex), booleans, arrays, objects, nested structures, empty objects/arrays, escaped characters in strings, malformed JSON error reporting (missing braces, trailing commas, unterminated strings). Cover FR-002, FR-018.

### MachineConfig Tests (`UnitTest/EmuTests/MachineConfigTests.cpp`)

- [ ] **T078** [P] Write MachineConfig loading tests: valid Apple II+ config loads all devices and memory regions, missing ROM file produces clear error, unknown device type produces clear error, overlapping address ranges detected, missing required fields (name, cpu) produce errors, valid IIe config with aux RAM and multi-range soft switches. Cover FR-002, FR-018, FR-033.

### RAM/ROM Device Tests (`UnitTest/EmuTests/DeviceTests.cpp`)

- [ ] **T079** [P] Write RAM device tests: read/write roundtrip at all addresses in range, writes outside range are rejected, zero-initialization. Write ROM device tests: reads return loaded data, writes are ignored (no-op), load from file, load from embedded data. Cover FR-005.

### Keyboard Tests (`UnitTest/EmuTests/KeyboardTests.cpp`)

- [ ] **T080** [P] Write Apple II keyboard tests: key press sets $C000 with bit 7 high, read $C010 clears strobe (bit 7), no key pressed returns last key with bit 7 low, all printable ASCII characters map correctly. Write Apple IIe keyboard tests: lowercase support, modifier keys (Open/Closed Apple mapped to host keys), auto-repeat timing. Cover FR-011.

### Soft Switch Tests (`UnitTest/EmuTests/SoftSwitchTests.cpp`)

- [ ] **T081** [P] Write soft switch tests: read $C051 sets text mode flag, read $C050 sets graphics mode, read $C052/$C053 toggles mixed mode, read $C054/$C055 toggles page, read $C056/$C057 toggles hi-res. Verify all switches are read-triggered (write has no effect). For IIe: $C00C/$C00D toggle 80-col, $C05E/$C05F toggle double hi-res. Cover FR-006, FR-033.

### Text Mode Renderer Tests (`UnitTest/EmuTests/TextModeTests.cpp`)

- [ ] **T082** [P] Write text mode renderer tests: verify interleaved row address calculation matches the formula `base + 128*(row%8) + 40*(row/8)` for all 24 rows on both pages. Verify character-to-glyph lookup from embedded ROM data. Verify inverse/flash character rendering. Verify cursor blink toggle. For 80-col mode: verify interleaved main/aux memory reading. Cover FR-007, FR-028.

### Lo-Res Renderer Tests (`UnitTest/EmuTests/LoResModeTests.cpp`)

- [ ] **T083** [P] Write lo-res renderer tests: verify nybble decoding (low nybble = top block, high nybble = bottom block), verify all 16 colors produce correct RGBA values against the reference palette, verify row address calculation matches text mode. Cover FR-008.

### Hi-Res Renderer Tests (`UnitTest/EmuTests/HiResModeTests.cpp`)

- [ ] **T084** [P] Write hi-res renderer tests: verify 7-pixels-per-byte decoding (LSB = leftmost), verify palette bit (bit 7) switches color groups (violet/green vs blue/orange), verify adjacent ON pixels merge to white, verify row address calculation for all 192 lines. NTSC artifact coloring: verify known byte patterns produce expected colors against a reference table. Cover FR-009.

### Speaker Tests (`UnitTest/EmuTests/SpeakerTests.cpp`)

- [ ] **T085** [P] Write speaker tests: verify $C030 read toggles speaker state, verify toggle timestamps are accumulated during a frame, verify PCM waveform generation from toggle sequence produces correct square wave shape. Test edge cases: no toggles = silence, single toggle = DC offset, rapid toggles = high-frequency signal. Cover FR-012, FR-024.

### Language Card Tests (`UnitTest/EmuTests/LanguageCardTests.cpp`)

- [ ] **T086** [P] Write Language Card tests: verify initial state (ROM visible at $D000–$FFFF), verify single read of $C080 enables RAM read, verify double read of $C081 enables RAM write, verify all 16 soft switch combinations ($C080–$C08F) produce correct read/write/bank states. Test bank 1 vs bank 2 selection at $D000–$DFFF. Verify write-enable requires two consecutive reads. Cover FR-013.

### Disk II Controller Tests (`UnitTest/EmuTests/DiskIITests.cpp`)

- [ ] **T087** [P] Write Disk II tests: verify .dsk sector read produces correct nibblized data (6-and-2 encoding), verify track seek via stepper motor phases, verify sector interleaving (DOS-order: physical→logical sector mapping), verify read data latch timing. Test write mode enable sequence. Test write-protected disk returns write-protect status. Test buffer-and-flush: verify memory buffer matches .dsk file after flush. Test copy-on-write: verify original .dsk unchanged after writes. Cover FR-014, FR-027.

### Game I/O Tests (`UnitTest/EmuTests/GameIOTests.cpp`)

- [ ] **T088** [P] Write game I/O device tests: verify $C061 returns Open Apple button state (bit 7), verify $C062 returns Closed Apple button state (bit 7). Test button press and release transitions. Cover FR-034.

### EmuCpu Tests (`UnitTest/EmuTests/EmuCpuTests.cpp`)

- [ ] **T089** [P] Write EmuCpu integration tests: verify ReadByte/WriteByte route through MemoryBus (not flat array), verify RAM read/write roundtrip via bus, verify ROM read returns correct data and write is ignored, verify soft switch read triggers side effect, verify CPU reset fetches vector from ROM via bus. Cover FR-020.

### Component Registry Tests (`UnitTest/EmuTests/RegistryTests.cpp`)

- [ ] **T090** [P] Write component registry tests: register a device factory by name, create device by name lookup, unknown name returns null/error, duplicate registration handling, verify all built-in device types are registered at startup. Cover FR-003.

---

## Phase 12: Menu and UI Completeness

- [ ] **T091** [US7] Implement File > Open Machine Config dialog: Win32 `GetOpenFileName` filtered to `.json` files, tear down current machine (stop emulation, release devices, clear bus), load new config, reinitialize and start emulation. (`Casso65Emu/EmulatorShell.cpp`)
- [ ] **T092** [US7] Implement File > Recent Machines submenu: track last 5 machine configs in a simple `.ini` or registry key, populate submenu dynamically, select to load. (`Casso65Emu/EmulatorShell.cpp`)
- [ ] **T093** Implement Help > About Casso65 dialog: `DialogBox` with version string from `Version.h`, project URL, copyright. (`Casso65Emu/EmulatorShell.cpp`)
- [ ] **T094** Fix accelerator table (T018): add `Ctrl+Shift+1` (Drive 1 Eject), `Ctrl+Shift+2` (Drive 2 Eject) to keyboard accelerator table. (`Casso65Emu/main.cpp`)
