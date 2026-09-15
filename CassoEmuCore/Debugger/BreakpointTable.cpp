#include "Pch.h"

#include "Debugger/BreakpointTable.h"

#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::BreakpointTable
//
////////////////////////////////////////////////////////////////////////////////

BreakpointTable::BreakpointTable (int & nextId) :
    m_nextId (nextId)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddAddress
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddAddress (Word first, Word last)
{
    Breakpoint  entry;



    entry.kind  = BreakpointKind::Address;
    entry.first = first;
    entry.last  = last;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddOpcode
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddOpcode (Byte opcode)
{
    Breakpoint  entry;



    entry.kind   = BreakpointKind::Opcode;
    entry.opcode = opcode;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddCondition
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddCondition (const Expression & condition)
{
    Breakpoint  entry;



    entry.kind      = BreakpointKind::Register;
    entry.condition = condition;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddIo
//
//  Stored for listing and for the session's watch mask. An I/O access is
//  reported by the bus, not seen before an instruction, so this entry never
//  matches in TryMatchBeforeInstruction.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddIo (Word first, Word last)
{
    Breakpoint  entry;



    entry.kind  = BreakpointKind::Io;
    entry.first = first;
    entry.last  = last;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddBrk
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddBrk()
{
    Breakpoint  entry;



    entry.kind   = BreakpointKind::Brk;
    entry.opcode = kBrkOpcode;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::AddInterrupt
//
//  An interrupt is dispatched inside the CPU step, so this entry is matched
//  by the session when the run driver reports one, not here.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::AddInterrupt()
{
    Breakpoint  entry;



    entry.kind = BreakpointKind::Interrupt;
    return Add (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::TryClear
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointTable::TryClear (int id)
{
    size_t  removed = std::erase_if (m_entries, [id] (const Breakpoint & entry) { return entry.id == id; });



    RebuildAddressBits();
    return removed > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::ClearAll
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointTable::ClearAll()
{
    m_entries.clear();
    RebuildAddressBits();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::TrySetEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointTable::TrySetEnabled (int id, bool enabled)
{
    bool  isFound = false;



    for (Breakpoint & entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.enabled = enabled;
            isFound       = true;
        }
    }

    RebuildAddressBits();
    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::HasEnabledStopCondition
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointTable::HasEnabledStopCondition() const
{
    return std::any_of (m_entries.begin(), m_entries.end(), [] (const Breakpoint & entry) { return entry.enabled; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::TryMatchBeforeInstruction
//
//  Two passes keep the order of precedence: address entries first, through
//  the bitmap, then every other kind.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointTable::TryMatchBeforeInstruction (
    Word                            pc,
    Byte                            opcode,
    const IDebugExpressionContext & context,
    int                           & hitId)
{
    bool  isAddressPass = m_addressBits[pc];



    for (int pass = isAddressPass ? 0 : 1; pass < 2; ++pass)
    {
        for (Breakpoint & entry : m_entries)
        {
            bool  isAddressKind = entry.kind == BreakpointKind::Address;



            if (!entry.enabled || isAddressKind != (pass == 0))
            {
                continue;
            }

            if (TryMatchEntry (entry, pc, opcode, context))
            {
                ++entry.hits;
                hitId = entry.id;
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::TryMatchEntry
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointTable::TryMatchEntry (
    Breakpoint                    & entry,
    Word                            pc,
    Byte                            opcode,
    const IDebugExpressionContext & context) const
{
    int32_t      value = 0;
    std::string  error;
    HRESULT      hr    = S_OK;



    switch (entry.kind)
    {
    case BreakpointKind::Address:
        return pc >= entry.first && pc <= entry.last;

    case BreakpointKind::Opcode:
    case BreakpointKind::Brk:
        return opcode == entry.opcode;

    case BreakpointKind::Register:
        hr = DebugExpressionEvaluator::Evaluate (entry.condition, context, value, error);
        return SUCCEEDED (hr) && value != 0;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::Add
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointTable::Add (Breakpoint entry)
{
    entry.id = m_nextId++;
    m_entries.push_back (entry);
    RebuildAddressBits();
    return entry.id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTable::RebuildAddressBits
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointTable::RebuildAddressBits()
{
    std::fill (m_addressBits.begin(), m_addressBits.end(), false);

    for (const Breakpoint & entry : m_entries)
    {
        if (!entry.enabled || entry.kind != BreakpointKind::Address)
        {
            continue;
        }

        for (int address = entry.first; address <= entry.last; ++address)
        {
            m_addressBits[address] = true;
        }
    }
}
