#include "Pch.h"

#include "Debugger/StepFilter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilter::Add
//
////////////////////////////////////////////////////////////////////////////////

void StepFilter::Add (const std::string & name, Word first, Word last)
{
    StepFilterEntry  entry;



    std::erase_if (m_entries, [first, last] (const StepFilterEntry & e) { return e.first == first && e.last == last; });

    entry.name  = name;
    entry.first = first;
    entry.last  = last;

    m_entries.push_back (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilter::TryRemove
//
//  By name when an entry has it, otherwise by the range the name evaluated
//  to. False when neither matches.
//
////////////////////////////////////////////////////////////////////////////////

bool StepFilter::TryRemove (const std::string & name, std::optional<std::pair<Word, Word>> range)
{
    size_t  removed = std::erase_if (m_entries, [&name] (const StepFilterEntry & e) { return !e.name.empty() && IsSameName (e.name, name); });



    if (removed == 0 && range.has_value())
    {
        removed = std::erase_if (m_entries, [&range] (const StepFilterEntry & e) { return e.first == range->first && e.last == range->second; });
    }

    return removed != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilter::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool StepFilter::Contains (Word address) const
{
    for (const StepFilterEntry & entry : m_entries)
    {
        if (address >= entry.first && address <= entry.last)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilter::IsSameName
//
//  Symbols match without regard to case, as the symbol tables look them up.
//
////////////////////////////////////////////////////////////////////////////////

bool StepFilter::IsSameName (const std::string & a, const std::string & b)
{
    auto  toUpper = [] (char c) { return (char) std::toupper ((unsigned char) c); };



    return a.size() == b.size() && std::equal (a.begin(), a.end(), b.begin(), [&toUpper] (char x, char y) { return toUpper (x) == toUpper (y); });
}
