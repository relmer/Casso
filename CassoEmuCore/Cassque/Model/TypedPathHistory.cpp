#include "Pch.h"

#include "Cassque/Model/TypedPathHistory.h"





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
