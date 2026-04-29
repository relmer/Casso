# Quickstart: Apple II Platform Emulator

**Feature**: `003-apple2-platform-emulator`
**Date**: 2025-07-22

## Prerequisites

- **Visual Studio 2026** with MSVC v145 toolset (Desktop development with C++ workload)
- **Windows SDK** (10.0.26100.0 or later) — provides D3D11, DXGI, WASAPI headers and libs
- **Apple II ROM images** (user-supplied, not included — see [ROM Setup](#rom-setup))

## Build

### Via VS Code Tasks (Recommended)

```
Ctrl+Shift+B → Build + Test Debug    (builds all projects, runs tests)
Ctrl+Shift+B → Build + Test Release  (builds all projects, runs tests)
```

### Via PowerShell

```powershell
# Debug build (x64)
& scripts/Build.ps1 -Configuration Debug -Platform x64

# Release build (ARM64)
& scripts/Build.ps1 -Configuration Release -Platform ARM64
```

### Via MSBuild Directly

```powershell
msbuild Casso65.sln /p:Configuration=Debug /p:Platform=x64
```

The solution builds four projects:
1. **Casso65Core** — static library (existing, unchanged)
2. **Casso65** — console assembler CLI (existing, unchanged)
3. **Casso65Emu** — Win32 GUI emulator (NEW)
4. **UnitTest** — test DLL (existing, with new emulator tests)

## ROM Setup

ROM images are copyrighted and not distributed with the project. Place them in:

```
Casso65Emu/roms/
├── apple2.rom           # Apple II (Integer BASIC) — 12KB
├── apple2plus.rom       # Apple II+ (Applesoft BASIC) — 12KB
├── apple2e.rom          # Apple IIe system ROM
└── apple2e-char.rom     # Apple IIe character generator ROM
```

A helper script can fetch ROMs from the AppleWin project:

```powershell
& scripts/FetchRoms.ps1
```

The `roms/` directory is gitignored.

## Run

### Boot to BASIC Prompt (Apple II+)

```powershell
# From solution root:
x64/Debug/Casso65Emu.exe --machine apple2plus
```

Expected: Window opens with green-on-black 40-column text displaying the Applesoft BASIC `]` prompt.

### Boot DOS 3.3 from Disk

```powershell
x64/Debug/Casso65Emu.exe --machine apple2plus --disk1 path/to/dos33.dsk
```

Expected: DOS 3.3 boots, displays greeting, presents `]` prompt. Type `CATALOG` to list files.

### Boot Apple IIe

```powershell
x64/Debug/Casso65Emu.exe --machine apple2e --disk1 path/to/prodos.dsk
```

Expected: Apple IIe boots with 65C02, supports lowercase input. `PR#3` activates 80-column mode.

### Boot Original Apple II (Integer BASIC)

```powershell
x64/Debug/Casso65Emu.exe --machine apple2
```

Expected: Window displays Integer BASIC `>` prompt.

## Test

### Run All Tests (Existing + New Emulator Tests)

```powershell
# Via VS Code task:
Ctrl+Shift+B → Build + Test Debug

# Via command line:
vstest.console.exe x64/Debug/UnitTest.dll
```

All 577+ existing tests continue to pass. New emulator-specific tests cover:
- MemoryBus address routing and conflict detection
- JSON parser (valid configs, error cases, edge cases)
- MachineConfig loading and validation
- Device components (RAM, ROM, keyboard, speaker, soft switches)
- Video renderers (text mode row addressing, lo-res colors, hi-res NTSC artifacts)
- Disk II (sector interleaving, 6-and-2 encoding/decoding, stepper motor)
- Language Card (bank-switching state machine)

## In-App Controls

### Keyboard

| Key | Action |
|-----|--------|
| Ctrl+R | Warm reset (preserves RAM) |
| Ctrl+Shift+R | Cold boot (clears RAM, reinitializes devices) |
| Pause | Pause/resume emulation |
| F11 | Step one CPU instruction (when paused) |
| Alt+Enter | Toggle fullscreen |
| Ctrl+D | Open debug console |
| Ctrl+1 | Insert disk in Drive 1 |
| Ctrl+2 | Insert disk in Drive 2 |

### Menu Bar

- **File**: Open machine config, recent machines, exit
- **Machine**: Reset, power cycle, pause, step, speed selection, machine info
- **Disk**: Insert/eject/write-protect per drive, write mode selection
- **View**: Fullscreen, display filter, color mode (Color/Green/Amber/White mono)
- **Help**: Keyboard map, debug console, about

## Project Layout

```
Casso65Emu/
├── Core/        # MemoryBus, MemoryDevice, ComponentRegistry, EmuCpu, JsonParser
├── Devices/     # RamDevice, RomDevice, AppleKeyboard, AppleSpeaker, LanguageCard, DiskII, etc.
├── Video/       # AppleTextMode, AppleLoResMode, AppleHiResMode, CharacterRom, NtscColorTable
├── Audio/       # WasapiAudio (WASAPI shared-mode)
├── Shell/       # EmulatorShell, D3DRenderer, MenuSystem, DebugConsole
├── Resources/   # resource.h, Casso65Emu.rc
├── machines/    # apple2.json, apple2plus.json, apple2e.json
├── roms/        # (gitignored) user-supplied ROM images
└── shaders/     # VertexShader.hlsl, PixelShader.hlsl (compiled at build time)
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "ROM file not found" | Place ROM files in `Casso65Emu/roms/` or run `scripts/FetchRoms.ps1` |
| "Unknown machine" | Check spelling. Valid names: `apple2`, `apple2plus`, `apple2e` |
| No audio | Check Windows audio output device. WASAPI failure is non-fatal (warning in debug console). |
| Black screen | Verify ROM file is correct size and not corrupted. Check debug console (Ctrl+D). |
| Existing tests fail | Verify Casso65Core changes are limited to adding `virtual` keyword on 4 protected methods in `Cpu.h`. No public API changes. |
