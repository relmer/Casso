# Machine Configuration JSON Schema

**Feature**: `003-apple2-platform-emulator`
**Schema Version**: 2 (refactored)

## File Location

Machine configs are stored in `Casso/Machines/` as `.json` files. The `--machine <name>` CLI argument resolves to `Machines/<name>.json`.

## Schema

```json
{
    "name": "<string>",
    "cpu": "<string>",
    "timing": {
        "videoStandard": "<string>",
        "clockSpeed": <integer>,
        "cyclesPerScanline": <integer>
    },
    "ram": [
        {
            "address": "<hex-string>",
            "size":    "<hex-string>",
            "bank":    "<string>"
        }
    ],
    "systemRom": {
        "address": "<hex-string>",
        "file":    "<string>"
    },
    "characterRom": {
        "file":    "<string>"
    },
    "internalDevices": [
        { "type": "<string>" }
    ],
    "slots": [
        {
            "slot":   <integer>,
            "device": "<string>",
            "rom":    "<string>"
        }
    ],
    "video": {
        "modes": ["<string>"]
    },
    "keyboard": {
        "type": "<string>"
    }
}
```

## Field Reference

### Root Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Display name (status bar, machine picker) |
| `cpu` | string | Yes | `"6502"` (only currently supported) |
| `timing` | object | Yes | NTSC/PAL timing parameters |
| `ram` | array | Yes | RAM regions (may be empty) |
| `systemRom` | object | Yes | The main system ROM |
| `characterRom` | object | No | Character generator ROM (used by video) |
| `internalDevices` | array | Yes | Motherboard I/O devices |
| `slots` | array | No | Expansion slots 1-7 |
| `video` | object | Yes | Video mode list |
| `keyboard` | object | Yes | Keyboard type |

### timing Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `videoStandard` | string | Yes | `"ntsc"` or `"pal"` |
| `clockSpeed` | integer | Yes | CPU clock in Hz |
| `cyclesPerScanline` | integer | Yes | CPU cycles per scanline |

### ram[] Entry

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `address` | hex-string | Yes | Start address `"0x0000"` |
| `size` | hex-string | Yes | Size in bytes `"0xC000"` (1..0x10000) |
| `bank` | string | No | `"aux"` for IIe auxiliary RAM (created by AuxRamCard, not main bus) |

### systemRom Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `address` | hex-string | Yes | Base address (size determined by file size) |
| `file` | string | Yes | Filename relative to `ROMs/` directory |

### characterRom Object

Character ROM is not mapped on the CPU bus, it's read by the video circuitry.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `file` | string | Yes | Filename relative to `ROMs/` directory |

### internalDevices[] Entry

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Registered device type (see below) |

Internal devices have hardcoded address mappings. The `type` is the only field needed.

### slots[] Entry

A slot represents an expansion card. At least one of `device` or `rom` must be provided.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `slot` | integer | Yes | 1..7 |
| `device` | string | No | Registered slot device type |
| `rom` | string | No | Slot ROM filename (256 bytes, mapped at $Cs00-$CsFF) |

The slot ROM auto-maps to address `$C000 + slot * 0x100`. Slot device I/O auto-maps to `$C080 + slot * 16`.

### video Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `modes` | string[] | Yes | Video mode names (see below) |

### keyboard Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Registered keyboard type |

## Registered Component Types

### Internal Device Types

| Type | Class | Address Range | Purpose |
|------|-------|---------------|---------|
| `apple2-keyboard` | AppleKeyboard | $C000-$C01F | Uppercase keyboard, strobe |
| `apple2e-keyboard` | AppleIIeKeyboard | $C000-$C01F | Full keyboard with lowercase, OA/CA |
| `apple2-speaker` | AppleSpeaker | $C030 | Speaker click |
| `apple2-softswitches` | AppleSoftSwitchBank | $C050-$C05F | Video toggles |
| `apple2e-softswitches` | AppleIIeSoftSwitchBank | $C050-$C07F | Extended IIe switches |
| `language-card` | LanguageCard | $C080-$C08F | $D000-$FFFF bank switching |
| `aux-ram-card` | AuxRamCard | $C003-$C006 | IIe auxiliary 64K RAM |

### Slot Device Types

| Type | Class | Default Slot | Purpose |
|------|-------|--------------|---------|
| `disk-ii` | DiskIIController | 6 | Disk II floppy controller |

### Video Modes

| Type | Class | Resolution | Description |
|------|-------|------------|-------------|
| `apple2-text40` | AppleTextMode | 40x24 | 40-column text |
| `apple2-text80` | Apple80ColTextMode | 80x24 | 80-column text (IIe) |
| `apple2-lores` | AppleLoResMode | 40x48 | 16-color lo-res |
| `apple2-hires` | AppleHiResMode | 280x192 | NTSC hi-res |
| `apple2-doublehires` | AppleDoubleHiResMode | 560x192 | Double hi-res (IIe) |

### Keyboard Types

| Type | Description |
|------|-------------|
| `apple2-uppercase` | Apple II/II+ uppercase-only |
| `apple2e-full` | Apple //e full keyboard with lowercase |

## Validation Rules

1. **JSON syntax**: Must be valid. Parser reports line/column on error.
2. **Required fields**: Missing required field produces an error naming the field.
3. **CPU type**: Must be `"6502"`.
4. **Address format**: `"0x"` prefix or `"$"` prefix; max value $FFFF.
5. **Size format**: Same as address; range 1..0x10000.
6. **RAM range**: `address + size` must not exceed 64K.
7. **ROM file existence**: All `file` references must resolve through the search paths.
8. **systemRom file size**: ROM must fit within 64K starting at `address`.
9. **Slot range**: `slot` must be 1-7.
10. **Slot ROM size**: Must be exactly 256 bytes.
11. **Slot must have content**: Each slot entry must specify `device` and/or `rom`.
12. **Component registry**: All `type` values must be registered.

## Example: Apple //e

```json
{
    "name": "Apple //e",
    "cpu": "6502",
    "timing": {
        "videoStandard": "ntsc",
        "clockSpeed": 1022727,
        "cyclesPerScanline": 65
    },
    "ram": [
        { "address": "0x0000", "size": "0xC000" },
        { "address": "0x0000", "size": "0xC000", "bank": "aux" }
    ],
    "systemRom": {
        "address": "0xC000",
        "file": "Apple2e.rom"
    },
    "characterRom": {
        "file": "Apple2e_Video.rom"
    },
    "internalDevices": [
        { "type": "apple2e-keyboard" },
        { "type": "apple2-speaker" },
        { "type": "apple2e-softswitches" },
        { "type": "aux-ram-card" },
        { "type": "language-card" }
    ],
    "slots": [
        { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-text80", "apple2-lores", "apple2-hires", "apple2-doublehires"]
    },
    "keyboard": {
        "type": "apple2e-full"
    }
}
```