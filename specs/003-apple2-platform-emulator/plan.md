# Implementation Plan: Apple II Platform Emulator

**Branch**: `003-apple2-platform-emulator` | **Date**: 2025-07-22 | **Spec**: `specs/003-apple2-platform-emulator/spec.md`
**Input**: Feature specification from `specs/003-apple2-platform-emulator/spec.md`

## Summary

Add a GUI-based Apple II platform emulator to the Casso65 solution as two new projects: `Casso65EmuCore` (static library with emulator core logic — devices, video, audio, config) and `Casso65Emu` (Win32 GUI application). The emulator links against the existing Casso65Core static library and provides data-driven machine configurations (JSON) for Apple II, Apple II+, and Apple IIe. All three target machines use the NMOS 6502 CPU (65C02 support for Enhanced IIe and //c is out of scope). It renders via Direct3D 11 with ComPtr<T> COM management, generates audio through WASAPI with 1ms execution slices on a dedicated CPU thread, and uses a component registry architecture to map named device types to C++ classes. The emulator supports text mode, lo-res graphics, hi-res graphics with NTSC color artifacting, 80-column/double hi-res (IIe), Disk II controller emulation (.dsk format), and Language Card bank-switching. No third-party libraries are used.

## Technical Context

**Language/Version**: C++ /std:c++latest (MSVC v145, Visual Studio 2026)
**Primary Dependencies**: Windows SDK (D3D11, DXGI, WASAPI, Win32), C++ STL — no third-party libraries
**Storage**: .dsk disk images (140KB files), JSON machine config files, ROM files (user-supplied, gitignored)
**Testing**: Microsoft C++ Unit Test Framework (CppUnitTestFramework) — existing UnitTest project
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Desktop application (Win32 GUI) + static library extension
**Performance Goals**: 60 fps video output, 1,022,727 Hz emulated CPU clock (14.31818 MHz / 14), 100ms WASAPI audio buffer with 1ms execution slices, <50ms keyboard response
**Constraints**: CPU thread + UI thread architecture, no third-party libs, resizable window with Per-Monitor V2 DPI awareness, 100ms WASAPI audio buffer
**Scale/Scope**: 3 machine configurations (Apple II, II+, IIe), ~14 device component classes, ~65 source files across Casso65EmuCore and Casso65Emu

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Code Quality — ✅ PASS

- **EHM macros**: All HRESULT-returning functions will use CHR/CBR/CWRA/BAIL_OUT_IF patterns. D3D11, WASAPI, and Win32 COM calls return HRESULT — perfect fit for EHM.
- **Single exit point**: All functions via `Error:` label. No early returns.
- **Formatting**: 5 blank lines between top-level constructs, 3 blank lines after declarations. Column alignment preserved.
- **Smart pointers**: `unique_ptr` for device ownership in MemoryBus. `ComPtr<T>` (`Microsoft::WRL::ComPtr`) for COM pointer management — D3DRenderer has 12 ComPtr members, WasapiAudio has 4. No manual `Release()` calls.

### II. Testing Discipline — ✅ PASS

- **No system state in tests**: All device components (RAM, ROM, keyboard, speaker, soft switches) will have interfaces tested with synthetic data. MemoryBus tested with mock MemoryDevices. JSON parser tested with string input. Video renderers tested with synthetic framebuffers.
- **System API isolation**: D3D11, WASAPI, Win32 window creation are in the EmulatorShell (not testable via unit tests). All logic behind those APIs is factored into pure functions that accept data, not handles.
- **Existing tests preserved**: FR-019 requires all 787+ existing tests pass unchanged.

### III. User Experience Consistency — ✅ PASS

- **CLI syntax**: `Casso65Emu.exe --machine <name> --disk1 <path> --disk2 <path>` follows established `--flag` pattern.
- **Error messages**: Clear, actionable errors for missing ROM, unknown device, invalid JSON, overlapping addresses.

### IV. Performance Requirements — ✅ PASS

- **1,022,727 Hz emulation** (14.31818 MHz / 14): 17,030 cycles per 60 Hz frame (65 cycles × 262 scanlines) — trivially fast on modern hardware.
- **CPU thread + UI thread**: Dedicated CPU thread for 6502 execution (1ms slices), WASAPI audio submission, and framebuffer rendering. UI thread for Win32 message pump and D3D11 Present(1) with vsync. Shared state uses atomic keyboard latch, mutex-protected framebuffer, atomic flags, and command queue.
- **Minimal allocation**: Framebuffer allocated once (560×384×4 = 860KB). Device components allocated at startup only.

### V. Simplicity & Maintainability — ✅ PASS

- **YAGNI**: No CRT shader (grayed-out menu item), no .nib/.woz formats, no network play. Only what's specified.
- **Single responsibility**: Each device class handles one hardware component. MemoryBus only routes addresses. EmulatorShell only coordinates frame loop.
- **Function size**: All functions under 50 lines. Video renderers use helper functions for row address calculation, byte decoding, color lookup.
- **File scope**: Casso65EmuCore static library (extracted from Casso65Emu for faster test builds) + Casso65Emu Win32 GUI. Total: 5 projects (Casso65Core, Casso65EmuCore, Casso65Emu, Casso65, UnitTest). Casso65Core change is minimal (`virtual` keyword on 4 protected methods).

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
Casso65.sln                          # Updated — adds Casso65EmuCore and Casso65Emu projects
├── Casso65Core/                     # Existing static library (minimal change: virtual keyword)
│   ├── Cpu.h                        #   ReadByte/WriteByte/ReadWord/WriteWord → virtual
│   └── (all other files unchanged)
├── Casso65/                         # Existing console app (unchanged)
├── UnitTest/                        # Existing tests (787+ tests, with new emulator tests)
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
├── Casso65EmuCore/                  # NEW — Static library (extracted for faster test builds)
│   ├── Casso65EmuCore.vcxproj
│   ├── Pch.h / Pch.cpp             #   Precompiled header (using namespace std; namespace fs = std::filesystem)
│   ├── Core/                        #   Core emulator architecture
│   │   ├── MemoryBus.h/.cpp         #     Address-space router
│   │   ├── MemoryDevice.h           #     Interface for bus-attached devices
│   │   ├── ComponentRegistry.h/.cpp #     Factory registry (string → class)
│   │   ├── MachineConfig.h/.cpp     #     JSON config loader, timing constants (kAppleCpuClock, kAppleCyclesPerFrame)
│   │   ├── JsonParser.h/.cpp        #     Hand-written recursive descent JSON parser
│   │   ├── EmuCpu.h/.cpp            #     Cpu subclass routing through MemoryBus
│   │   └── PathResolver.h/.cpp      #     File path resolution using std::filesystem::path
│   ├── Devices/                     #   Hardware device components
│   │   ├── RamDevice.h/.cpp
│   │   ├── RomDevice.h/.cpp
│   │   ├── AppleKeyboard.h/.cpp
│   │   ├── AppleIIeKeyboard.h/.cpp
│   │   ├── AppleSpeaker.h/.cpp
│   │   ├── AppleSoftSwitchBank.h/.cpp
│   │   ├── AppleIIeSoftSwitchBank.h/.cpp
│   │   ├── LanguageCard.h/.cpp
│   │   ├── AuxRamCard.h/.cpp
│   │   └── DiskIIController.h/.cpp
│   ├── Video/                       #   Video renderers
│   │   ├── VideoOutput.h            #     Interface for video mode renderers
│   │   ├── AppleTextMode.h/.cpp
│   │   ├── Apple80ColTextMode.h/.cpp
│   │   ├── AppleLoResMode.h/.cpp
│   │   ├── AppleHiResMode.h/.cpp
│   │   ├── AppleDoubleHiResMode.h/.cpp
│   │   ├── CharacterRom.h           #     Embedded 2KB Apple II/II+ glyph data
│   │   └── NtscColorTable.h         #     Pre-computed NTSC artifact color LUTs
│   └── Audio/                       #   Audio subsystem
│       └── AudioGenerator.h/.cpp    #     Audio sample generation (decoupled from WASAPI)
└── Casso65Emu/                      # NEW — Win32 GUI application (flat structure, no subdirs)
    ├── Casso65Emu.vcxproj
    ├── Pch.h / Pch.cpp              #   Precompiled header (includes <windows.h>, D3D11, ComPtr<T>, etc.)
    ├── Main.cpp                     #   WinMain, CPU thread launch, emulation loop
    ├── Window.h/.cpp                #   Base class with virtual On* handlers (ported from Mandelbrot repo)
    ├── EmulatorShell.h/.cpp         #   Main app class (derives from Window), frame loop, timing
    ├── D3DRenderer.h/.cpp           #   D3D11 device, swap chain, texture upload (12 ComPtr members)
    ├── WasapiAudio.h/.cpp           #   WASAPI shared-mode audio stream (4 ComPtr members)
    ├── MenuSystem.h/.cpp            #   Table-driven Win32 menu creation and command dispatch
    ├── DebugConsole.h/.cpp          #   Debug log window (derives from Window, lazy-init, real Show/Hide)
    ├── resource.h                   #   Win32 resource IDs
    ├── Casso65Emu.rc                #   Menu accelerators, icon, version info
    ├── machines/                    #   JSON machine config files
    │   ├── apple2.json
    │   ├── apple2plus.json
    │   └── apple2e.json
    ├── roms/                        #   ROM images (gitignored)
    └── shaders/                     #   HLSL shaders (compiled at build time via FXC)
        ├── VertexShader.hlsl
        └── PixelShader.hlsl
```

**Structure Decision**: Two new projects added: `Casso65EmuCore` static library (Core, Devices, Video, Audio — extracted for faster test builds) and `Casso65Emu` Win32 GUI application (flat structure — Window base class, EmulatorShell, D3DRenderer, WasapiAudio, MenuSystem, DebugConsole). The Casso65Core static library receives one minimal change (4 methods become virtual). All new unit tests go into a new `EmuTests/` folder within the existing UnitTest project. Total: 5 projects, ~65 source files across EmuCore and Emu.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| 4th+5th projects (Casso65EmuCore + Casso65Emu) | GUI emulator is fundamentally different from CLI assembler — different entry point (WinMain vs main), different dependencies (D3D11, WASAPI, Win32 menus), different subsystem (WINDOWS vs CONSOLE). Casso65EmuCore extracted as static library so UnitTest can link it without pulling in Win32 GUI dependencies (faster test builds) | Merging into Casso65 CLI would violate single responsibility and add GUI dependencies to a command-line tool; a single Casso65Emu project would require UnitTest to link GUI code |
| ~14 device component classes | Each device implements distinct hardware behavior (stepper motor state machine, bank-switching sequencing, speaker toggle, NTSC color decoding, etc.) that cannot be generalized | A single "GenericDevice" would require massive switch statements and violate SRP; the component registry architecture enables extensibility per FR-003 |
| Hand-written JSON parser | No third-party libs allowed; WinRT JSON APIs require UWP or C++/WinRT projection library (effectively third-party) | ~500-700 lines of self-contained code; well-defined schema makes this tractable and fully testable |
