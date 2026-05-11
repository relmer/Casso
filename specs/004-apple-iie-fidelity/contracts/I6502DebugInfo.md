# Contract: I6502DebugInfo

Family-specific register-file inspection for 6502-compatible CPUs. Used by
debuggers and tests that need to inspect or assert against CPU register
state. NOT part of `ICpu` — `ICpu` is CPU-family agnostic.

A 6502-family CPU implementation (`Cpu6502`, future `Cpu65C02`,
`Cpu65C816` if its 6502-emulation-mode register set is sufficient)
implements both `ICpu` AND `I6502DebugInfo`. Future CPU families will
have their own debug interfaces (`I65816DebugInfo`, `IZ80DebugInfo`, etc.)
modeled after this one.

Consumers obtain `I6502DebugInfo` by `dynamic_cast`-ing an `ICpu*` they
already have. If the cast fails, the CPU does not expose 6502-shaped
register state — consumers must fall back to a generic path (e.g. don't
display registers, or display "unsupported").

## Header (canonical declarations)

```cpp
struct Cpu6502Registers {
    uint16_t pc;
    uint8_t  a;
    uint8_t  x;
    uint8_t  y;
    uint8_t  sp;
    uint8_t  p;
};

class I6502DebugInfo {
public:
    virtual                     ~I6502DebugInfo () = default;
    virtual Cpu6502Registers     GetRegisters () const = 0;
    virtual void                 SetRegisters (const Cpu6502Registers & regs) = 0;
};
```

## Semantics

- `GetRegisters()` — returns a snapshot of the CPU's 6502-shaped register
  file. Snapshot is point-in-time; not live-tracking.
- `SetRegisters()` — writes all six register fields. Used by debugger and
  by tests that need to seed CPU state before stepping.

## Test obligations

- `Cpu6502Tests` validates that `Cpu6502` implements both `ICpu` and
  `I6502DebugInfo`, and that the debug-info round-trip (`SetRegisters`
  then `GetRegisters`) is byte-identical.
- `IntegrationTests` may downcast `ICpu*` to `I6502DebugInfo*` to seed
  register state; tests for non-6502 CPUs (when those exist) must NOT
  assume the cast succeeds.
