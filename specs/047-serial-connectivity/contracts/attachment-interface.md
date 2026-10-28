# Contract: 6551 attachment interface

## IAciaEndpoint additions

```cpp
// The format the guest set; called on attach and whenever it changes.
virtual void OnFormatChanged (const SerialLineFormat & format) {}

// The chip finished attaching the endpoint; a host link sets its starting
// lines and opens its connection here.
virtual void OnAttached () {}

// An attachment that brings input from outside the machine, polled by the
// chip on the emulation thread while live.
virtual bool IsHostLink () const { return false; }

// Emulation thread, live only. `cycles` is the chip's running count of
// emulated cycles; returns true when anything moved; the chip then polls
// at the short interval.
virtual bool Poll (uint64_t cycles) { return false; }

// The machine is live again at a point in the past: close any connection,
// and set the chip's lines and queue directly to their state after a dropped line.
virtual void DropConnection () {}
```

A host link applies the chip's format, DTR, RTS and break to the PC on its
next `Poll`, reading them from the chip at that moment, so a replay, which is
never polled, changes nothing on the PC, and after a seek that loads older
registers the PC matches the chip once live.
`OnByteTransmitted` is not called on a host link while the chip is muted.

## Acia6551 host-input methods (emulation thread)

| Method | Journals | Effect |
|---|---|---|
| `ReceiveFromHost (Byte value, Byte errors)` | `AciaByte` | queued for the receiver, paced at the character time |
| `SetHostLines (bool cts, bool dsr, bool dcd)` | `AciaLines`, when they differ from the chip's | applied in this tick |
| `NoteHostConnection (bool isOpen, std::string_view peer)` | `AciaConnection` | a serial log entry |
| `ApplyHostLinesNow` / `ReceiveFromHostNow` | none | direct, for the build and for `DropConnection` |
| `GetElapsedCycles ()` | | emulated cycles since the chip was built |
| `GetLineFormat ()` | | the current `SerialLineFormat` |
| `GetQueuedReceiveCount ()` | | for backpressure |

Entries are stamped `journal.GetCycle() - cycles of this Tick`, as
`TakeHostBytes` stamps them.

## Serial log

The error bits of an `IoEventKind::SerialReceived` entry are in `IoEvent::reg`.
`IoEventKind::SerialConnection` has `value` 1 or 0 and the peer text as its
bytes.
