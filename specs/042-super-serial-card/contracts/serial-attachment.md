# Contract: serial connector attachments

The seam spec 033 builds its Casso Explorer, TCP and COM-port connections on. It applies to every emulated 6551: the SSC's and the //c's two ports.

## IAciaEndpoint (implemented by an attachment)

Called on the emulation thread.

| Method | When |
|---|---|
| `OnByteTransmitted (Byte value)` | a character has finished shifting out, one character time after it entered the shift register |
| `OnModemControlChanged (bool dtr, bool rts)` | DTR or RTS changed; also called once when the attachment is set, with the current states. Default: no-op |
| `OnBreakChanged (bool active)` | the transmitter started or stopped sending break. Default: no-op |

An attachment that forwards to another thread (a socket, a COM port) copies the byte and returns; it never blocks the emulation thread.

## Acia6551 (called by an attachment)

| Method | Thread | Effect |
|---|---|---|
| `EnqueueReceivedBytes (const Byte * bytes, size_t count)` | any | appends to the receive queue; the 6551 delivers them one receiver character time apart (R-008) |
| `SetModemInputs (bool cts, bool dsr, bool dcd)` | any | takes effect on the emulation thread at the next tick |
| `GetQueuedReceiveCount()` | any | bytes queued and not yet delivered, for an attachment that applies its own flow control |
| `SetEndpoint (IAciaEndpoint * endpoint)` | emulation | attaches or detaches (nullptr); detaching empties the receive queue and restores CTS, DSR and DCD to true |

## Guarantees

- Bytes land in the receive data register in queue order, never closer together than one character time at the format in the control register when each lands.
- The receive queue has no fixed limit; an attachment that applies backpressure uses `GetQueuedReceiveCount` or the guest's RTS from `OnModemControlChanged`.
- Pausing the machine pauses delivery; nothing is dropped.
- Ctrl-Reset does not empty the queue (the RESB transition in R-008), and it resends `OnModemControlChanged` with DTR and RTS off.

## Attachments in this feature

| `ports[0].device` | Class | Behavior |
|---|---|---|
| `""` | none | transmitted bytes are discarded; nothing is received; CTS, DSR, DCD true |
| `serial-loopback` | `AciaLoopbackEndpoint` | each transmitted byte is enqueued back; RTS drives CTS and DTR drives DSR and DCD, as a loopback plug wires them |
| `serial-file-out` | `AciaFileEndpoint` | transmitted bytes are appended to the file, opened when the machine is built and closed when it is torn down or the attachment changes |
| `serial-file-in` | `AciaByteSourceEndpoint` | the file's bytes, read when the machine is built, are enqueued at once and arrive one character time apart; transmitted bytes are discarded |
