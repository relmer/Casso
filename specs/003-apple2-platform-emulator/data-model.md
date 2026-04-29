# Data Model: Apple II Platform Emulator

**Feature**: `003-apple2-platform-emulator`
**Date**: 2025-07-22

## Entity Overview

```
┌──────────────┐     owns      ┌──────────────┐     owns      ┌──────────────┐
│ EmulatorShell │──────────────▸│  MemoryBus   │──────────────▸│ MemoryDevice │
│              │               │              │  0..*         │  (interface) │
│ - hwnd       │               │ - devices[]  │               ├──────────────┤
│ - emuCpu     │               │ - ranges[]   │               │ + Read()     │
│ - memoryBus  │               │              │               │ + Write()    │
│ - d3dRenderer│               │ + ReadByte() │               │ + GetStart() │
│ - wasapiAudio│               │ + WriteByte()│               │ + GetEnd()   │
│ - menuSystem │               │ + Validate() │               └──────┬───────┘
└──────────────┘               └──────────────┘                      │
       │                              ▲                              │ implements
       │ owns                         │                    ┌─────────┼──────────┐
       ▼                              │                    │         │          │
┌──────────────┐              ┌───────┴──────┐    ┌────────┴──┐  ┌──┴────┐  ┌──┴──────────┐
│   EmuCpu     │──────────────│ComponentReg. │    │ RamDevice │  │RomDev │  │Apple-specific│
│              │  routes via  │              │    └───────────┘  └───────┘  │  devices     │
│ - memBusRef  │              │ + Register() │                              └──────────────┘
│ - is65C02    │              │ + Create()   │
│ - cycleCount │              └──────────────┘
└──────────────┘
```

## Entities

### MachineConfig

Represents a complete machine definition loaded from JSON.

| Field | Type | Constraints | Source |
|-------|------|-------------|--------|
| name | string | Non-empty, unique across configs | JSON `name` |
| cpu | string | One of: `"6502"`, `"65c02"` | JSON `cpu` |
| clockSpeed | uint32_t | > 0, typically 1023000 | JSON `clockSpeed` |
| memoryRegions | vector\<MemoryRegion\> | No overlapping ranges | JSON `memory[]` |
| devices | vector\<DeviceConfig\> | Types must exist in registry | JSON `devices[]` |
| videoConfig | VideoConfig | At least one mode | JSON `video` |
| keyboardType | string | Must exist in registry | JSON `keyboard.type` |

**MemoryRegion**:
| Field | Type | Constraints |
|-------|------|-------------|
| type | string | `"ram"` or `"rom"` |
| start | Word (uint16_t) | 0x0000–0xFFFF |
| end | Word (uint16_t) | ≥ start |
| file | string | Required for `"rom"`, path to ROM file |
| bank | string | Optional: `"aux"` for IIe auxiliary RAM |
| target | string | Optional: `"chargen"` for character ROM |

**DeviceConfig**:
| Field | Type | Constraints |
|-------|------|-------------|
| type | string | Must exist in ComponentRegistry |
| address | Word | For point-mapped devices (single address) |
| start | Word | For range-mapped devices |
| end | Word | For range-mapped devices, ≥ start |
| slot | int | 1–7 for slot-based devices (auto-maps I/O + ROM ranges) |

**VideoConfig**:
| Field | Type | Constraints |
|-------|------|-------------|
| modes | vector\<string\> | Each must be a registered video mode component |
| width | int | > 0, typically 560 |
| height | int | > 0, typically 384 |

**Validation rules**:
- All `memory[].type == "rom"` entries must reference a file that exists at load time
- No two memory regions or device address ranges may overlap (checked at startup)
- All `devices[].type` values must be registered in the ComponentRegistry
- Slot-based devices auto-expand to I/O range `$C080 + slot×16` through `$C08F + slot×16` AND slot ROM `$Cs00–$CsFF`

---

### MemoryBus

Central address-space router. Maps address ranges to MemoryDevice instances.

| Field | Type | Description |
|-------|------|-------------|
| entries | vector\<BusEntry\> | Sorted by start address |
| floatingBusValue | Byte | Last value read from data bus (returned for unmapped I/O) |

**BusEntry**:
| Field | Type | Description |
|-------|------|-------------|
| start | Word | First address in range |
| end | Word | Last address in range (inclusive) |
| device | MemoryDevice* | Pointer to the device handling this range |

**Operations**:
- `ReadByte(Word address) → Byte`: Linear scan of entries to find matching range; returns `floatingBusValue` if no match in $C000–$CFFF I/O space
- `WriteByte(Word address, Byte value)`: Same lookup; writes ignored for ROM devices
- `Validate() → HRESULT`: Checks for overlapping address ranges at startup

---

### MemoryDevice (Interface)

Abstract interface for all bus-attached components.

| Method | Signature | Description |
|--------|-----------|-------------|
| Read | `Byte Read(Word address)` | Read byte at address (relative to device's mapped range) |
| Write | `void Write(Word address, Byte value)` | Write byte at address |
| GetStart | `Word GetStart() const` | First address of mapped range |
| GetEnd | `Word GetEnd() const` | Last address of mapped range (inclusive) |
| Reset | `void Reset()` | Reinitialize device state (for power cycle) |

**Static factory pattern**:
```cpp
static std::unique_ptr<MemoryDevice> Create(const DeviceConfig& config, MemoryBus& bus);
```

---

### ComponentRegistry

Factory registry mapping string device type names to C++ constructors.

| Field | Type | Description |
|-------|------|-------------|
| factories | unordered_map\<string, FactoryFunc\> | Type name → factory function |

**FactoryFunc**: `std::function<std::unique_ptr<MemoryDevice>(const DeviceConfig&, MemoryBus&)>`

**Operations**:
- `Register(string typeName, FactoryFunc factory)`: Register a device type
- `Create(string typeName, DeviceConfig config, MemoryBus& bus) → unique_ptr<MemoryDevice>`: Instantiate by name; returns error for unknown types

---

### EmuCpu

Subclass of Casso65Core `Cpu`. Routes memory access through MemoryBus.

| Field | Type | Description |
|-------|------|-------------|
| memoryBus | MemoryBus& | Reference to the memory bus |
| is65C02 | bool | True when config specifies `"cpu": "65c02"` |
| cyclesThisStep | uint32_t | Cycles consumed by the most recent `StepOne()` call |
| totalCycles | uint64_t | Running total for frame timing |

**Overridden methods** (from base `Cpu`, made `virtual`):
- `ReadByte(Word address) → Byte` → delegates to `memoryBus.ReadByte()`
- `WriteByte(Word address, Byte value)` → delegates to `memoryBus.WriteByte()`
- `ReadWord(Word address) → Word` → two `ReadByte` calls (little-endian)
- `WriteWord(Word address, Word value)` → two `WriteByte` calls

**65C02 initialization**: When `is65C02 == true`, constructor patches `instructionSet[]` with 65C02 opcodes (see research.md R-005).

---

### RamDevice

| Field | Type | Description |
|-------|------|-------------|
| data | vector\<Byte\> | Backed by `end - start + 1` bytes |
| start | Word | Mapped start address |
| end | Word | Mapped end address |

**Read**: Return `data[address - start]`
**Write**: Set `data[address - start] = value`
**Reset**: Zero-fill all bytes

---

### RomDevice

| Field | Type | Description |
|-------|------|-------------|
| data | vector\<Byte\> | Loaded from ROM file at startup |
| start | Word | Mapped start address |
| end | Word | Mapped end address |

**Read**: Return `data[address - start]`
**Write**: Ignored (ROM is read-only)
**Reset**: No-op (ROM contents are immutable)

---

### AppleSoftSwitchBank

Manages Apple II video mode state.

| Field | Type | Description |
|-------|------|-------------|
| graphicsMode | bool | True when $C050 set (graphics), false when $C051 (text) |
| mixedMode | bool | True when $C053 set (4 lines of text at bottom) |
| page2 | bool | True when $C055 set (display page 2) |
| hiresMode | bool | True when $C057 set (hi-res), false = lo-res |

**State transitions**: Read or write to any address $C050–$C057 toggles the corresponding flag. Even addresses set the flag; odd addresses clear it (or vice versa per Apple II convention — see spec memory map).

---

### LanguageCard

Bank-switching state machine for $D000–$FFFF.

| Field | Type | Description |
|-------|------|-------------|
| ramBank2 | vector\<Byte\> | 4KB bank 2 at $D000–$DFFF |
| ramMain | vector\<Byte\> | 8KB at $E000–$FFFF |
| readRam | bool | True = reads come from RAM; false = ROM |
| writeRam | bool | True = writes go to RAM; false = ignored |
| preWrite | bool | Write-enable requires two consecutive reads of the same switch |
| bank2Select | bool | True = $D000–$DFFF maps to bank 2 |

**State transitions** ($C080–$C08F): Determined by bits 0-3 of the soft switch address. The write-enable latch requires two consecutive reads of a write-enable switch (preWrite tracking).

---

### DiskIIController

Emulates the Disk II controller in a slot.

| Field | Type | Description |
|-------|------|-------------|
| phases | uint8_t | Bit mask of energized stepper phases (bits 0-3) |
| quarterTrack | int | Current head position (0-139) |
| motorOn | bool | Drive motor state |
| activeDrive | int | 1 or 2 |
| q6 | bool | Q6 latch state |
| q7 | bool | Q7 latch state |
| shiftRegister | Byte | Current byte being shifted from/to disk |
| diskImages | DiskImage[2] | One per drive |
| nibbleBuffer | vector\<Byte\> | Pre-nibblized current track (~6,400 bytes) |
| nibblePos | size_t | Current read position in nibble buffer |

**State machine**: See research.md R-003 for stepper motor and Q6/Q7 details.

---

### DiskImage

Represents a mounted .dsk file.

| Field | Type | Description |
|-------|------|-------------|
| filePath | string | Path to the .dsk file |
| data | array\<Byte, 143360\> | Full disk image in memory |
| dirty | bool | True if any sector has been written |
| writeProtected | bool | User-controlled write protect flag |
| writeMode | enum | BufferAndFlush or CopyOnWrite |
| deltaPath | string | Path to .delta sidecar file (COW mode) |

**Operations**:
- `ReadSector(track, logicalSector) → Byte[256]`: Read using DOS 3.3 interleave table
- `WriteSector(track, logicalSector, data[256])`: Write with interleave; set dirty flag
- `Flush()`: Write modified data to .dsk file (buffer-and-flush) or .delta file (COW)
- `IsWriteProtected() → bool`: Check write protection

---

### VideoOutput (Interface)

Interface for video mode renderers.

| Method | Signature | Description |
|--------|-----------|-------------|
| Render | `void Render(const Byte* videoRam, uint32_t* framebuffer, int fbWidth, int fbHeight)` | Read video RAM, write RGBA pixels |
| GetActivePageAddress | `Word GetActivePageAddress(bool page2) const` | Return base address of active video page |

**Implementations**: AppleTextMode, Apple80ColTextMode, AppleLoResMode, AppleHiResMode, AppleDoubleHiResMode.

---

### AppleSpeaker

| Field | Type | Description |
|-------|------|-------------|
| speakerState | float | Current output level (+0.3f or -0.3f) |
| toggleTimestamps | vector\<uint32_t\> | CPU cycle counts when $C030 was read this frame |

**Operation**: On `Read($C030)`, toggle `speakerState` and record timestamp. At frame end, `WasapiAudio` converts timestamps to PCM samples at 44.1 kHz.

---

### EmulatorShell

Main application class.

| Field | Type | Description |
|-------|------|-------------|
| hwnd | HWND | Win32 window handle |
| memoryBus | MemoryBus | Central address-space router |
| emuCpu | unique_ptr\<EmuCpu\> | CPU instance |
| d3dRenderer | D3DRenderer | D3D11 device, swap chain, texture |
| wasapiAudio | WasapiAudio | WASAPI audio stream |
| menuSystem | MenuSystem | Win32 menu bar |
| videoRenderers | vector\<unique_ptr\<VideoOutput\>\> | All video modes |
| activeVideoMode | VideoOutput* | Currently active renderer |
| framebuffer | vector\<uint32_t\> | 560×384 RGBA pixel buffer |
| machineConfig | MachineConfig | Loaded configuration |
| running | bool | Emulation running state |
| paused | bool | Emulation paused state |
| speedMultiplier | int | 1 (authentic), 2, or 0 (maximum) |
| colorMode | enum | Color, GreenMono, AmberMono, WhiteMono |

**Frame loop** (integrated with Win32 message pump):
1. `PeekMessage()` — process all pending Win32 messages
2. Execute ~17,030 CPU cycles via `emuCpu->StepOne()` in a loop
3. Call `activeVideoMode->Render()` to fill framebuffer
4. Apply monochrome tint if `colorMode != Color`
5. `d3dRenderer.UploadAndPresent(framebuffer)`
6. `wasapiAudio.SubmitFrame(speaker.toggleTimestamps)`
7. Sleep for remaining frame time (`QueryPerformanceCounter` timing)

---

## Entity Relationship Summary

```
MachineConfig ──loads──▸ ComponentRegistry ──creates──▸ MemoryDevice instances
                                                              │
EmulatorShell ──owns──▸ MemoryBus ──routes──▸ MemoryDevice instances
      │                     ▲
      │                     │
      └──owns──▸ EmuCpu ───┘ (reads/writes via bus)
      │
      ├──owns──▸ D3DRenderer (texture upload + present)
      ├──owns──▸ WasapiAudio (audio buffer submission)
      ├──owns──▸ VideoOutput[] (framebuffer rendering)
      └──owns──▸ MenuSystem (Win32 menu dispatch)
```
