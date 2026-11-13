# Contract: device event feeds

Two feeds, built as `IDisk2EventSink` and `Disk2EventRing` are: a sink
interface the device calls on the emulation thread, a lock-free single-producer
ring, and attachment through `MachineHost::AttachObservers`. Consumers are the
drive lamps, the audio sources, and the debugger's log.

## IBlockDeviceEventSink

```cpp
struct BlockDeviceEvent
{
    uint64_t           cycle;
    BlockDeviceKind    kind;        // Scsi, ProFile, SmartPort
    Byte               slot;
    Byte               unit;
    BlockDeviceOp      op;          // Status, Read, Write, Format, Control, Init, SpinUp, Seek
    uint32_t           firstBlock;
    uint32_t           blockCount;
    Byte               result;      // ProDOS or SmartPort error code, 0 for success
};

class IBlockDeviceEventSink
{
public:
    virtual ~IBlockDeviceEventSink() = default;
    virtual void OnBlockDeviceEvent (const BlockDeviceEvent & event) = 0;
};
```

## IFloppy35EventSink

```cpp
struct Floppy35Event
{
    uint64_t        cycle;
    Byte            drive;
    Floppy35Op      op;       // Motor, Step, Side, Eject, Insert, AddressMark, DataRead, DataWrite
    Byte            track;
    Byte            side;
    Byte            sector;
};
```

## Rules

- The device emits an event for each completed operation, not each byte.
- A failed operation emits an event with its error code; a degraded operation
  never reports success.
- Emission costs nothing when no sink is attached.
