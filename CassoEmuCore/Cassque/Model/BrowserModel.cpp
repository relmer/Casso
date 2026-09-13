#include "Pch.h"

#include "Cassque/Model/BrowserModel.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::IsSameImage
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::IsSameImage (const std::wstring & a, const std::wstring & b)
{
    return _wcsicmp (a.c_str(), b.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::OpenTab
//
//  A new tab opens at the end and becomes the active one.
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserModel::OpenTab (const Location & location)
{
    Tab  tab;



    tab.location = location;

    m_tabs.push_back (tab);
    m_active = m_tabs.size() - 1;

    return m_active;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::CloseTab
//
//  Closing the active tab moves to the one before it, or the first, so the
//  window is never left showing nothing while tabs remain.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::CloseTab (size_t index)
{
    if (index >= m_tabs.size())
    {
        return false;
    }

    m_tabs.erase (m_tabs.begin() + (ptrdiff_t) index);

    if (m_tabs.empty())
    {
        m_active = 0;
    }
    else if (m_active >= m_tabs.size())
    {
        m_active = m_tabs.size() - 1;
    }
    else if (index < m_active)
    {
        m_active--;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::MoveTab
//
//  Moves a tab to another position; the active tab stays active.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::MoveTab (size_t from, size_t to)
{
    if (from >= m_tabs.size() || to >= m_tabs.size())
    {
        return false;
    }

    if (from < to)
    {
        std::rotate (m_tabs.begin() + (ptrdiff_t) from, m_tabs.begin() + (ptrdiff_t) from + 1, m_tabs.begin() + (ptrdiff_t) to + 1);
    }
    else if (to < from)
    {
        std::rotate (m_tabs.begin() + (ptrdiff_t) to, m_tabs.begin() + (ptrdiff_t) from, m_tabs.begin() + (ptrdiff_t) from + 1);
    }

    if (m_active == from)
    {
        m_active = to;
    }
    else if (from < m_active && m_active <= to)
    {
        m_active--;
    }
    else if (to <= m_active && m_active < from)
    {
        m_active++;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::SwitchTo
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::SwitchTo (size_t index)
{
    if (index >= m_tabs.size())
    {
        return false;
    }

    m_active = index;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GetNextIndex
//
//  Ctrl+Tab's target: the next tab, wrapping.
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserModel::GetNextIndex() const
{
    if (m_tabs.empty())
    {
        return 0;
    }

    return (m_active + 1) % m_tabs.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::NavigateTo
//
//  Going somewhere new pushes the old location behind and drops the forward
//  stack, as every browser does; the selection belongs to the old location
//  and is dropped with it.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::NavigateTo (const Location & location)
{
    Tab &  tab = GetActiveTab();



    if (tab.location.kind != Location::Kind::None)
    {
        tab.back.push_back (tab.location);
    }

    tab.forward.clear();
    tab.selection.clear();
    tab.previewScroll = 0;
    tab.location      = location;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::CanGoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::CanGoBack() const
{
    return HasTabs() && !GetActiveTab().back.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::CanGoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::CanGoForward() const
{
    return HasTabs() && !GetActiveTab().forward.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::GoBack()
{
    Tab &  tab = GetActiveTab();



    if (tab.back.empty())
    {
        return false;
    }

    tab.forward.push_back (tab.location);
    tab.location = tab.back.back();
    tab.back.pop_back();
    tab.selection.clear();
    tab.previewScroll = 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::GoForward()
{
    Tab &  tab = GetActiveTab();



    if (tab.forward.empty())
    {
        return false;
    }

    tab.back.push_back (tab.location);
    tab.location = tab.forward.back();
    tab.forward.pop_back();
    tab.selection.clear();
    tab.previewScroll = 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::SetSelection
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::SetSelection (std::vector<std::wstring> ids)
{
    GetActiveTab().selection = std::move (ids);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::SetSort
//
//  Sorting reorders rows, not what is selected: the selection is kept by
//  id, so it survives untouched.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::SetSort (CatalogModel::Column column, bool descending)
{
    Tab &  tab = GetActiveTab();



    tab.sortColumn     = column;
    tab.sortDescending = descending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::TryGetCachedCatalog
//
//  Any tab's cache answers, since a catalog is a fact about the image and
//  not about who read it.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::TryGetCachedCatalog (const std::wstring & imagePath, VolumeListing & outListing, VolumeKind & outKind) const
{
    for (const Tab & tab : m_tabs)
    {
        if (tab.hasCatalog && IsSameImage (tab.catalogImage, imagePath))
        {
            outListing = tab.catalog;
            outKind    = tab.catalogKind;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::CacheCatalog
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::CacheCatalog (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind)
{
    Tab &  tab = GetActiveTab();



    tab.hasCatalog   = true;
    tab.catalogImage = imagePath;
    tab.catalog      = listing;
    tab.catalogKind  = kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::InvalidateCatalog
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::InvalidateCatalog (const std::wstring & imagePath)
{
    for (Tab & tab : m_tabs)
    {
        if (tab.hasCatalog && IsSameImage (tab.catalogImage, imagePath))
        {
            tab.hasCatalog = false;
            tab.catalog    = VolumeListing();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::InvalidateAllCatalogs
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::InvalidateAllCatalogs()
{
    for (Tab & tab : m_tabs)
    {
        tab.hasCatalog = false;
        tab.catalog    = VolumeListing();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GetLocations
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::GetLocations (std::vector<Location> & outLocations) const
{
    outLocations.clear();

    for (const Tab & tab : m_tabs)
    {
        outLocations.push_back (tab.location);
    }
}
