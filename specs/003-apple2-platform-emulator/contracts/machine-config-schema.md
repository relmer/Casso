# Machine Configuration JSON Schema

**Feature**: `003-apple2-platform-emulator`
**Date**: 2025-07-22

## File Location

Machine configs are stored in `Casso65Emu/machines/` as `.json` files. The `--machine <name>` CLI argument resolves to `machines/<name>.json`.

## Schema

```json
{
    "name": "<string>",
    "cpu": "<string>",
    "clockSpeed": <integer>,
    "memory": [
        {
            "type": "<string>",
            "start": "<hex-string>",
            "end": "<hex-string>",
            "file": "<string>",
            "bank": "<string>",
            "target": "<string>"
        }
    ],
    "devices": [
        {
            "type": "<string>",
            "address": "<hex-string>",
            "start": "<hex-string>",
            "end": "<hex-string>",
            "slot": <integer>
        }
    ],
    "video": {
        "modes": ["<string>"],
        "width": <integer>,
        "height": <integer>
    },
    "keyboard": {
        "type": "<string>"
    }
}
```

## Field Reference

### Root Object

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `name` | string | Yes | Non-empty | Human-readable machine name (displayed in title bar) |
| `cpu` | string | Yes | `"6502"` or `"65c02"` | CPU variant |
| `clockSpeed` | integer | Yes | > 0 | CPU clock speed in Hz |
| `memory` | array | Yes | ≥ 1 entry | RAM and ROM region definitions |
| `devices` | array | Yes | May be empty | Named device instances wired to the address bus |
| `video` | object | Yes | — | Video output configuration |
| `keyboard` | object | Yes | — | Keyboard input configuration |

### memory[] Entry

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `type` | string | Yes | `"ram"` or `"rom"` | Memory region type |
| `start` | string | Yes | Hex address `"0xNNNN"` | Start address (inclusive) |
| `end` | string | Yes | Hex ≥ start | End address (inclusive) |
| `file` | string | rom only | Relative to `roms/` dir | ROM image filename |
| `bank` | string | No | `"aux"` | Bank identifier (IIe auxiliary RAM) |
| `target` | string | No | `"chargen"` | Target subsystem (character generator ROM) |

### devices[] Entry

Exactly one of: `address`, `start`/`end` pair, or `slot` must be provided.

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `type` | string | Yes | Registered in ComponentRegistry | Device type name |
| `address` | string | No | Hex address | Single-address device mapping |
| `start` | string | No | Hex address | Range start (used with `end`) |
| `end` | string | No | Hex ≥ start | Range end (used with `start`) |
| `slot` | integer | No | 1–7 | Slot number (auto-maps I/O + ROM ranges) |

**Slot auto-mapping**: When `slot` is specified, the device is mapped to:
- I/O: `$C080 + slot×16` through `$C08F + slot×16`
- ROM: `$Cs00` through `$CsFF` (where s = slot number)

### video Object

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `modes` | array\<string\> | Yes | ≥ 1, all registered | Video mode component names |
| `width` | integer | Yes | > 0 | Framebuffer width in pixels |
| `height` | integer | Yes | > 0 | Framebuffer height in pixels |

### keyboard Object

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `type` | string | Yes | Registered in ComponentRegistry | Keyboard component name |

## Registered Component Types

### Device Types

| Type String | C++ Class | Description |
|-------------|-----------|-------------|
| `apple2-keyboard` | AppleKeyboard | Uppercase-only keyboard, strobe at $C000/$C010 |
| `apple2e-keyboard` | AppleIIeKeyboard | Full keyboard with lowercase, modifier keys |
| `apple2-speaker` | AppleSpeaker | Speaker toggle on $C030 read |
| `apple2-softswitches` | AppleSoftSwitchBank | Video mode toggles $C050–$C057 |
| `apple2e-softswitches` | AppleIIeSoftSwitchBank | Extended IIe soft switches |
| `language-card` | LanguageCard | $D000–$FFFF bank switching at $C080–$C08F |
| `aux-ram-card` | AuxRamCard | IIe auxiliary 64KB RAM at $C003/$C005 |
| `disk-ii` | DiskIIController | Disk II controller (requires `slot` field) |

### Video Mode Types

| Type String | C++ Class | Description |
|-------------|-----------|-------------|
| `apple2-text40` | AppleTextMode | 40×24 text, interleaved row layout |
| `apple2-text80` | Apple80ColTextMode | 80×24 text (IIe, main+aux interleaved) |
| `apple2-lores` | AppleLoResMode | 40×48, 16-color lo-res graphics |
| `apple2-hires` | AppleHiResMode | 280×192 hi-res with NTSC color artifacts |
| `apple2-doublehires` | AppleDoubleHiResMode | 560×192 double hi-res (IIe, 16 colors) |

### Keyboard Types

| Type String | Description |
|-------------|-------------|
| `apple2-uppercase` | Apple II/II+ uppercase-only keyboard |
| `apple2e-full` | Apple IIe full keyboard with lowercase |

## Validation Rules

1. **JSON syntax**: Must be valid JSON. Parser reports line/column on error.
2. **Required fields**: All required fields must be present. Missing field → error naming the field.
3. **CPU type**: Must be `"6502"` or `"65c02"`. Unknown value → error.
4. **Address format**: Must be `"0x"` prefixed hex string. Invalid format → error.
5. **Address ordering**: `end` must be ≥ `start` in all ranges. Violation → error.
6. **ROM file existence**: All `memory[].file` paths must resolve to existing files. Missing → error with expected path.
7. **Component registry**: All `devices[].type`, `video.modes[]`, and `keyboard.type` values must be registered. Unknown → error listing registered types.
8. **Address overlap**: No two devices/memory regions may claim the same address. Overlap → error naming both devices and the conflicting range.
9. **Slot range**: `slot` must be 1–7. Out of range → error.
10. **Device mapping**: Each device entry must have exactly one of: `address`, `start`+`end`, or `slot`. Multiple or none → error.
