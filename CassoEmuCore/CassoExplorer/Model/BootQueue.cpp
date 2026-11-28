#include "Pch.h"

#include "CassoExplorer/Model/BootQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::TryAdd
//
//  A connection's image goes to the front, an in-view image ahead of the
//  first folder image, and a folder image to the back.
//
////////////////////////////////////////////////////////////////////////////////

bool BootQueue::TryAdd (Item item)
{
    std::deque<Item>::iterator  waiting  = Find (item.path);
    bool                        promotes = false;



    if (waiting != m_items.end())
    {
        promotes = waiting->urgency == Urgency::Folder && item.urgency != Urgency::Folder;

        if (!promotes)
        {
            return false;
        }

        item.model  = waiting->model;
        item.source = waiting->source;
        m_items.erase (waiting);
    }

    if (item.urgency == Urgency::Connection)
    {
        m_items.push_front (std::move (item));
    }
    else if (item.urgency == Urgency::InView)
    {
        m_items.insert (FindFirstFrom (Urgency::Folder), std::move (item));
    }
    else
    {
        m_items.push_back (std::move (item));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::TryTakeNext
//
//  Images other than folder images are in front, in order. Among folder
//  images the lowest rank goes first, and of those, the one added first.
//
////////////////////////////////////////////////////////////////////////////////

bool BootQueue::TryTakeNext (const RankFn & rank, Item & outItem)
{
    std::deque<Item>::iterator  best     = m_items.begin();
    std::deque<Item>::iterator  each     = m_items.begin();
    int                         bestRank = 0;
    int                         eachRank = 0;



    if (m_items.empty())
    {
        return false;
    }

    if (best->urgency == Urgency::Folder && rank)
    {
        bestRank = rank (std::filesystem::path (best->path).parent_path().wstring());

        for (each = std::next (best); each != m_items.end(); ++each)
        {
            eachRank = rank (std::filesystem::path (each->path).parent_path().wstring());

            if (eachRank < bestRank)
            {
                best     = each;
                bestRank = eachRank;
            }
        }
    }

    outItem = std::move (*best);
    m_items.erase (best);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool BootQueue::Contains (const std::wstring & path) const
{
    return std::any_of (m_items.begin(), m_items.end(), [&path] (const Item & item) { return _wcsicmp (item.path.c_str(), path.c_str()) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::RemoveFolderImages
//
////////////////////////////////////////////////////////////////////////////////

size_t BootQueue::RemoveFolderImages()
{
    return std::erase_if (m_items, [] (const Item & item) { return item.urgency == Urgency::Folder; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::Find
//
////////////////////////////////////////////////////////////////////////////////

std::deque<BootQueue::Item>::iterator BootQueue::Find (const std::wstring & path)
{
    return std::find_if (m_items.begin(), m_items.end(), [&path] (const Item & item) { return _wcsicmp (item.path.c_str(), path.c_str()) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue::FindFirstFrom
//
//  The first item of the urgency or a later one, or the end.
//
////////////////////////////////////////////////////////////////////////////////

std::deque<BootQueue::Item>::iterator BootQueue::FindFirstFrom (Urgency urgency)
{
    return std::find_if (m_items.begin(), m_items.end(), [urgency] (const Item & item) { return item.urgency >= urgency; });
}
