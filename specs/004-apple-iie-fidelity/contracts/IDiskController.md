# Contract: IDiskController (DiskIIController, rewritten)

Nibble-level Disk II controller per slot.

## Header (selected)

```cpp
class DiskIIController {
public:
    HRESULT  Initialize (int slot, IDiskImageStore * store);
    HRESULT  MountDrive (int drive, IDiskImage * image);
    HRESULT  EjectDrive (int drive);

    // Soft-switch surface at $C0E0+slot*16 .. $C0EF+slot*16
    uint8_t  ReadSoftSwitch  (uint8_t addrLow);
    void     WriteSoftSwitch (uint8_t addrLow, uint8_t value);

    // ROM at $C600 + slot*$100 (one page)
    void     MapRom (RomDevice * slotRom);

    // Cycle-driven advance from EmuCpu
    void     Tick (uint32_t cpuCycles);
};
```

## Soft-switch table (per slot 6: $C0E0-$C0EF)

| Addr | Effect |
|---|---|
| $C0E0 | motor off (delayed ~1s in real HW; here: immediate or scheduled) |
| $C0E1 | motor on |
| $C0E2 | drive 1 select |
| $C0E3 | drive 2 select |
| $C0E4..$C0E7 | phase 0..1 off / on (head step) |
| $C0E8..$C0EF | Q6 / Q7 latches per Sather table 9.1 |

Reads return the current data latch when in read mode (Q6=0, Q7=0); writes
to the appropriate switch enter write mode and stream bits via
`DiskIINibbleEngine::WriteLatch`.

## Test obligations

`DiskIITests` (rewrite):
- Motor on/off observable via $C0E9 status.
- Drive select: writes go to selected drive's image only.
- Phase stepping moves head by 1 half-track per phase pulse.
- Q6/Q7 transitions transition LSS state correctly.
- Read latch yields the next nibble of the current track bit stream.
- Write latch pushes bits into the track bit stream and marks track dirty.

`WozLoaderTests`:
- WOZ v1 parses INFO/TMAP/TRKS chunks correctly.
- WOZ v2 with quarter-track resolution maps correctly.

`NibblizationTests`:
- DOS 3.3 .dsk → nibbles → boots in the controller.
- ProDOS .po → nibbles → boots.
- Round-trip: nibblize then de-nibblize equals original bytes.

`DiskImageStoreTests`:
- Eject flushes dirty image.
- Machine-switch flushes all mounted dirty images.
- Exit flushes all.
- No flush of non-dirty images.
