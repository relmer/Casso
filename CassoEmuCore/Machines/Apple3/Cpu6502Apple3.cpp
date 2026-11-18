#include "Pch.h"

#include "Cpu6502Apple3.h"
#include "IExtendedAddressing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502Apple3
//
////////////////////////////////////////////////////////////////////////////////

Cpu6502Apple3::Cpu6502Apple3 (MemoryBus & memoryBus, IExtendedAddressing & extended)
    : MemoryBusCpu (memoryBus),
      m_extended   (extended)
{
    FlagIndirectInstructions();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagIndirectInstructions
//
//  Marks every executable instruction with a (zp,X) or (zp),Y operand,
//  undocumented ones included: the extended addressing is in the hardware
//  around the CPU and applies to any data access through a zero page
//  pointer. JMP ($nnnn) reads its pointer with an absolute address and is
//  not one of them.
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502Apple3::FlagIndirectInstructions()
{
    for (Microcode & microcode : instructionSet)
    {
        bool  isIndirect = microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageXIndirect
                        || microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageIndirectY;



        microcode.isExtendedIndirect = microcode.isLegal && isIndirect;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveExtendedIndirect
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502Apple3::ResolveExtendedIndirect (Byte pointerHigh, Word effectiveAddress)
{
    m_isExtendedAccess = m_extended.TryBeginExtendedAccess (pointerHigh, effectiveAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndExtendedAccess
//
////////////////////////////////////////////////////////////////////////////////

void Cpu6502Apple3::EndExtendedAccess()
{
    m_extended.EndExtendedAccess();
}
