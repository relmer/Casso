#pragma once

#include "Pch.h"

#include "MemoryBusCpu.h"

class MemoryBus;




////////////////////////////////////////////////////////////////////////////////
//
//  Cpu65C02
//
//  CMOS 65C02 core. Derives from MemoryBusCpu (so it inherits the bus-routed
//  memory access, DRAM power-on init, and split-reset semantics unchanged) and
//  expresses the CMOS delta entirely as data: the constructor rewrites the
//  affected entries of the shared instruction table to point at the 65C02
//  operations / addressing modes. The NMOS dispatch is never altered.
//
//  The only non-table-driven difference — the decimal flag being cleared on a
//  hardware IRQ/NMI — is handled by overriding DispatchVector.
//
////////////////////////////////////////////////////////////////////////////////

class Cpu65C02 : public MemoryBusCpu
{
public:
    explicit    Cpu65C02 (MemoryBus & memoryBus);

protected:
    void        DispatchVector (Word vector, bool fromBrk) override;

private:
    // These run after the base ctor has already built the NMOS table, so they
    // override existing entries rather than initialize from scratch; the
    // Initialize* naming matches the base table-builder family (InitializeMisc,
    // InitializeGroup00...).
    void        InitializeCmos          ();
    void        InitializeArithmetic    ();
    void        InitializeCmosLeftovers ();
    void        InitializeNops          ();

    void        SetOpcode (Byte                                  opcode,
                           const char                          * name,
                           Microcode::Operation                  operation,
                           GlobalAddressingMode::AddressingMode  mode,
                           Byte                                * pSourceRegister,
                           Byte                                * pDestinationRegister,
                           Byte                                  cycles);
};
