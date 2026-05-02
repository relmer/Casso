# Research: Apple II Platform Emulator

**Feature**: `003-apple2-platform-emulator`
**Date**: 2025-07-22

## R-001: Direct3D 11 Rendering Pipeline

**Decision**: Minimal D3D11 setup with pre-compiled HLSL shaders and borderless fullscreen.

**Rationale**: D3D11 requires explicit shaders (no fixed-function pipeline). A vertex shader and pixel shader are mandatory even for a simple textured quad. Pre-compiling shaders via FXC at build time produces CPU-agnostic `.cso` bytecode that works on both x64 and ARM64, avoids runtime `d3dcompiler.dll` dependency, and provides faster startup.

**Alternatives considered**:
- **GDI/DIB**: Simpler but cannot support future CRT shader effects (HLSL pixel shaders). Would require rewrite later.
- **DXGI exclusive fullscreen**: Causes mode switches, black screen flickering, and multi-monitor issues. Borderless fullscreen window approach is simpler and more robust.
- **Runtime shader compilation (D3DCompile)**: Works but adds `d3dcompiler.dll` dependency and is unreliable on ARM64. Pre-compiled `.cso` is portable and faster.

**Key technical details**:
- **Headers**: `d3d11.h`, `dxgi.h` (via Windows SDK)
- **Link libraries**: `d3d11.lib`, `dxgi.lib`
- **Device creation**: `D3D11CreateDeviceAndSwapChain()` — single call creates both device and swap chain
- **Texture**: `D3D11_USAGE_DYNAMIC` with `D3D11_CPU_ACCESS_WRITE` for per-frame `Map`/`Unmap` upload
- **Sampler**: `D3D11_FILTER_MIN_MAG_MIP_POINT` for nearest-neighbor (crisp pixels)
- **Fullscreen**: Borderless window via `SetWindowLong(GWL_STYLE, WS_POPUP)` + `SetWindowPos()`. No DXGI fullscreen state changes.
- **Quad**: 4 vertices (position + texcoord), 6 indices. Static vertex/index buffers created once.
- **Shader build step**: FXC.exe custom build rule in `.vcxproj` compiling `.hlsl` → `.cso`. Load `.cso` at runtime via `CreateVertexShader()`/`CreatePixelShader()`.

---

## R-002: WASAPI Audio Streaming

**Decision**: Pull-mode WASAPI shared-mode with float32 mono at 44.1 kHz, 100ms buffer, submitted from CPU thread in 1ms slices.

**Rationale**: Pull-mode (polling) integrates cleanly with the 1ms execution slice architecture — audio samples generated per-slice are submitted to WASAPI immediately on the CPU thread. Shared-mode coexists with other Windows audio. Float32 is the modern WASAPI native format. 100ms WASAPI buffer provides headroom; a pending sample buffer decouples sample generation from WASAPI drain. Speaker amplitude is ±0.25f.

**Alternatives considered**:
- **Event-driven WASAPI**: More complex (requires separate thread or event wait), no benefit for a 1ms-slice emulator architecture.
- **DirectSound**: Legacy API, deprecated. WASAPI is the modern replacement. AppleWin uses DirectSound with audio-driven clock feedback — we adopted 1ms slices from AppleWin but with WASAPI instead.
- **PCM16 format**: Works but float32 is preferred by modern drivers; avoids quantization noise and format conversion overhead.

**Key technical details**:
- **Headers**: `<mmdeviceapi.h>`, `<audioclient.h>` (via Windows SDK)
- **Link libraries**: `ole32.lib` (for COM)
- **Interfaces**: `IMMDeviceEnumerator` → `IMMDevice` → `IAudioClient` → `IAudioRenderClient`
- **Initialization**: `CoInitializeEx(COINIT_MULTITHREADED)` on CPU thread → `CoCreateInstance(CLSID_MMDeviceEnumerator)` → `GetDefaultAudioEndpoint(eRender, eConsole)` → `Activate(IID_IAudioClient)` → `Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 100ms, 0, format, NULL)` → `GetBufferSize()` → `GetService(IID_IAudioRenderClient)` → `Start()`
- **Per-slice pattern**: Each 1ms execution slice generates audio samples → accumulated in pending buffer → `GetCurrentPadding()` → calculate available frames → `GetBuffer()` → drain pending samples to WASAPI → `ReleaseBuffer()`
- **Speaker sample generation**: Toggle events accumulated during CPU execution. Each toggle flips speaker state between +0.25f and -0.25f. Samples between toggles hold the current state. Cycle-accurate timing via `baseCycles` in Microcode.
- **Fallback**: If `Initialize()` returns `AUDCLNT_E_UNSUPPORTED_FORMAT`, retry with `GetMixFormat()`.
- **ARM64**: Fully supported, same API, no platform-specific code needed.

---

## R-003: Disk II Controller Emulation

**Decision**: Nibble-level emulation with 6-and-2 encoding/decoding, stepper motor state machine, and DOS 3.3 sector interleaving.

**Rationale**: Full nibble-level emulation is required for DOS 3.3 and ProDOS compatibility because the Disk II boot ROM reads nibble-encoded data directly. The .dsk format stores raw sector data that must be encoded on-the-fly into the nibble stream the ROM expects.

**Alternatives considered**:
- **Sector-level emulation (trap DOS calls)**: Would break ProDOS and any software that directly accesses the disk controller. Not viable for compatibility.
- **Pre-nibblized disk images (.nib format)**: Out of scope per spec. Would simplify the controller but requires a different file format.

**Key technical details**:

### Stepper Motor State Machine
- 4 phases controlled by $C0E0-$C0E7 (phase 0-3 OFF/ON)
- Track position stored as quarter-tracks (0-139, where logical track = quarter_track / 4)
- On phase change: detect which phases are energized, determine direction, move one quarter-track
- Clamp to 0-139 (tracks 0-34)

### Soft Switch Map ($C0E0-$C0EF for Slot 6)
| Offset | Address | Function |
|--------|---------|----------|
| 0-7 | $C0E0-$C0E7 | Phase 0-3 OFF/ON (stepper motor) |
| 8 | $C0E8 | Motor OFF |
| 9 | $C0E9 | Motor ON |
| A | $C0EA | Select Drive 1 |
| B | $C0EB | Select Drive 2 |
| C | $C0EC | Q6L (read mode) |
| D | $C0ED | Q6H (write mode) |
| E | $C0EE | Q7L (read/sense mode) |
| F | $C0EF | Q7H (load/write mode) |

### Q6/Q7 Data Path State Machine
| Q7 | Q6 | Mode | Read Returns |
|----|----|----|-------------|
| 0 | 0 | Read | Shift register (next nibble from disk) |
| 0 | 1 | Sense | Write-protect status (bit 7) |
| 1 | 0 | Write | Shift register output to disk |
| 1 | 1 | Load | Load data latch for write |

### 6-and-2 Encoding
- 256 source bytes → 342 six-bit values (86 secondary + 256 primary) → XOR encoding → 343 nibbles + checksum
- Write translate table: 64-entry lookup mapping 6-bit values to valid disk bytes (all ≥ $96, no consecutive zero bits)
- Read translate table: inverse 256-entry lookup

### DOS 3.3 Sector Interleave
```
Logical:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
Physical: 0  7 14  5 12  3 10  1  8 15  6 13  4 11  2  9
```

### Track Format (nibble stream)
- Gap 1: ~40-128 × $FF self-sync bytes
- Address field: D5 AA 96 + 4-and-4 encoded volume/track/sector/checksum + DE AA EB
- Gap 2: ~6 × $FF
- Data field: D5 AA AD + 342 6-and-2 encoded bytes + checksum + DE AA EB
- Gap 3: ~20 × $FF
- ~400 bytes per sector × 16 sectors = ~6,400 bytes per track

### .dsk File Layout
- 143,360 bytes = 35 tracks × 16 sectors × 256 bytes
- Sector offset: `(track × 16 + logical_sector) × 256`
- Sectors stored in logical (DOS) order

---

## R-004: NTSC Color Artifact Simulation

**Decision**: Pre-computed lookup table approach mapping hi-res byte patterns to RGBA pixel pairs.

**Rationale**: The Apple II produces color through NTSC artifact coloring — each pixel's screen column determines its color phase. A lookup table captures this relationship without requiring a full NTSC signal simulation. This is the approach used by production emulators (AppleWin, MAME, JACE).

**Alternatives considered**:
- **Full NTSC signal simulation (YIQ color space)**: More accurate but significantly more complex and slower. Not justified for functional emulation.
- **Per-pixel calculation**: Correct but slower than a lookup. The 7-pixel-per-byte × palette-bit state space is only 256 × 2 = 512 entries — easily pre-computed.

**Key technical details**:

### Color Artifact Rules
- **Even-column pixel ON**: Violet (palette 0) or Blue (palette 1)
- **Odd-column pixel ON**: Green (palette 0) or Orange (palette 1)
- **Two adjacent ON pixels**: White (regardless of palette)
- **OFF pixels**: Black
- Bit 7 of each hi-res byte selects palette (0 = violet/green, 1 = blue/orange)
- 7 pixels per byte (bits 0-6, LSB = leftmost), displayed at 2× horizontal (each pixel → 2 framebuffer pixels)

### Standard Apple II Color Values (RGBA)
| Color | R | G | B | Used When |
|-------|---|---|---|-----------|
| Black | 0 | 0 | 0 | Pixel OFF |
| White | 255 | 255 | 255 | Two adjacent ON pixels |
| Violet | 255 | 68 | 253 | Even column, palette 0 |
| Green | 20 | 245 | 60 | Odd column, palette 0 |
| Blue | 20 | 207 | 255 | Even column, palette 1 |
| Orange | 255 | 106 | 60 | Odd column, palette 1 |

### Algorithm
1. Pre-compute `NtscColorTable[512]` — indexed by `(palette_bit << 8) | byte_value`
2. Each entry contains 14 RGBA pixels (7 logical pixels × 2× doubling)
3. Per-scanline: for each of 40 bytes, look up the 14-pixel strip and copy to framebuffer
4. Cross-byte adjacency: check last pixel of previous byte against first pixel of current byte for white merging

### Monochrome Modes
- Convert RGBA framebuffer to luminance: `L = 0.299R + 0.587G + 0.114B`
- Tint: Green mono → `(0, L, 0)`, Amber → `(L, L×0.75, 0)`, White → `(L, L, L)`

---

## R-005: 65C02 Instruction Set Extensions

**Decision**: Extend existing opcode table in `EmuCpu` initialization with ~37 new valid instructions and guaranteed NOP behavior for all undefined opcodes.

**Rationale**: The Apple IIe requires 65C02 support. The existing Casso65Core `instructionSet` table (256 Microcode entries) can be extended by `EmuCpu` without modifying the base table used by existing tests. The 65C02 adds useful instructions (PHX/PHY/PLX/PLY, BRA, STZ, TSB, TRB) and a new addressing mode (zero-page indirect).

**Alternatives considered**:
- **Separate Cpu65C02 class**: Would duplicate too much code from the base Cpu class. Extending via `EmuCpu` is cleaner.
- **Modifying the base instruction table**: Would affect all existing unit tests. The 65C02 extensions are applied only when `EmuCpu` is constructed with `cpu: "65c02"`.

**Key new instructions**:
| Opcode | Mnemonic | Mode | Bytes | Cycles |
|--------|----------|------|-------|--------|
| $04 | TSB | Zero Page | 2 | 5 |
| $0C | TSB | Absolute | 3 | 6 |
| $14 | TRB | Zero Page | 2 | 5 |
| $1C | TRB | Absolute | 3 | 6 |
| $34 | BIT | Zero Page,X | 2 | 4 |
| $3C | BIT | Absolute,X | 3 | 4 |
| $5A | PHY | Implied | 1 | 3 |
| $64 | STZ | Zero Page | 2 | 3 |
| $74 | STZ | Zero Page,X | 2 | 4 |
| $7A | PLY | Implied | 1 | 4 |
| $7C | JMP | (Absolute,X) | 3 | 6 |
| $80 | BRA | Relative | 2 | 3 |
| $89 | BIT | Immediate | 2 | 2 |
| $92 | STA | (Zero Page) | 2 | 5 |
| $9C | STZ | Absolute | 3 | 4 |
| $9E | STZ | Absolute,X | 3 | 5 |
| $B2 | LDA | (Zero Page) | 2 | 5 |
| $DA | PHX | Implied | 1 | 3 |
| $FA | PLX | Implied | 1 | 4 |

**Behavioral fixes**:
- JMP indirect ($6C) page-crossing bug fixed (NMOS wraps within page, 65C02 crosses correctly)
- All undefined opcodes become guaranteed 1-byte 1-cycle NOPs (vs unpredictable on NMOS)

**New addressing mode**:
- Zero Page Indirect `($ZP)` — operand is 8-bit ZP address; CPU reads 16-bit effective address from ZP and ZP+1. No X/Y index.

**Metrics**: NMOS 6502 has 151 legal opcodes. 65C02 has 178 legal opcodes (+27 new). Remaining 78 slots are guaranteed NOPs.

---

## R-006: JSON Parsing Strategy

**Decision**: Hand-written recursive descent parser using only C++ STL.

**Rationale**: The "no third-party libraries" constraint eliminates all popular JSON libraries (nlohmann, RapidJSON, etc.). Windows.Data.Json (WinRT) requires C++/WinRT projection which is effectively a third-party dependency for Win32 desktop apps. IJsonReader (WWSAPI) is overengineered for config files. The machine config JSON schema is well-defined and simple (~50 lines, known fields), making a hand-written parser tractable.

**Alternatives considered**:
- **Windows.Data.Json (WinRT)**: Requires C++/WinRT projection library. Not available in standard Win32 desktop apps without NuGet packages. Architectural mismatch.
- **IJsonReader (WWSAPI)**: Streaming-only, COM-based, massive boilerplate for simple config reading. ~500+ lines of infrastructure for what should be a simple parse.
- **std::regex**: Fragile for nested structures, unreadable, slow. Cannot handle nested objects/arrays correctly.

**Implementation approach**:
- **Lexer**: Tokenize input into string/number/bool/null/punctuation tokens (~150-200 lines)
- **Parser**: Recursive descent producing `JsonValue` variant type (object, array, string, number, bool, null) (~250-350 lines)
- **Error reporting**: Line/column tracking for clear error messages
- **Total estimate**: ~500-700 lines across `JsonParser.h` and `JsonParser.cpp`
- **Schema validation**: Done in `MachineConfig` loader, not in the parser itself
- **Dependencies**: Only `<string>`, `<vector>`, `<unordered_map>`, `<sstream>` from STL

---

## R-007: AppleWin Architecture Research

**Decision**: Adopt AppleWin's 1ms execution slice approach with a dedicated CPU thread and WASAPI (replacing AppleWin's DirectSound).

**Rationale**: AppleWin is the most mature open-source Apple II emulator for Windows and provided key architectural insights that influenced our design, particularly around audio timing and NTSC clock derivation.

**Key findings from AppleWin**:

### Execution Granularity
- AppleWin is single-threaded but uses **1ms execution slices** rather than per-frame (16.6ms) granularity
- Each 1ms slice executes ~1,023 CPU cycles, then submits audio samples for that slice
- This fine granularity produces smoother audio with fewer pops/clicks compared to per-frame audio submission
- We adopted the 1ms slice approach but run it on a **dedicated CPU thread** with WASAPI instead of DirectSound

### Audio-Driven Clock Feedback
- AppleWin uses DirectSound buffer fill level to adjust the number of CPU cycles per slice
- When the audio buffer is running low, more cycles are executed; when full, fewer cycles
- This creates a closed-loop feedback system where audio timing drives CPU speed
- We opted for a simpler approach: fixed 1ms slices with a pending sample buffer to decouple generation from WASAPI drain, plus a 100ms WASAPI buffer for headroom

### NTSC Timing Derivation
- The Apple II's master clock is a **14.31818 MHz** NTSC color burst crystal
- CPU clock = 14.31818 MHz / 14 = **1,022,727 Hz** (not 1,023,000 Hz as often approximated)
- Each scanline is 65 CPU cycles; each frame is 262 scanlines (including VBL)
- Cycles per frame = 65 × 262 = **17,030** (not clockSpeed/60 ≈ 17,050)
- These constants are defined as `kAppleCpuClock` and `kAppleCyclesPerFrame` in `MachineConfig.h`

### Threading Architecture (our divergence)
- AppleWin runs everything single-threaded including Win32 message pump
- We split into CPU thread (6502 execution, audio submission, framebuffer rendering) and UI thread (Win32 message pump, D3D11 Present(1) with vsync, keyboard dispatch)
- Shared state: atomic keyboard latch, mutex-protected framebuffer, atomic flags, command queue
- WASAPI init/submit/shutdown all on CPU thread with its own `CoInitializeEx`
