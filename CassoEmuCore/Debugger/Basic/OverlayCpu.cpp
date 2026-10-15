#include "Pch.h"

#include "Debugger/Basic/OverlayCpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu::TryCall
//
//  The stack has a return address one short of the sentinel, as JSR
//  pushes it, so the routine's RTS lands on the sentinel.
//
////////////////////////////////////////////////////////////////////////////////

bool OverlayCpu::TryCall (Word address, uint32_t maxSteps)
{
    static constexpr Byte  kStackTop = 0xFF;
    Cpu6502Registers       registers = {};
    Word                   back      = (Word) (kSentinel - 1);
    uint32_t               cycles    = 0;
    uint32_t               steps     = 0;



    Poke (0x01FF, (Byte) (back >> 8));
    Poke (0x01FE, (Byte) (back & 0xFF));

    registers.pc = address;
    registers.sp = (Byte) (kStackTop - 2);
    registers.p  = 0x34;
    SetRegisters (registers);

    while (GetRegisters().pc != kSentinel && steps < maxSteps)
    {
        Step (cycles);
        steps++;
    }

    return GetRegisters().pc == kSentinel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu::ReadByteSlow
//
////////////////////////////////////////////////////////////////////////////////

Byte OverlayCpu::ReadByteSlow (Word address)
{
    auto  found = m_overlay.find (address);



    return (found != m_overlay.end()) ? found->second : m_peek (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word OverlayCpu::ReadWord (Word address)
{
    return (Word) (ReadByteSlow (address) | (ReadByteSlow ((Word) (address + 1)) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu::WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void OverlayCpu::WriteByte (Word address, Byte value)
{
    m_overlay[address] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayCpu::WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void OverlayCpu::WriteWord (Word address, Word value)
{
    WriteByte (address, (Byte) (value & 0xFF));
    WriteByte ((Word) (address + 1), (Byte) (value >> 8));
}
