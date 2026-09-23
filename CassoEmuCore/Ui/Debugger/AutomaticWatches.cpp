#include "Pch.h"

#include "Ui/Debugger/AutomaticWatches.h"





//  The flags worth probing. B has no bit of its own to depend on, and bit 5
//  is not a flag at all.
static constexpr std::pair<char, Byte>  s_kFlags[] =
{
    { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  AutomaticWatches::Outcome::DiffersFrom
//
//  Whether two runs of the same instruction ended anywhere different. The
//  flags named in `ignoredFlags` are left out of the comparison: a probe that
//  flipped a flag the instruction never touches would otherwise find its own
//  change staring back at it.
//
////////////////////////////////////////////////////////////////////////////////

bool AutomaticWatches::Outcome::DiffersFrom (const Outcome & other, Byte ignoredFlags, Byte Cpu6502Registers::* ignoredRegister) const
{
    Byte Cpu6502Registers::*  kMembers[] =
    {
        &Cpu6502Registers::a, &Cpu6502Registers::x, &Cpu6502Registers::y, &Cpu6502Registers::sp,
    };
    Byte  mask = (Byte) ~ignoredFlags;



    if (isComplete != other.isComplete)
    {
        return true;
    }

    for (Byte Cpu6502Registers::* member : kMembers)
    {
        if (member != ignoredRegister && registers.*member != other.registers.*member)
        {
            return true;
        }
    }

    if (registers.pc != other.registers.pc || (registers.p & mask) != (other.registers.p & mask))
    {
        return true;
    }

    if (writes.size() != other.writes.size())
    {
        return true;
    }

    for (size_t i = 0; i < writes.size(); i++)
    {
        if (writes[i].address != other.writes[i].address || writes[i].value != other.writes[i].value)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutomaticWatches::Run
//
//  One instruction on a CPU of its own, against memory it cannot change.
//
////////////////////////////////////////////////////////////////////////////////

AutomaticWatches::Outcome AutomaticWatches::Run (const IDebugExpressionContext & memory, const Cpu6502Registers & registers)
{
    ShadowCpu  cpu (memory);
    Outcome    outcome;
    uint32_t   cycles = 0;
    HRESULT    hr     = S_OK;



    cpu.SetRegisters (registers);

    hr = cpu.Step (cycles);

    outcome.isComplete = SUCCEEDED (hr) && cpu.IsComplete();
    outcome.registers  = cpu.GetRegisters();
    outcome.writes     = cpu.GetWrites();

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutomaticWatches::Find
//
////////////////////////////////////////////////////////////////////////////////

std::vector<AutomaticWatches::Item> AutomaticWatches::Find (const IDebugExpressionContext & memory,
                                                            const Cpu6502Registers & registers, Word pc, Word length)
{
    const std::pair<const char *, Byte Cpu6502Registers::*>  kRegisters[] =
    {
        { "A", &Cpu6502Registers::a }, { "X", &Cpu6502Registers::x },
        { "Y", &Cpu6502Registers::y }, { "S", &Cpu6502Registers::sp },
    };
    ShadowCpu          cpu (memory);
    Cpu6502Registers   before = registers;
    Cpu6502Registers   after  = {};
    Outcome            baseline;
    std::vector<Item>  items;
    uint32_t           cycles = 0;
    HRESULT            hr     = S_OK;



    before.pc = pc;

    cpu.SetRegisters (before);

    hr = cpu.Step (cycles);

    if (FAILED (hr) || !cpu.IsComplete())
    {
        return items;
    }

    after               = cpu.GetRegisters();
    baseline.registers  = after;
    baseline.writes     = cpu.GetWrites();
    baseline.isComplete = true;

    //  The instruction's own bytes are read on the way in, and nobody
    //  watches those.
    for (Word address : cpu.GetReads())
    {
        bool  isFetch = (address >= pc && address < (Word) (pc + length));

        if (!isFetch && std::none_of (items.begin(), items.end(),
                                      [address] (const Item & item) { return item.kind == Kind::Address && item.address == address; }))
        {
            items.push_back (Item { Kind::Address, std::string(), address, true, false });
        }
    }

    for (const ShadowCpu::Write & write : cpu.GetWrites())
    {
        auto  found = std::find_if (items.begin(), items.end(),
                                    [&write] (const Item & item) { return item.kind == Kind::Address && item.address == write.address; });

        if (found != items.end())
        {
            found->isWrite = true;
        }
        else
        {
            items.push_back (Item { Kind::Address, std::string(), write.address, false, true });
        }
    }

    //  A register is READ when inverting it changes where the instruction
    //  ends -- ignoring the inverted value itself, which survives untouched
    //  in a register the instruction never looked at. A register it does
    //  change is read as well when the value it lands on follows the value it
    //  started from, which separates INY from LDY.
    for (const auto & [name, member] : kRegisters)
    {
        Cpu6502Registers  probed    = before;
        bool              isWritten = after.*member != before.*member;
        bool              isRead    = false;
        Outcome           result;

        probed.*member = (Byte) ~(before.*member);
        result         = Run (memory, probed);
        isRead         = baseline.DiffersFrom (result, 0, member) ||
                         (isWritten && result.registers.*member != after.*member);

        if (isRead || isWritten)
        {
            items.push_back (Item { Kind::Register, name, 0, isRead, isWritten });
        }
    }

    for (const auto & [letter, bit] : s_kFlags)
    {
        Cpu6502Registers  probed  = before;
        bool              isRead  = false;
        bool              isWrite = ((after.p ^ before.p) & bit) != 0;

        probed.p = (Byte) (before.p ^ bit);
        isRead   = Run (memory, probed).DiffersFrom (baseline, bit, nullptr);

        if (isRead || isWrite)
        {
            items.push_back (Item { Kind::Flag, std::string (1, letter), 0, isRead, isWrite });
        }
    }

    return items;
}
