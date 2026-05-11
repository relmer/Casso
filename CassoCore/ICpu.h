#pragma once

#include "Pch.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuInterruptKind
//
//  CPU-family-agnostic interrupt source identifier. Implementations map
//  these to whatever interrupt structure the real CPU has. For 6502-family:
//  kNonMaskable -> NMI, kMaskable -> IRQ. For CPUs with multiple maskable
//  levels, additional kMaskable* identifiers may be added later without
//  breaking existing implementations.
//
////////////////////////////////////////////////////////////////////////////////

enum class CpuInterruptKind : uint8_t
{
    kNonMaskable,
    kMaskable,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICpu
//
//  Pluggable CPU strategy for the emulator engine. CPU-family agnostic —
//  nothing in ICpu assumes a 6502-shaped register file, vector address, or
//  interrupt model. Implementations are free to model their CPU's reset,
//  interrupt, and execution semantics however the real hardware does.
//
//  Implementations:
//    - Cpu6502 (this feature; existing Cpu logic re-homed in F1)
//    - future: Cpu65C02, Cpu65C816, Z80
//
//  Family-specific register-file access lives outside ICpu — see
//  I6502DebugInfo and the future per-family debug interfaces.
//
////////////////////////////////////////////////////////////////////////////////

class ICpu
{
public:
    virtual              ~ICpu () = default;

    // Cold reset. Implementation-defined: e.g. 6502 sets I=1 and loads PC
    // from $FFFC, leaving SP/A/X/Y per real hardware.
    virtual HRESULT       Reset () = 0;

    // Execute exactly one instruction (including any pending interrupt
    // dispatch checked at the appropriate boundary for this CPU). Reports
    // cycle count consumed via out-param.
    virtual HRESULT       Step (uint32_t & outCycles) = 0;

    // Assert/deassert an interrupt line. Implementation defines whether
    // the line is level-sensitive or edge-triggered per the real CPU.
    // For Cpu6502: kMaskable is level-sensitive IRQ; kNonMaskable is
    // edge-triggered NMI.
    virtual void          SetInterruptLine (CpuInterruptKind kind, bool asserted) = 0;

    // Total cycle count since last Reset(). Generic across all CPUs.
    virtual uint64_t      GetCycleCount () const = 0;
};
