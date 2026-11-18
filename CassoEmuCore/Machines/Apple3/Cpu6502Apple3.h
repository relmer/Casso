#pragma once

#include "Pch.h"

#include "Core/MemoryBusCpu.h"

class IExtendedAddressing;
class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502Apple3
//
//  The Apple ///'s 6502B: the NMOS core with its indirect instructions
//  flagged so their data access goes through the memory map's extended
//  addressing. The hardware latches the extension byte on any zero page
//  read, so an absolute instruction running in zero page is extended by its
//  operand's high byte too. The other instructions are the NMOS core's.
//
////////////////////////////////////////////////////////////////////////////////

class Cpu6502Apple3 : public MemoryBusCpu
{
public:
    Cpu6502Apple3 (MemoryBus & memoryBus, IExtendedAddressing & extended);

protected:
    void  ResolveExtendedIndirect (Byte pointerHigh, Word effectiveAddress) override;
    void  EndExtendedAccess       () override;

private:
    void  FlagIndirectInstructions ();

    IExtendedAddressing  & m_extended;
};
