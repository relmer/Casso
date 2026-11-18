# 048 handoff notes

## For spec 035: the block device and 3.5-inch event feeds

Spec 048 adds two event feeds for 035's debugger to log, as it logs
`Disk2Event` (FR-022, research R14):

- `IBlockDeviceEventSink` (`CassoEmuCore/Devices/BlockDevice/IBlockDeviceEventSink.h`)
  receives a `BlockDeviceEvent` (`BlockDeviceEvent.h`) for each completed hard
  disk, ProFile or SmartPort operation: the cycle, the device kind, slot and
  unit, the operation, the first block and count, and the result code.
  `BlockDeviceEventRing` is its ring buffer.
- `IFloppy35EventSink` (`CassoEmuCore/Devices/Floppy35/IFloppy35EventSink.h`)
  receives a `Floppy35Event` (`Floppy35Event.h`) for each 3.5-inch mechanism
  operation: motor, step, side, eject, insert, address mark, data read and
  write, with the track, side and sector.

The machine's sources are `MachineRefs::blockDeviceSources` and
`MachineRefs::floppy35Sources`. `MachineHost::AttachObservers` sets each
source's sink from `MachineObservers::blockDevice` and
`MachineObservers::floppy35`.

Each source has one sink. `MachineBuilder::WireDeviceAudio` points the sinks at
the drives' audio sources, so a debugger that attaches through
`AttachObservers` replaces the audio sources' feed. Before 035 attaches its
log, add a fan-out sink that passes each event to both.
