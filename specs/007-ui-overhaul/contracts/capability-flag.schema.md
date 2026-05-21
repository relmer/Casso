# Contract: `capabilityFlag` Schema Extension

**Schema location**: `internalDevices[*].capabilityFlag` and
`slots[*].capabilityFlag` in each `Resources/Machines/<Name>/<Name>.json`.
**Affected requirements**: FR-005, FR-006, FR-007, FR-008, FR-015.
**Migration version**: bumps machine schema to `$cassoMachineVersion: 2`.

## Values

| Value | Render | Interactive | Tooltip | Default for... |
|-------|--------|-------------|---------|----------------|
| `optional` | unchecked-or-checked per `componentEnabled` | yes | no | absent on `slots[*]` |
| `required` | checked, grayed-out | no | no (FR-007) | absent on `internalDevices[*]` |
| `platform-locked` | checked, grayed-out | no | yes (from `lockReason`) | never default — must be explicit |

## Schema additions

Per component entry (both `internalDevices` and `slots`):

```json
{
    "type": "apple2e-mmu",                 // existing
    "displayName": "MMU (Apple //e)",      // NEW — shown in hardware tree
    "capabilityFlag": "required",          // NEW — one of optional/required/platform-locked
    "lockReason": "Built into the //e motherboard"   // NEW — required iff capabilityFlag = platform-locked
}
```

`displayName` defaults to a humanized form of `type` if absent.
`lockReason` is ignored for non-platform-locked components.

## Upgrade path (v1 → v2)

Migrator behavior:

1. Read existing machine JSON.
2. Rename `$cassoDefault` → `$cassoMachineVersion`.
3. For each `internalDevices[*]` lacking `capabilityFlag` → inject
   `"capabilityFlag": "required"`.
4. For each `slots[*]` lacking `capabilityFlag` → inject
   `"capabilityFlag": "optional"`.
5. Inject `displayName` from a name-table for known component types; otherwise
   humanize the type.
6. For known platform-locked devices on Apple //c (when that machine is added
   later), inject `"capabilityFlag": "platform-locked"` + `lockReason`. (Not
   needed for the three machines that ship in this feature.)
7. Bump `$cassoMachineVersion` to 2.
8. Write back.

## Tree rendering contract (HardwareTreeView)

For each component:

```
flag == required        -> draw checked, disabled, no tooltip
flag == platform-locked -> draw checked, disabled, tooltip = lockReason
flag == optional        -> draw user-controllable checkbox; initial state from
                           merged MachineConfig (default = enabled unless the
                           user JSON's componentEnabled.<key> = false)
unknown flag            -> treat as required (safe default), log warning
```

Slot key for `componentEnabled` map: `"slot-<N>"` (e.g. `"slot-6"`).
Internal device key for `componentEnabled` map: the device `type` string
(e.g. `"disk-ii"`, but disk-ii is in a slot, so realistically internal devices
keys would be `"language-card"`, etc.).

## Test scenarios

- Default machine JSON loads → all internal devices flagged required, all
  slots flagged optional.
- v1 machine JSON (no `capabilityFlag` fields, `$cassoDefault` only) → upgrade
  produces a v2 file with correct defaults, written back to disk (mocked IO).
- User attempts to disable a required component → merge silently drops the
  entry; tree still draws unchecked-disabled state? No — required is always
  drawn checked-disabled regardless of `componentEnabled` value (the flag wins).
- Platform-locked without `lockReason` → schema validation error, machine
  rejected, fall back to embedded default.
