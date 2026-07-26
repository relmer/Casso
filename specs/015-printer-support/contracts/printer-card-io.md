# Contract: Parallel Printer Card — Guest-Visible Interface

**Consumers**: guest software (Print Shop drivers, BASIC/DOS via firmware,
Pascal-protocol callers). This contract is what 1980s software observes; it
must stay stable once shipped.

## I/O window (`$C080 + slot*$10`; slot 1 → `$C090-$C09F`)

| Offset | Access | Behavior |
|---|---|---|
| +$0 | write | Data latch: byte accepted, delivered in order (FR-002/FR-004) |
| +$0 | read | Status (alias — some drivers read the latch address) |
| +$1 | read | Status: ready while the ring has headroom, busy at high water |
| +$1..+$F | write | Ignored (no side effects) |
| +$2..+$F | read | Status alias (tolerant decode, R-001; Print Shop's Apple II Parallel driver polls +$4) |

Status byte — LOCKED from the live Print Shop capture (R-001/T011):
ready = `$83`, busy = `$00`. Bit 7 set = ready (our firmware and the Apple II
Parallel driver poll it); low three bits = `011` (Centronics SELECT/FAULT#
high, PAPER-OUT low — Print Shop's Grappler+ driver refuses to send anything
until `(status & $07) == $03`). No further state machine is observable from
the guest.

## Slot ROM (`$Cn00-$CnFF`, installed via CxxxRomRouter)

Original firmware (FR-003), assembled from `ParallelFirmware.a65`:

- `$Cn00` — BASIC/DOS entry: installs output hook (`PR#n`), prints via CSW.
- Output routine: preserves A/X/Y per Apple II slot firmware conventions;
  writes char to `+$0`; no busy-wait needed (always ready) but a compatible
  status check is included for authenticity.
- Pascal 1.1 protocol: signature bytes `$Cn05=$38, $Cn07=$18, $Cn0B=$01`,
  `$Cn0C` = generic-printer device class byte; entry-point table for
  init/read/write/status with read returning "not supported" per protocol.
- No expansion ROM (`$C800` space unused; INTC8ROM interactions untouched).

## Invariants

- With the card disabled in machine config, the slot presents exactly today's
  empty-slot behavior (floating bus reads, no ROM) — FR-001.
- Bytes are never dropped, reordered, or duplicated regardless of guest write
  rate — FR-002/FR-004.
- Guest reset / `PowerCycle` do not clear the paper strip (FR-026 semantics);
  they do reset interpreter state (FR-010).
