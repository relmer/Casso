#include "Pch.h"

#include "Debugger/WatchTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchItem::SplitWidth
//
////////////////////////////////////////////////////////////////////////////////

int WatchItem::SplitWidth (const std::string & text, std::string & base)
{
    char  last  = text.empty() ? '\0' : (char) std::tolower ((unsigned char) text.back());
    bool  isCut = text.size() > 2 && text[text.size() - 2] == '.' && (last == 'b' || last == 'w');



    base = isCut ? text.substr (0, text.size() - 2) : text;

    return !isCut ? 0 : (last == 'b') ? 1 : 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::Add
//
////////////////////////////////////////////////////////////////////////////////

bool WatchTable::TryChangeId (int from, int to)
{
    WatchItem  existing;
    auto       found = std::ranges::find_if (m_items, [from] (const WatchItem & item) { return item.id == from; });



    if (found == m_items.end() || (from != to && TryFind (to, existing)))
    {
        return false;
    }

    found->id = to;
    std::sort (m_items.begin(), m_items.end(), [] (const WatchItem & a, const WatchItem & b) { return a.id < b.id; });
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::Add
//
////////////////////////////////////////////////////////////////////////////////

int WatchTable::Add (Word address)
{
    WatchItem  item;



    item.id      = m_nextId++;
    item.address = address;
    m_items.push_back (item);
    return item.id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::AddBasic
//
////////////////////////////////////////////////////////////////////////////////

int WatchTable::AddBasic (const std::string & name)
{
    WatchItem  item;



    item.id        = m_nextId++;
    item.basicName = name;
    item.kind      = WatchItem::Kind::Basic;
    m_items.push_back (item);
    return item.id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::AddTyped
//
//  The item as resolved, given the next id.
//
////////////////////////////////////////////////////////////////////////////////

int WatchTable::AddTyped (WatchItem item)
{
    item.id = m_nextId++;
    m_items.push_back (item);
    return item.id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::AddAt
//
////////////////////////////////////////////////////////////////////////////////

int WatchTable::AddAt (int id, Word address)
{
    WatchItem  item;



    for (WatchItem & existing : m_items)
    {
        if (existing.id == id)
        {
            existing.address = address;
            existing.enabled = true;
            return id;
        }
    }

    item.id      = id;
    item.address = address;
    m_items.push_back (item);
    std::sort (m_items.begin(), m_items.end(), [] (const WatchItem & a, const WatchItem & b) { return a.id < b.id; });
    m_nextId = std::max (m_nextId, id + 1);
    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::TryClear
//
////////////////////////////////////////////////////////////////////////////////

bool WatchTable::TryClear (int id)
{
    size_t  removed = std::erase_if (m_items, [id] (const WatchItem & item) { return item.id == id; });



    return removed > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::ClearAll
//
////////////////////////////////////////////////////////////////////////////////

void WatchTable::ClearAll()
{
    m_items.clear();
    m_nextId = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::TrySetEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool WatchTable::TrySetEnabled (int id, bool enabled)
{
    bool  isFound = false;



    for (WatchItem & item : m_items)
    {
        if (item.id == id)
        {
            item.enabled = enabled;
            isFound      = true;
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTable::TryFind
//
////////////////////////////////////////////////////////////////////////////////

bool WatchTable::TryFind (int id, WatchItem & item) const
{
    for (const WatchItem & candidate : m_items)
    {
        if (candidate.id == id)
        {
            item = candidate;
            return true;
        }
    }

    return false;
}
