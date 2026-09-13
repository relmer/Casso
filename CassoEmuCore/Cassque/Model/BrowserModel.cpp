#include "Pch.h"

#include "Cassque/Model/BrowserModel.h"
#include "Cassque/Model/TreeModel.h"
#include "Core/TextEncoding.h"
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





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GetAddressSegments
//
//  A host path splits at its separators below the drive or share; an image
//  follows the folder holding it, and a directory inside one follows the
//  image, one segment per directory.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BrowserModel::AddressSegment> BrowserModel::GetAddressSegments (const Location & location)
{
    std::vector<AddressSegment>  segments;
    size_t                       slash = 0;
    size_t                       start = 0;
    std::string                  inner;



    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            AppendHostSegments (location.path, segments);
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            slash = location.path.rfind (L'\\');
            AppendHostSegments (location.path.substr (0, (slash == std::wstring::npos) ? 0 : slash), segments);
            segments.push_back (AddressSegment { location.path.substr (slash + 1), Location::MakeDiskImage (location.path) });
            break;

        default:
            segments.push_back (AddressSegment { L"Home", Location() });
            break;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        while (start < location.innerPath.size())
        {
            slash = location.innerPath.find_first_of ("/:", start);
            slash = (slash == std::string::npos) ? location.innerPath.size() : slash;

            if (slash > start)
            {
                inner += (inner.empty() ? "" : "/") + location.innerPath.substr (start, slash - start);
                segments.push_back (AddressSegment { TextEncoding::NarrowToWide (location.innerPath.substr (start, slash - start)),
                                                     Location::MakeDiskDirectory (location.path, inner) });
            }

            start = slash + 1;
        }
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::AppendHostSegments
//
//  The first segment is the drive, or a share's server and share together,
//  as Explorer shows them.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::AppendHostSegments (const std::wstring & path, std::vector<AddressSegment> & outSegments)
{
    std::wstring  prefix;
    size_t        slash  = std::wstring::npos;
    size_t        start  = 0;



    if (path.compare (0, 2, L"\\\\") == 0)
    {
        slash  = path.find (L'\\', 2);
        slash  = (slash == std::wstring::npos) ? slash : path.find (L'\\', slash + 1);
        prefix = path.substr (0, slash);
        outSegments.push_back (AddressSegment { prefix, Location::MakeHostFolder (prefix) });
    }
    else if (!path.empty())
    {
        slash  = path.find (L'\\');
        prefix = path.substr (0, slash);
        outSegments.push_back (AddressSegment { prefix, Location::MakeHostFolder (prefix + L"\\") });
    }

    start = (slash == std::wstring::npos) ? path.size() : slash + 1;

    while (start < path.size())
    {
        slash = path.find (L'\\', start);
        slash = (slash == std::wstring::npos) ? path.size() : slash;

        if (slash > start)
        {
            prefix += L"\\" + path.substr (start, slash - start);
            outSegments.push_back (AddressSegment { path.substr (start, slash - start), Location::MakeHostFolder (prefix) });
        }

        start = slash + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::FormatAddress
//
//  Home has no path, so its address is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserModel::FormatAddress (const Location & location)
{
    std::wstring  text;
    std::wstring  inner;



    if (location.kind != Location::Kind::None)
    {
        text = location.path;
    }

    if (location.kind == Location::Kind::DiskDirectory && !location.innerPath.empty())
    {
        inner = TextEncoding::NarrowToWide (location.innerPath);
        std::replace (inner.begin(), inner.end(), L'/', L'\\');
        std::replace (inner.begin(), inner.end(), L':', L'\\');
        text += L"\\" + inner;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::ParseAddress
//
//  Spaces, quotes and trailing separators around the text are ignored. The
//  first prefix naming a disk image that exists makes the rest a directory
//  inside it; otherwise the whole path must be a folder on the host.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::ParseAddress (IFileSystem & fs, const std::wstring & text, Location & outLocation)
{
    constexpr const wchar_t *  s_kpszTrimmed = L" \t\"";



    std::wstring  path;
    std::wstring  prefix;
    size_t        first = text.find_first_not_of (s_kpszTrimmed);
    size_t        last  = text.find_last_not_of (s_kpszTrimmed);
    size_t        end   = 0;
    bool          found = false;



    if (first != std::wstring::npos)
    {
        path = text.substr (first, last - first + 1);
    }

    while (path.size() > 3 && path.back() == L'\\')
    {
        path.pop_back();
    }

    if (path.size() == 2 && path[1] == L':')
    {
        path += L'\\';
    }

    while (!path.empty() && !found && end != std::wstring::npos)
    {
        end    = path.find (L'\\', end + 1);
        prefix = path.substr (0, end);

        if (TreeModel::IsSupportedImage (prefix) && fs.Exists (prefix))
        {
            found = true;

            if (end == std::wstring::npos || end + 1 >= path.size())
            {
                outLocation = Location::MakeDiskImage (prefix);
            }
            else
            {
                std::wstring  inner = path.substr (end + 1);

                std::replace (inner.begin(), inner.end(), L'\\', L'/');
                outLocation = Location::MakeDiskDirectory (prefix, TextEncoding::WideToNarrow (inner));
            }
        }
    }

    if (!found && !path.empty() && IsHostFolder (fs, path))
    {
        outLocation = Location::MakeHostFolder (path);
        found       = true;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::IsHostFolder
//
//  A folder is one its parent lists as a folder. The file system's Exists
//  answers for files only, so a drive root counts when it can be listed.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::IsHostFolder (IFileSystem & fs, const std::wstring & path)
{
    std::vector<FileSystemEntry>  entries;
    std::wstring                  folder = path;
    std::wstring                  name;
    size_t                        slash  = 0;
    HRESULT                       hr     = E_FAIL;
    bool                          found  = false;



    while (!folder.empty() && folder.back() == L'\\')
    {
        folder.pop_back();
    }

    slash = folder.rfind (L'\\');

    if (slash == std::wstring::npos)
    {
        hr    = folder.empty() ? E_FAIL : fs.EnumerateEntries (folder, entries);
        found = SUCCEEDED (hr);
    }
    else
    {
        hr   = fs.EnumerateEntries (folder.substr (0, slash), entries);
        name = folder.substr (slash + 1);

        for (const FileSystemEntry & entry : entries)
        {
            found = found || (SUCCEEDED (hr) && entry.isFolder && _wcsicmp (entry.name.c_str(), name.c_str()) == 0);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::GetFolderChildren
//
////////////////////////////////////////////////////////////////////////////////

void BrowserModel::GetFolderChildren (IFileSystem & fs, const Location & location, std::vector<AddressSegment> & outChildren)
{
    std::vector<FileSystemEntry>  entries;
    std::vector<AddressSegment>   images;
    std::wstring                  folder = location.path;
    HRESULT                       hr     = E_FAIL;



    outChildren.clear();

    while (!folder.empty() && folder.back() == L'\\')
    {
        folder.pop_back();
    }

    if (location.kind == Location::Kind::HostFolder && !folder.empty())
    {
        hr = fs.EnumerateEntries (folder, entries);
    }

    for (const FileSystemEntry & entry : entries)
    {
        if (SUCCEEDED (hr) && entry.isFolder)
        {
            outChildren.push_back (AddressSegment { entry.name, Location::MakeHostFolder (folder + L"\\" + entry.name) });
        }
        else if (SUCCEEDED (hr) && TreeModel::IsSupportedImage (entry.name))
        {
            images.push_back (AddressSegment { entry.name, Location::MakeDiskImage (folder + L"\\" + entry.name) });
        }
    }

    std::sort (outChildren.begin(), outChildren.end(), IsLabelBefore);
    std::sort (images.begin(), images.end(), IsLabelBefore);
    outChildren.insert (outChildren.end(), images.begin(), images.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel::IsLabelBefore
//
//  Name order as Explorer sorts, without regard to case.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserModel::IsLabelBefore (const AddressSegment & a, const AddressSegment & b)
{
    return _wcsicmp (a.label.c_str(), b.label.c_str()) < 0;
}
