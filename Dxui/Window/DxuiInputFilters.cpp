#include "Pch.h"

#include "DxuiInputFilters.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters::Add
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInputFilters::Add (const void * key, Filter filter)
{
    Remove (key);
    m_filters.emplace_back (key, std::move (filter));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters::Remove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInputFilters::Remove (const void * key)
{
    std::erase_if (m_filters, [key] (const auto & filter) { return filter.first == key; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters::RunMouse
//
//  True once a filter keeps the event; the rest do not see it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInputFilters::RunMouse (const DxuiMouseEvent & ev) const
{
    for (const auto & filter : m_filters)
    {
        if (filter.second.onMouse && filter.second.onMouse (ev))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters::RunKey
//
//  True once a filter keeps the key; the rest do not see it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInputFilters::RunKey (WPARAM vk) const
{
    for (const auto & filter : m_filters)
    {
        if (filter.second.onKeyDown && filter.second.onKeyDown (vk))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInputFilters::RunMouseLost
//
//  Every filter hears it. A copy is walked, since a handler that ends a drag
//  may remove filters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInputFilters::RunMouseLost (UINT message) const
{
    std::vector<std::pair<const void *, Filter>>  filters = m_filters;



    for (const auto & filter : filters)
    {
        if (filter.second.onMouseLost)
        {
            filter.second.onMouseLost (message);
        }
    }
}
