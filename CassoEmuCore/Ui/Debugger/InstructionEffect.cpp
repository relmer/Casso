#include "Pch.h"

#include "Ui/Debugger/InstructionEffect.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::GetRegisterName
//
//  A pointer into this CPU's own register file, as a letter.
//
////////////////////////////////////////////////////////////////////////////////

const char * ShadowCpu::GetRegisterName (const Byte * which) const
{
    if (which == &A)  { return "A"; }
    if (which == &X)  { return "X"; }
    if (which == &Y)  { return "Y"; }
    if (which == &SP) { return "S"; }
    if (which == &status.status) { return "P"; }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::GetSourceRegister
//
////////////////////////////////////////////////////////////////////////////////

const char * ShadowCpu::GetSourceRegister (Byte opcode) const
{
    return GetRegisterName (GetMicrocode (opcode).pSourceRegister);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::GetDestinationRegister
//
////////////////////////////////////////////////////////////////////////////////

const char * ShadowCpu::GetDestinationRegister (Byte opcode) const
{
    return GetRegisterName (GetMicrocode (opcode).pDestinationRegister);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void ShadowCpu::WriteByte (Word address, Byte value)
{
    m_writes.push_back (Write { address, value });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void ShadowCpu::WriteWord (Word address, Word value)
{
    WriteByte (address,              (Byte) (value & 0xFF));
    WriteByte ((Word) (address + 1), (Byte) (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::ReadByteSlow
//
//  A byte this instruction already wrote reads back as written, so an
//  instruction that writes and reads the same address within itself sees its
//  own value rather than what the machine still holds.
//
////////////////////////////////////////////////////////////////////////////////

Byte ShadowCpu::ReadByteSlow (Word address)
{
    Byte  value = 0;



    m_reads.push_back (address);

    for (auto it = m_writes.rbegin(); it != m_writes.rend(); ++it)
    {
        if (it->address == address)
        {
            return it->value;
        }
    }

    if (m_memory == nullptr || !m_memory->TryPeek (address, value))
    {
        m_isComplete = false;
        return 0;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word ShadowCpu::ReadWord (Word address)
{
    Byte  low  = ReadByteSlow (address);
    Byte  high = ReadByteSlow ((Word) (address + 1));



    return (Word) ((high << 8) | low);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect::GetFlagChanges
//
//  Only the bits that moved, each as the value it takes. A flag the
//  instruction leaves alone says nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstructionEffect::GetFlagChanges (Byte before, Byte after)
{
    static constexpr char  kFlagLetters[] = { 'C', 'Z', 'I', 'D', 'B', '-', 'V', 'N' };
    std::string            text;



    for (int bit = 0; bit < 8; bit++)
    {
        Byte  mask = (Byte) (1u << bit);

        if (kFlagLetters[bit] != '-' && ((before ^ after) & mask) != 0)
        {
            text += std::format (" {}={}", kFlagLetters[bit], (after & mask) ? 1 : 0);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect::Format
//
//  The registers, writes and flags that differ, from a run the caller
//  already made.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstructionEffect::Format (const Cpu6502Registers              & before,
                                       const Cpu6502Registers              & after,
                                       const std::vector<ShadowCpu::Write> & writes,
                                       Word                                  next,
                                       const WriteText                     & describeWrite)
{
    std::string  text;



    if (after.a  != before.a)  { text += std::format (" A={:02X}", after.a);  }
    if (after.x  != before.x)  { text += std::format (" X={:02X}", after.x);  }
    if (after.y  != before.y)  { text += std::format (" Y={:02X}", after.y);  }
    if (after.sp != before.sp) { text += std::format (" S={:02X}", after.sp); }

    for (const ShadowCpu::Write & write : writes)
    {
        std::string  what = describeWrite ? describeWrite (write.address) : std::string();

        text += what.empty() ? std::format (" ${:04X}={:02X}", write.address, write.value)
                             : " " + what;
    }

    text += GetFlagChanges (before.p, after.p);

    //  A PC that merely walked past the instruction is where anyone would
    //  expect it; a branch taken, a jump, a call or a return is not.
    if (after.pc != next)
    {
        text += std::format (" PC=${:04X}", after.pc);
    }

    return text.empty() ? text : text.substr (1);
}
