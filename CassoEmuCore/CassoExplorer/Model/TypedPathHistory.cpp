#include "Pch.h"

#include "CassoExplorer/Model/TypedPathHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory::IsBlank
//
////////////////////////////////////////////////////////////////////////////////

bool TypedPathHistory::IsBlank (const std::wstring & text)
{
    return text.find_first_not_of (L" \t") == std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory::Add
//
////////////////////////////////////////////////////////////////////////////////

void TypedPathHistory::Add (const std::wstring & text)
{
    size_t  i = 0;



    if (IsBlank (text))
    {
        return;
    }

    for (i = 0; i < m_entries.size(); i++)
    {
        if (_wcsicmp (m_entries[i].c_str(), text.c_str()) == 0)
        {
            m_entries.erase (m_entries.begin() + (ptrdiff_t) i);
            break;
        }
    }

    m_entries.insert (m_entries.begin(), text);

    if (m_entries.size() > kMaxEntries)
    {
        m_entries.resize (kMaxEntries);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory::Clear
//
////////////////////////////////////////////////////////////////////////////////

void TypedPathHistory::Clear()
{
    m_entries.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory::Reset
//
//  Oldest first, so that adding each in turn leaves the newest on top and
//  applies the limit and the duplicate rule to a hand-edited file.
//
////////////////////////////////////////////////////////////////////////////////

void TypedPathHistory::Reset (const std::vector<std::wstring> & entries)
{
    size_t  i = 0;



    m_entries.clear();

    for (i = entries.size(); i > 0; i--)
    {
        Add (entries[i - 1]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistory::Merge
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> TypedPathHistory::Merge (const std::vector<std::wstring> & explorer, const std::vector<std::wstring> & ours)
{
    std::vector<std::wstring>  merged;
    size_t                     next = 0;



    auto  find = [&merged] (const std::wstring & text)
    {
        return std::find_if (merged.begin(), merged.end(),
                             [&text] (const std::wstring & held) { return _wcsicmp (held.c_str(), text.c_str()) == 0; });
    };

    for (const std::wstring & text : explorer)
    {
        if (!IsBlank (text) && find (text) == merged.end())
        {
            merged.push_back (text);
        }
    }

    //  Ours in order, newest first: one Explorer holds marks the place, and
    //  one it lacks goes in after the last place marked.
    for (const std::wstring & text : ours)
    {
        auto  found = find (text);

        if (found != merged.end())
        {
            next = (size_t) (found - merged.begin()) + 1;
        }
        else if (!IsBlank (text))
        {
            merged.insert (merged.begin() + (std::ptrdiff_t) next, text);
            next++;
        }
    }

    if (merged.size() > kMaxEntries)
    {
        merged.resize (kMaxEntries);
    }

    return merged;
}
