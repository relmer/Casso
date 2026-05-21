# Contract: Per-Machine User Config (`<MachineName>_user.json`)

**Location**: `<assetBaseDir>/Machines/<MachineName>/<MachineName>_user.json`
**Version field**: `$cassoMachineVersion` (integer; the same field name used
by the default machine JSON — see `theme.schema.md` for the analogous
`$cassoThemeVersion`).
**Owner**: `UserConfigStore` (Casso/Config/).

## Purpose

Persist only the fields the user has explicitly changed from the read-only
default `MachineConfig`. Merged in memory at load time (FR-014, FR-017).

## Schema (v1)

```json
{
    "$cassoMachineVersion": 2,
    "speedMode":            "Authentic" | "Double" | "Maximum",
    "colorMode":            "Color" | "GreenMono" | "AmberMono" | "WhiteMono",
    "writeProtect": {
        "0": false,
        "1": true
    },
    "floppySoundEnabled":   true,
    "floppyMechanism":      "Alps" | "Shugart",
    "componentEnabled": {
        "disk-ii":          true,
        "slot-7":           false
    }
}
```

All fields except `$cassoMachineVersion` are optional. Absent → use default.

## Merge rules (FR-014)

For each present field, the user JSON value wins over the default. For
`componentEnabled`, per-key override (other components' enabled state
inherited from the default `capabilityFlag` interpretation).

## Validation

| Condition | Action |
|-----------|--------|
| Unknown top-level field | logged warning, field ignored |
| Unknown enum value (e.g. `speedMode: "Turbo"`) | logged warning, field ignored, default used |
| Attempt to disable a `required` or `platform-locked` component | merge silently drops the entry |
| Unknown `componentEnabled` key (refers to removed component type) | logged warning, key skipped |
| File missing | no merge, defaults used; no warning |
| File malformed (invalid JSON) | logged warning, treated as missing |

## Upgrade

`MachineConfigUpgrade::Plan` is called when:
- the user file exists, AND
- the user file's `$cassoMachineVersion` (or legacy `$cassoDefault`) is lower
  than the embedded default's version.

The migrator must:
1. Rename `$cassoDefault` → `$cassoMachineVersion` if present in legacy file.
2. Preserve every user-set field that still has meaning in the new schema.
3. Drop fields that no longer apply (logged).
4. Write the migrated content back to the same path before merge proceeds.

## Test scenarios

- Round-trip: write a `MachineUserConfig`, read it back, structural equality.
- Legacy v1 file (with `$cassoDefault`) is silently upgraded to v2.
- Missing file → defaults applied with no warning.
- Unknown component key → warning + skip.
- Required component in `componentEnabled: false` → silently dropped at merge.
- Concurrent read/write through `IUserConfigIo` mock — last-write-wins.
