#include "Pch.h"

#include "Cassque/Model/RefreshAnchor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshAnchor::GetMaxTopRow
//
////////////////////////////////////////////////////////////////////////////////

int RefreshAnchor::GetMaxTopRow (int rowCount, int capacity)
{
    return (capacity > 0 && rowCount > capacity) ? (rowCount - capacity) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshAnchor::Compute
//
////////////////////////////////////////////////////////////////////////////////

RefreshAnchor::After RefreshAnchor::Compute (const Before & before, const std::vector<std::wstring> & keys)
{
    After                                  after;
    std::unordered_map<std::wstring, int>  found;
    int                                    oldCount = (int) before.keys.size();
    int                                    newCount = (int) keys.size();
    int                                    oldMax   = GetMaxTopRow (oldCount, before.capacity);
    int                                    newMax   = GetMaxTopRow (newCount, before.capacity);
    bool                                   placed   = false;
    int                                    i        = 0;



    for (i = 0; i < newCount; i++)
    {
        found.emplace (keys[(size_t) i], i);
    }

    //  The selection, by key. A row that went away drops out of it.
    for (int oldIndex : before.selected)
    {
        if (oldIndex >= 0 && oldIndex < oldCount)
        {
            auto  hit = found.find (before.keys[(size_t) oldIndex]);

            if (hit != found.end())
            {
                after.selected.push_back (hit->second);
            }
        }
    }

    std::sort (after.selected.begin(), after.selected.end());

    //  The focused row, or the nearest selected row still here when the focused
    //  one went away.
    if (before.focused >= 0 && before.focused < oldCount)
    {
        auto  hit = found.find (before.keys[(size_t) before.focused]);

        if (hit != found.end())
        {
            after.focused = hit->second;
        }
    }

    //  Nearest by where the rows were, so a deleted focus lands on the
    //  selected item beside it rather than on whichever sorts first.
    if (after.focused < 0 && before.focused >= 0 && !after.selected.empty())
    {
        int  bestDistance = INT_MAX;

        for (int oldIndex : before.selected)
        {
            if (oldIndex < 0 || oldIndex >= oldCount)
            {
                continue;
            }

            auto  hit = found.find (before.keys[(size_t) oldIndex]);

            if (hit != found.end() && std::abs (oldIndex - before.focused) < bestDistance)
            {
                bestDistance  = std::abs (oldIndex - before.focused);
                after.focused = hit->second;
            }
        }
    }

    //  1. The focused item keeps its screen row, if it was on screen and that
    //     row is reachable.
    if (before.focused >= 0 && before.focused < oldCount
        && before.focused >= before.topRow && before.focused < before.topRow + before.capacity)
    {
        auto  hit = found.find (before.keys[(size_t) before.focused]);

        if (hit != found.end())
        {
            int  offset = before.focused - before.topRow;
            int  top    = hit->second - offset;

            if (top >= 0 && top <= newMax)
            {
                after.topRow = top;
                placed       = true;
            }
        }
    }

    //  2. A view at its end keeps its bottom item on the bottom row. Only a
    //     view that scrolled is at an end worth keeping.
    if (!placed && oldCount > before.capacity && before.topRow >= oldMax && before.capacity > 0)
    {
        int  oldBottom = (std::min) (before.topRow + before.capacity - 1, oldCount - 1);

        for (i = oldBottom; i >= 0 && !placed; i--)
        {
            auto  hit = found.find (before.keys[(size_t) i]);

            if (hit != found.end())
            {
                after.topRow = (std::clamp) (hit->second - (before.capacity - 1), 0, newMax);
                placed       = true;
            }
        }
    }

    //  3. Otherwise the top item keeps the top row, or the first survivor
    //     after it.
    for (i = (std::max) (before.topRow, 0); i < oldCount && !placed; i++)
    {
        auto  hit = found.find (before.keys[(size_t) i]);

        if (hit != found.end())
        {
            after.topRow = (std::clamp) (hit->second, 0, newMax);
            placed       = true;
        }
    }

    if (!placed)
    {
        after.topRow = (std::clamp) (before.topRow, 0, newMax);
    }

    return after;
}
