# CLI Contract: Casso65Emu

**Feature**: `003-apple2-platform-emulator`
**Date**: 2025-07-22

## Executable

`Casso65Emu.exe` — Win32 GUI application (WINDOWS subsystem)

## Command-Line Arguments

```
Casso65Emu.exe --machine <name> [--disk1 <path>] [--disk2 <path>]
```

### Required Arguments

| Argument | Type | Description |
|----------|------|-------------|
| `--machine <name>` | string | Machine configuration name. Resolves to `Casso65Emu/machines/<name>.json`. |

### Optional Arguments

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `--disk1 <path>` | file path | (none) | .dsk disk image to insert in Drive 1 at boot. |
| `--disk2 <path>` | file path | (none) | .dsk disk image to insert in Drive 2 at boot. |

### Valid Machine Names

| Name | Config File | Description |
|------|-------------|-------------|
| `apple2` | `machines/apple2.json` | Apple II (original) with Integer BASIC ROM |
| `apple2plus` | `machines/apple2plus.json` | Apple II+ with Applesoft BASIC ROM |
| `apple2e` | `machines/apple2e.json` | Apple IIe with 128KB RAM, 65C02, 80-column |

### Examples

```bash
# Boot Apple II+ to BASIC prompt
Casso65Emu.exe --machine apple2plus

# Boot Apple II+ with DOS 3.3 disk
Casso65Emu.exe --machine apple2plus --disk1 dos33.dsk

# Boot Apple IIe with ProDOS
Casso65Emu.exe --machine apple2e --disk1 prodos.dsk

# Boot original Apple II (Integer BASIC)
Casso65Emu.exe --machine apple2
```

## Error Behavior

| Condition | Exit Code | Stderr Message |
|-----------|-----------|----------------|
| Missing `--machine` argument | 1 | `Error: --machine argument is required. Available machines: apple2, apple2plus, apple2e` |
| Unknown machine name | 1 | `Error: Unknown machine 'foo'. Available machines: apple2, apple2plus, apple2e` |
| Machine config JSON parse error | 1 | `Error: Failed to parse machines/<name>.json at line N, column M: <detail>` |
| Missing ROM file | 1 | `Error: ROM file not found: roms/<filename>. Place Apple II ROM images in the roms/ directory.` |
| Missing .dsk file (--disk1/--disk2) | 1 | `Error: Disk image not found: <path>` |
| Invalid .dsk file size | 1 | `Error: Disk image '<path>' is not a valid .dsk file (expected 143360 bytes, got N bytes)` |
| Overlapping device addresses | 1 | `Error: Address conflict in machine config: <device1> ($XXXX-$XXXX) overlaps <device2> ($XXXX-$XXXX)` |
| Unknown device type in config | 1 | `Error: Unknown device type '<type>' in machine config. Registered types: <list>` |
| D3D11 initialization failure | 1 | `Error: Failed to initialize Direct3D 11: <HRESULT description>` |
| WASAPI initialization failure | 0 | Warning logged to debug console; emulator runs without audio |

## Window Specification

| Property | Value |
|----------|-------|
| Title | `Casso65 — {machine name} [{state}]` (e.g., `Casso65 — Apple II+ [Running]`) |
| Client area | 560 × 384 pixels (fixed, no resize) |
| Style | `WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX` |
| Menu bar | File, Machine, Disk, View, Help |

## Keyboard Accelerators

| Key | Action |
|-----|--------|
| Ctrl+O | Open Machine Config... |
| Ctrl+R | Reset (warm) |
| Ctrl+Shift+R | Power Cycle (cold) |
| Pause | Pause/Resume emulation |
| F11 | Step one instruction (when paused) |
| Ctrl+1 | Insert disk in Drive 1 |
| Ctrl+Shift+1 | Eject disk from Drive 1 |
| Ctrl+2 | Insert disk in Drive 2 |
| Ctrl+Shift+2 | Eject disk from Drive 2 |
| Alt+Enter | Toggle fullscreen |
| Ctrl+D | Debug Console |
| F1 | Keyboard Map |
