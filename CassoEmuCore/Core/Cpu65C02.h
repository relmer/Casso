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
    void        InstallCmosOpcodes  ();
    void        InstallArithmetic   ();
    void        InstallZeroPageInd  ();
    void        InstallNewOps       ();
    void        InstallBitOps       ();
    void        InstallNops         ();

    void        SetOpcode (Byte                                  opcode,
                           const char                          * name,
                           Microcode::Operation                  operation,
                           GlobalAddressingMode::AddressingMode  mode,
                           Byte                                * pSourceRegister,
                           Byte                                * pDestinationRegister,
                           Byte                                  cycles);
};
