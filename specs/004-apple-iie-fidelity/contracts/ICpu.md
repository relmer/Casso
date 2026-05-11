# Contract: ICpu

Pluggable CPU strategy for the emulator engine. The interface is intentionally
**CPU-family agnostic** — nothing in `ICpu` assumes a 6502-shaped register
file, vector address, or interrupt model. Implementations are free to model
their CPU's reset, interrupt, and execution semantics however the real
hardware does.

Implementations:
- `Cpu6502` (this feature; existing Cpu logic re-homed)
- `Cpu65C02` (out of scope; future — drops in via the same interface)
- `Cpu65C816`, Z80, others (out of scope; drop in via the same interface)

## Header (canonical declarations)

```cpp
// CPU-family-agnostic interrupt source identifier.
// Implementations map these to whatever interrupt structure the real CPU
// has. For 6502-family: kNonMaskable -> NMI, kMaskable -> IRQ. For CPUs
// with multiple maskable levels, additional kMaskable* identifiers may be
// added later without breaking existing implementations.
enum class CpuInterruptKind : uint8_t {
    kNonMaskable,
    kMaskable,
};

class ICpu {
public:
    virtual         ~ICpu () = default;

    // Cold reset. Implementation defines what state is initialized vs
    // preserved (e.g. 6502 sets I=1 and loads PC from $FFFC, leaves
    // A/X/Y/SP per real hardware).
    virtual HRESULT  Reset () = 0;

    // Execute exactly one instruction (including any pending interrupt
    // dispatch checked at the appropriate boundary for this CPU).
    // Reports cycle count consumed via out-param.
    virtual HRESULT  Step (uint32_t & outCycles) = 0;

    // Assert/deassert an interrupt line. Implementation defines whether
    // the line is level-sensitive or edge-triggered per the real CPU.
    virtual void     SetInterruptLine (CpuInterruptKind kind, bool asserted) = 0;

    // Total cycle count since last Reset(). Generic across all CPUs.
    virtual uint64_t GetCycleCount () const = 0;
};
```

`CpuRegisters` and any family-specific register-file inspection live OUTSIDE
`ICpu`. See `ICpuDebugInfo` and the family-specific debug interfaces
(`I6502DebugInfo`, future `I65816DebugInfo`, etc.) for register access by
debuggers and tests. CPUs that want to expose registers implement the
appropriate family debug interface in addition to `ICpu`; consumers query
for it via `dynamic_cast` or a feature-query method on a parent host
interface.

## Semantics

- `Reset()` — implementation-defined. Documented per implementation. For
  `Cpu6502`: sets I=1, loads PC from $FFFC/$FFFD via the connected
  MemoryBus, leaves SP/A/X/Y unchanged (real //e ROM handler reinitializes SP).
- `Step()` — executes exactly one instruction, including any
  interrupt-dispatch check at the appropriate boundary. Reports cycle count.
- `SetInterruptLine(kNonMaskable, true)` — for `Cpu6502`, edge-triggered (false→true) NMI.
- `SetInterruptLine(kMaskable, true)`    — for `Cpu6502`, level-sensitive IRQ; dispatched
  when `(P & I) == 0`.
- The `kNonMaskable`/`kMaskable` mapping is a CPU implementation detail; callers
  do not need to know the exact electrical model.

## Test obligations

- Existing `CpuOperationTests`, `Dormann`, `Harte` MUST continue to pass
  against `Cpu6502` (byte-exact behavior preservation).
- `InterruptControllerTests` exercises IRQ dispatch via `MockIrqAsserter`
  using `SetInterruptLine(kMaskable, ...)`.
- Register-file tests use `I6502DebugInfo` (defined in a separate contract),
  not `ICpu`.
