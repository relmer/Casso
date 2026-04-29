# Implementation Plan: Apple II Platform Emulator

**Branch**: `003-apple2-platform-emulator` | **Date**: 2025-07-22 | **Spec**: `specs/003-apple2-platform-emulator/spec.md`
**Input**: Feature specification from `specs/003-apple2-platform-emulator/spec.md`

## Summary

Add a GUI-based Apple II platform emulator (`Casso65Emu`) as a new Win32 application project to the Casso65 solution. The emulator links against the existing Casso65Core static library and provides data-driven machine configurations (JSON) for Apple II, Apple II+, and Apple IIe. It renders via Direct3D 11, generates audio through WASAPI, and uses a component registry architecture to map named device types to C++ classes. The emulator supports text mode, lo-res graphics, hi-res graphics with NTSC color artifacting, 80-column/double hi-res (IIe), Disk II controller emulation (.dsk format), Language Card bank-switching, and 65C02 instruction set extensions. No third-party libraries are used.

## Technical Context

**Language/Version**: C++ /std:c++latest (MSVC v145, Visual Studio 2026)
**Primary Dependencies**: Windows SDK (D3D11, DXGI, WASAPI, Win32), C++ STL — no third-party libraries
**Storage**: .dsk disk images (140KB files), JSON machine config files, ROM files (user-supplied, gitignored)
**Testing**: Microsoft C++ Unit Test Framework (CppUnitTestFramework) — existing UnitTest project
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Desktop application (Win32 GUI) + static library extension
**Performance Goals**: 60 fps video output, 1.023 MHz emulated CPU clock, ≤50ms audio latency, <50ms keyboard response
**Constraints**: Single-threaded emulation loop, no third-party libs, fixed 560×384 window, ≤50ms audio latency
**Scale/Scope**: 3 machine configurations (Apple II, II+, IIe), ~14 device component classes, ~40 source files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Code Quality — ✅ PASS

- **EHM macros**: All HRESULT-returning functions will use CHR/CBR/CWRA/BAIL_OUT_IF patterns. D3D11, WASAPI, and Win32 COM calls return HRESULT — perfect fit for EHM.
- **Single exit point**: All functions via `Error:` label. No early returns.
- **Formatting**: 5 blank lines between top-level constructs, 3 blank lines after declarations. Column alignment preserved.
- **Smart pointers**: `unique_ptr` for device ownership in MemoryBus. COM pointers managed via explicit Release in EHM cleanup.

### II. Testing Discipline — ✅ PASS

- **No system state in tests**: All device components (RAM, ROM, keyboard, speaker, soft switches) will have interfaces tested with synthetic data. MemoryBus tested with mock MemoryDevices. JSON parser tested with string input. Video renderers tested with synthetic framebuffers.
- **System API isolation**: D3D11, WASAPI, Win32 window creation are in the EmulatorShell (not testable via unit tests). All logic behind those APIs is factored into pure functions that accept data, not handles.
- **Existing tests preserved**: FR-019 requires all 577+ existing tests pass unchanged.

### III. User Experience Consistency — ✅ PASS

- **CLI syntax**: `Casso65Emu.exe --machine <name> --disk1 <path> --disk2 <path>` follows established `--flag` pattern.
- **Error messages**: Clear, actionable errors for missing ROM, unknown device, invalid JSON, overlapping addresses.

### IV. Performance Requirements — ✅ PASS

- **1.023 MHz emulation**: ~17,030 cycles per 60 Hz frame on modern hardware — trivially fast.
- **Single-threaded**: No synchronization overhead. CPU execution + video render + audio submission in one frame.
- **Minimal allocation**: Framebuffer allocated once (560×384×4 = 860KB). Device components allocated at startup only.

### V. Simplicity & Maintainability — ✅ PASS

- **YAGNI**: No CRT shader (grayed-out menu item), no .nib/.woz formats, no network play. Only what's specified.
- **Single responsibility**: Each device class handles one hardware component. MemoryBus only routes addresses. EmulatorShell only coordinates frame loop.
- **Function size**: All functions under 50 lines. Video renderers use helper functions for row address calculation, byte decoding, color lookup.
- **File scope**: New project (Casso65Emu) added; Casso65Core change is minimal (`virtual` keyword on 4 protected methods).

### Technology Constraints — ✅ PASS

- **stdcpplatest / MSVC v145**: Confirmed.
- **Windows SDK + STL only**: D3D11, DXGI, WASAPI, Win32 are all Windows SDK. JSON parser is hand-written (STL only).
- **Both x64 and ARM64**: D3D11 and WASAPI are supported on ARM64 Windows. Shaders pre-compiled to CPU-agnostic bytecode.

## Project Structure

### Documentation (this feature)

```text
specs/003-apple2-platform-emulator/
├── plan.md              # This file
├── research.md          # Phase 0 — technology research
├── data-model.md        # Phase 1 — entity model and state machines
├── quickstart.md        # Phase 1 — build and run guide
├── contracts/           # Phase 1 — CLI and config schema contracts
│   ├── cli-contract.md
│   └── machine-config-schema.md
└── tasks.md             # Phase 2 — task breakdown (created by /speckit.tasks)
```

### Source Code (repository root)

```text
Casso65.sln                          # Updated — adds Casso65Emu project
├── Casso65Core/                     # Existing static library (minimal change: virtual keyword)
│   ├── Cpu.h                        #   ReadByte/WriteByte/ReadWord/WriteWord → virtual
│   └── (all other files unchanged)
├── Casso65/                         # Existing console app (unchanged)
├── UnitTest/                        # Existing tests (unchanged, 577+ tests)
│   └── EmuTests/                    #   NEW — emulator-specific unit tests
│       ├── MemoryBusTests.cpp
│       ├── JsonParserTests.cpp
│       ├── MachineConfigTests.cpp
│       ├── DeviceTests.cpp
│       ├── TextModeTests.cpp
│       ├── LoResModeTests.cpp
│       ├── HiResModeTests.cpp
│       ├── SpeakerTests.cpp
│       ├── DiskIITests.cpp
│       ├── LanguageCardTests.cpp
│       ├── SoftSwitchTests.cpp
│       └── KeyboardTests.cpp
└── Casso65Emu/                      # NEW — Win32 GUI application
    ├── Casso65Emu.vcxproj
    ├── Pch.h / Pch.cpp              #   Precompiled header (includes <windows.h>, D3D11, etc.)
    ├── Main.cpp                     #   WinMain, message pump, emulation loop
    ├── machines/                    #   JSON machine config files
    │   ├── apple2.json
    │   ├── apple2plus.json
    │   └── apple2e.json
    ├── roms/                        #   ROM images (gitignored)
    ├── shaders/                     #   HLSL shaders (compiled at build time via FXC)
    │   ├── VertexShader.hlsl
    │   └── PixelShader.hlsl
    ├── Core/                        #   Core emulator architecture
    │   ├── MemoryBus.h/.cpp         #     Address-space router
    │   ├── MemoryDevice.h           #     Interface for bus-attached devices
    │   ├── ComponentRegistry.h/.cpp #     Factory registry (string → class)
    │   ├── MachineConfig.h/.cpp     #     JSON config loader and validator
    │   ├── JsonParser.h/.cpp        #     Hand-written recursive descent JSON parser
    │   └── EmuCpu.h/.cpp            #     Cpu subclass routing through MemoryBus
    ├── Devices/                     #   Hardware device components
    │   ├── RamDevice.h/.cpp
    │   ├── RomDevice.h/.cpp
    │   ├── AppleKeyboard.h/.cpp
    │   ├── AppleIIeKeyboard.h/.cpp
    │   ├── AppleSpeaker.h/.cpp
    │   ├── AppleSoftSwitchBank.h/.cpp
    │   ├── AppleIIeSoftSwitchBank.h/.cpp
    │   ├── LanguageCard.h/.cpp
    │   ├── AuxRamCard.h/.cpp
    │   └── DiskIIController.h/.cpp
    ├── Video/                       #   Video renderers
    │   ├── VideoOutput.h            #     Interface for video mode renderers
    │   ├── AppleTextMode.h/.cpp
    │   ├── Apple80ColTextMode.h/.cpp
    │   ├── AppleLoResMode.h/.cpp
    │   ├── AppleHiResMode.h/.cpp
    │   ├── AppleDoubleHiResMode.h/.cpp
    │   ├── CharacterRom.h           #     Embedded 2KB Apple II/II+ glyph data
    │   └── NtscColorTable.h         #     Pre-computed NTSC artifact color LUTs
    ├── Audio/                       #   Audio subsystem
    │   └── WasapiAudio.h/.cpp       #     WASAPI shared-mode audio stream
    ├── Shell/                       #   Application shell
    │   ├── EmulatorShell.h/.cpp     #     Main app class, frame loop, timing
    │   ├── D3DRenderer.h/.cpp       #     D3D11 device, swap chain, texture upload
    │   ├── MenuSystem.h/.cpp        #     Win32 menu creation and command dispatch
    │   └── DebugConsole.h/.cpp      #     In-app debug log window
    └── Resources/                   #   Win32 resources
        ├── resource.h
        └── Casso65Emu.rc            #     Menu accelerators, icon, version info
```

**Structure Decision**: New `Casso65Emu` Win32 application project added to the existing solution. Organized into Core (architecture), Devices (hardware components), Video (renderers), Audio (WASAPI), and Shell (Win32/D3D11 application). The Casso65Core static library receives one minimal change (4 methods become virtual). All new unit tests go into a new `EmuTests/` folder within the existing UnitTest project.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| 4th project (Casso65Emu) | GUI emulator is fundamentally different from CLI assembler — different entry point (WinMain vs main), different dependencies (D3D11, WASAPI, Win32 menus), different subsystem (WINDOWS vs CONSOLE) | Merging into Casso65 CLI would violate single responsibility and add GUI dependencies to a command-line tool |
| ~14 device component classes | Each device implements distinct hardware behavior (stepper motor state machine, bank-switching sequencing, speaker toggle, NTSC color decoding, etc.) that cannot be generalized | A single "GenericDevice" would require massive switch statements and violate SRP; the component registry architecture enables extensibility per FR-003 |
| Hand-written JSON parser | No third-party libs allowed; WinRT JSON APIs require UWP or C++/WinRT projection library (effectively third-party) | ~500-700 lines of self-contained code; well-defined schema makes this tractable and fully testable |
