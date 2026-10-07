#include "Pch.h"

#include "Debugger/HeatMapSymbols.h"
#include "Debugger/SymbolTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::Build
//
//  A table counts as loaded when a file filled it; the built-in tables have
//  no origins. A constant resolves by name but is not an address, so it has
//  no place in the program's span.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const HeatMapSymbols> HeatMapSymbols::Build (const SymbolTable & symbols)
{
    std::shared_ptr<HeatMapSymbols>  copy     = std::make_shared<HeatMapSymbols>();
    std::vector<SymbolInfo>          entries;
    bool                             isLoaded = false;



    for (int i = 0; i < SymbolTable::kTableCount; i++)
    {
        SymbolTableId  table = (SymbolTableId) i;



        if (!symbols.IsEnabled (table))
        {
            continue;
        }

        entries.clear();
        symbols.GetAll (table, entries);
        isLoaded = !symbols.GetOrigins (table).empty();

        for (const SymbolInfo & entry : entries)
        {
            copy->Add (entry.name, entry.address, entry.size, isLoaded, entry.isConstant);
        }
    }

    return copy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::Add
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapSymbols::Add (const std::string & name, Word address, Word size, bool isLoaded, bool isConstant)
{
    constexpr uint32_t  kLastAddress = 0xFFFF;
    uint32_t            last         = (uint32_t) address + std::max<uint32_t> (size, 1) - 1;



    (void) m_symbols.try_emplace (SymbolTable::ToUpper (name), Entry { address, size });

    if (!isConstant)
    {
        (void) m_names.try_emplace (address, name);
    }

    if (!isLoaded || isConstant)
    {
        return;
    }

    m_programFirst = m_programFirst.has_value() ? std::min (*m_programFirst, address) : address;
    m_programLast  = std::max (m_programLast, std::min (last, kLastAddress));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryGetRegister
//
//  A range is a place in memory, not a moment, so it reads no register.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryGetRegister (const std::string &, Word &) const
{
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryPeek
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryPeek (Word, Byte &) const
{
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryResolveSymbol
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryResolveSymbol (const std::string & name, Word & address) const
{
    auto  found = m_symbols.find (SymbolTable::ToUpper (name));



    if (found == m_symbols.end())
    {
        return false;
    }

    address = found->second.address;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryGetSize
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryGetSize (const std::string & name, Word & size) const
{
    auto  found = m_symbols.find (SymbolTable::ToUpper (name));



    if (found == m_symbols.end() || found->second.size == 0)
    {
        return false;
    }

    size = found->second.size;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryGetProgramSpan
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryGetProgramSpan (Word & first, Word & last) const
{
    if (!m_programFirst.has_value())
    {
        return false;
    }

    first = *m_programFirst;
    last  = (Word) m_programLast;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols::TryGetNameAt
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapSymbols::TryGetNameAt (Word address, std::string & name) const
{
    auto  found = m_names.find (address);



    if (found == m_names.end())
    {
        return false;
    }

    name = found->second;
    return true;
}
