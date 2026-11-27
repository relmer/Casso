#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueue
//
//  The disk images waiting to be booted, in the order they are booted. An
//  image a connection wrote goes first, then the images in view in a list
//  or the preview, then the images of the folders being swept for
//  thumbnails, ordered by the rank of the folder each is in and then by
//  when each was added. Not thread-safe; ExplorerBootWorker locks it.
//
////////////////////////////////////////////////////////////////////////////////

class BootQueue
{
public:
    enum class Urgency { Connection, InView, Folder };

    struct Item
    {
        std::wstring  path;
        std::string   model;
        std::string   source;
        Urgency       urgency = Urgency::InView;
    };

    //  A folder's rank: lower is booted sooner.
    using RankFn = std::function<int (const std::wstring & folder)>;

    //  False for a path already waiting. A folder image already waiting
    //  that is added again as in view moves up to the end of the in-view
    //  images, and true is returned.
    bool    TryAdd      (Item item);

    //  The next image to boot, removed. False when nothing is waiting.
    bool    TryTakeNext (const RankFn & rank, Item & outItem);

    bool    Contains    (const std::wstring & path) const;
    size_t  GetCount    () const { return m_items.size(); }
    void    Clear       ()       { m_items.clear(); }

    //  Removes every folder image and returns the count removed.
    size_t  RemoveFolderImages ();

private:
    std::deque<Item>::iterator  Find          (const std::wstring & path);
    std::deque<Item>::iterator  FindFirstFrom (Urgency urgency);

    std::deque<Item>  m_items;
};
