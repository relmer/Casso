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
| `modelLabel` | text | The image's size in MB, rounded to a whole number, then "SC": "20SC", "32SC". Drawn on the front of the HD20SC model. An empty drive is "20SC". |
| `media` | `BlockDeviceMedia` | Optional; no media means the target does not answer selection. |
| `realTiming` | bool | Off by default (R15). |
| `sense` | key, ASC | Set by a failed command, cleared by REQUEST SENSE. |

SCSI bus phase machine: `BusFree -> Arbitration -> Selection -> Command ->
(DataIn | DataOut) -> Status -> MessageIn -> BusFree`. Selection of an absent
ID times out back to `BusFree`.

### AppleScsiCard

| Field | Rule |
|---|---|
| `rom` | 16 KB. Page 0 at `$Cn00`; a 1 KB bank at `$CC00-$CFFF` from bits 0-3 of the bank latch. |
| `ram` | 8 KB. A 1 KB bank at `$C800-$CBFF` from bits 4-6 of the bank latch. |
| `bankLatch` | Written at `$C0nA`. Cleared at reset. |
| `ncr5380` | Registers at `$C0n0-$C0n7`; pseudo-DMA data at `$C0n8`; DRQ in bit 7 of `$C0nE`; reset by a write to `$C0nB`. |
| `scsiId` | Read at `$C0n9` as a bit mask, `$80` for ID 7. |

### StockHardDiskImage

| Field | Rule |
|---|---|
| `path` | Under the user's Casso data folder. Created on first use, never overwritten. |
| content | An empty ProDOS volume of 65,535 blocks with a blank block 0, as `casso disk create --size 32M` writes. |

### ProFileDrive

Every rule in this section is to verify (research R6). The ProFile's first
task replaces it with the facts from the card ROMs and Apple's documentation.

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

Limit: four SmartPort devices per chain, or the host firmware's limit where
that is lower (research R10). The Machine tab prevents a longer chain.

### DeviceCpu

The 65C02 inside the UniDisk 3.5 and on the 3.5 controller card.

| Field | Rule |
|---|---|
| `cpu` | `Cpu65C02` over a device `MemoryBus`. |
| `clockRatio` | Device clock over host clock: 2.046 MHz for the 3.5 controller card; 2 MHz, to verify, for the UniDisk 3.5. |
| `state` | `Parked` while the device is inactive, `Running` while active. |

### Apple35ControllerCard

| Field | Rule |
|---|---|
| `ram` | 32 KB at the card CPU's `$0000-$7FFF`. |
| `rom` | 32 KB at the card CPU's `$8000-$FFFF`. The Apple has no access to it. |
| card I/O | `$0A00-$0A0F` SWIM; `$0A40` and `$0A41` head select; `$0A80` and `$0A81` diagnostic LED. |
| `bankLatch` | The low 4 address bits of any Apple access to `$C0n0-$C0nF`. |
| Apple windows | `$Cn00-$CnFF` is card RAM `$7B00-$7BFF`, read-only; `$C800-$CBFF` is card RAM at `bankLatch * $400`; `$CC00-$CFFF` is card RAM `$7C00-$7FFF`. |

## Machine

### Apple2cPlus

Derives from `Apple2c`: ROM 05, the internal Apple 3.5 Drive, the external
port chain, and two parts.

`MigChip`, in `$CC00-$CFFF`:

| Field | Rule |
|---|---|
| `ram` | 2 KB. `$CE00-$CE1F` is a 32-byte window at `page`; `$CE20-$CE3F` is the same window, and `page` advances by 32 after the access, wrapping at 2 KB; `$CEA0` resets `page`. |
| `internalDrive` | On by a write to `$CC80-$CC9F`, off by a write to `$CCC0-$CCDF`. |
| `headSelect` | 0 by a read of `$CE40-$CE5F`, 1 by a read of `$CE60-$CE7F`. |
| `select35` | Off by a write to `$CE40-$CE5F`, on by a write to `$CE60-$CE7F`. |
| IWM reset | A write to `$CC40`. |

`Apple2cPlusAccelerator`:

| Field | Rule |
|---|---|
| `unlocked` | `$5A` written four times to `$C05A` unlocks; `$A5` to `$C05A` locks. |
| `fast` | `$08` to `$C05B` selects fast, `$08` to `$C05A` normal. On at reset unless Esc is down. |
| `slowdownMask` | Written at `$C05C`: the slots and the speaker whose accesses run at 1 MHz. The firmware writes `$67`. |
| speed | Casso's setting: "Authentic" is 4 MHz when `fast`, 1 MHz otherwise; "2x" is 8 MHz; "Max" is unthrottled. The Machine tab displays 4 MHz. |

## Configuration

### Slot card and port entries (machine JSON)

See [contracts/machine-config.md](contracts/machine-config.md). A slot entry
lists its card's `device`, `rom`, and its drives as `ports`; a drive entry
lists its `device`, its `image`, and options (`realTiming`, `scsiId`).

Default entries: the //e and enhanced //e have `apple-scsi-card` in slot 7
with an `hd20sc` at ID 6 on the stock image, and `liron-card` in slot 5 with
one `unidisk35`, all enabled. A user copy gains them only where slots 5 and 7
are unconfigured.

## Events

See [contracts/device-events.md](contracts/device-events.md).
`BlockDeviceEvent`: time (cycle), device kind, slot, unit, operation (status,
read, write, format, control, init, spin-up, seek), first block, block count,
result code. `Floppy35Event`: time, drive, operation (motor, step, side, eject,
insert, address mark, data mark read, data mark write), track, side, sector.
