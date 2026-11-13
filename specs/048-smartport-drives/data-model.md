# Data model: SmartPort and hard drive devices

Entities from [spec.md](spec.md), with fields, rules and state machines. Types
are the planned classes.

## Media

### BlockDeviceMedia

An image attached to a block device unit.

| Field | Type | Rule |
|---|---|---|
| `path` | file path | One writer per path across all drives (FR-018). |
| `format` | `Po`, `Hdv`, `TwoImg`, `DiskCopy42`, `Woz35` | Detected from the header, then the extension. |
| `store` | `IBlockStore` | Spec 033's `CachingBlockStore` over `FileBlockStore` or `OffsetBlockStore`. |
| `blockCount` | uint32 | At most 65,535 (32 MB) per ProDOS unit; at most the ProFile model's capacity. |
| `writeProtected` | bool | Host file read-only, 2IMG locked flag, or a write-protect tab on 3.5-inch media. |
| `writeProtectReason` | enum | Used for the tip, in the Disk II's wording. |

Attach fails, leaving the file untouched, when the format is not one the drive
accepts, the size exceeds the drive's capacity, a 2IMG is in DOS order, or a
WOZ image cannot be fully decoded to blocks on a block-only drive (FR-017).

### Floppy35Media

A 3.5-inch disk in a Sony mechanism.

| Field | Type | Rule |
|---|---|---|
| `sides` | 1 or 2 | 400K single-sided, 800K double-sided. |
| `tracks[80][sides]` | bit stream | From a WOZ 2 image, or built by `Gcr35Track` from blocks. |
| `source` | `BlockDeviceMedia` | Built tracks write back as blocks; WOZ tracks write as bits. |
| `writeProtected` | bool | From the WOZ INFO chunk or the host file. |

## Block stack

### ScsiTarget (ScsiHardDisk)

| Field | Type | Rule |
|---|---|---|
| `id` | 0-6 | ID 7 is the card. Unique per card. |
| `model` | `HardDisk20SC`, `GenericScsiDisk` | Both drawn as an HD20SC (spec assumption). |
| `media` | `BlockDeviceMedia` | Optional; no media means the target does not answer selection. |
| `realTiming` | bool | Off by default (R15). |
| `sense` | key, ASC | Set by a failed command, cleared by REQUEST SENSE. |

SCSI bus phase machine: `BusFree -> Arbitration -> Selection -> Command ->
(DataIn | DataOut) -> Status -> MessageIn -> BusFree`. Selection of an absent
ID times out back to `BusFree`.

### ProFileDrive

| Field | Type | Rule |
|---|---|---|
| `model` | `ProFile5MB`, `ProFile10MB` | 9,728 or 19,456 blocks. |
| `media` | `BlockDeviceMedia` | 512-byte blocks; the 20 tag bytes are zeros on read and discarded on write. |
| `realTiming` | bool | Off by default. |

Handshake states: `Idle -> CommandWait ($01) -> CommandReceive (6 bytes) ->
ReadyForData ($02 or $03) -> Transfer (532 bytes) -> Status ($04, 4 bytes) ->
Idle`.

## 3.5-inch stack

### Iwm

| Field | Rule |
|---|---|
| `mode` | Latch, asynchronous, bit cell 2 or 4 microseconds, timer, fast clock. |
| `phases[4]`, `motor`, `driveSelect`, `q6`, `q7` | Set by the `$C0x0-$C0xF` switch accesses. |
| `bus` | `IDiskDriveBus` to the attached drive or the SmartPort chain. |

### Sony35Drive

| Field | Rule |
|---|---|
| `track` (0-79), `side` | Stepped by the step command; side from SEL/HDSEL. |
| `motorOn`, `diskInPlace`, `ejecting` | Eject takes the media out after the eject motor's travel time. |
| `zone` | `track / 16`; sets the rotation speed and sectors per track. |

Status read through CA0-CA2 and SEL returns one of the drive's sense lines.

### SmartPortChain

| Field | Rule |
|---|---|
| `devices[]` | UniDisk 3.5 and Apple 3.5 Drive units in plug order, then optionally one 5.25-inch drive. |
| `ids` | Assigned by the host's INIT; reset clears them. |

Limits per host: the //c ROM 4 and the //c Plus support the chain lengths
their firmware supports; the Machine tab prevents a longer chain.

### DeviceCpu

The 65C02 inside the UniDisk 3.5 and on the 3.5 controller card.

| Field | Rule |
|---|---|
| `cpu` | `Cpu65C02` over a device `MemoryBus`. |
| `clockRatio` | Device clock over host clock (2 MHz over 1.023 MHz for the UniDisk 3.5). |
| `state` | `Parked` while the device is inactive, `Running` while active. |

## Machine

### Apple2cPlus

Derives from `Apple2c`: ROM 05, MIG RAM window, internal Apple 3.5 Drive,
external port chain, and `Accelerator` (`enabled`, `fastClock`, slowdown on
the I/O ranges the real machine slows for).

## Configuration

### Slot card and port entries (machine JSON)

See [contracts/machine-config.md](contracts/machine-config.md). A slot entry
lists its card's `device`, `rom`, and its drives as `ports`; a drive entry
lists its `device`, its `image`, and options (`realTiming`, `scsiId`).

## Events

See [contracts/device-events.md](contracts/device-events.md).
`BlockDeviceEvent`: time (cycle), device kind, slot, unit, operation (status,
read, write, format, control, init, spin-up, seek), first block, block count,
result code. `Floppy35Event`: time, drive, operation (motor, step, side, eject,
insert, address mark, data mark read, data mark write), track, side, sector.
