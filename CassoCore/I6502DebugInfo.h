#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502Registers
//
//  Snapshot of the 6502-family register file. Used by debuggers and tests
//  that need to inspect or seed CPU state. Not part of ICpu — ICpu is
//  CPU-family agnostic.
//
////////////////////////////////////////////////////////////////////////////////

struct Cpu6502Registers
{
    uint16_t    pc;
    uint8_t     a;
    uint8_t     x;
    uint8_t     y;
    uint8_t     sp;
    uint8_t     p;
};





////////////////////////////////////////////////////////////////////////////////
//
//  I6502DebugInfo
//
//  Family-specific register-file inspection for 6502-compatible CPUs.
//  A 6502-family CPU implementation (Cpu6502, future Cpu65C02) implements
//  both ICpu and I6502DebugInfo. Future families (65816, Z80) get their
//  own debug interfaces.
//
//  Consumers obtain I6502DebugInfo by dynamic_cast-ing an ICpu* they
//  already have. If the cast fails, the CPU does not expose 6502-shaped
//  register state.
//
////////////////////////////////////////////////////////////////////////////////

class I6502DebugInfo
{
public:
    virtual                     ~I6502DebugInfo () = default;

    virtual Cpu6502Registers     GetRegisters () const = 0;
    virtual void                 SetRegisters (const Cpu6502Registers & regs) = 0;
};
