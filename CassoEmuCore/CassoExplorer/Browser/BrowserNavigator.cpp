#include "Pch.h"

#include "CassoExplorer/Browser/BrowserNavigator.h"
#include "CassoExplorer/Browser/IBrowserListing.h"
#include "CassoExplorer/Model/CollectionQuery.h"
#include "CassoExplorer/Model/HostFolderReader.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Seams/IShellItemVerbs.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::BrowserNavigator
//
////////////////////////////////////////////////////////////////////////////////

BrowserNavigator::BrowserNavigator (
    BrowserModel      & model,
    IFileSystem       & fs,
    HostFolderReader  & hostFolders,
    IBrowserListing   & listing)
    : m_model       (model),
      m_fs          (fs),
      m_hostFolders (hostFolders),
      m_listing     (listing)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::NewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserNavigator::NewTab()
{
    //  Home, the way a browser and Explorer open one, rather than a second
    //  copy of the folder the tab it was opened from is showing.
    size_t  index = m_model.OpenTab (Location());



    m_listing.ReloadAfterNavigation();

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::SwitchTab
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::SwitchTab (size_t index)
{
    std::vector<std::wstring>  names;



    if (!m_model.SwitchTo (index))
    {
        return false;
    }

    //  Returning to a tab lists what was last read there; Refresh, or going to
    //  the drive again, reads its disks.
    names = m_model.GetActiveTab().selection;
    m_listing.ReturnToTab (names);

    if (m_listing.IsHomeLocation())
    {
        m_listing.RequestHomeRead();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::CloseTab
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::CloseTab (size_t index)
{
    if (m_model.GetTabCount() <= 1 || !m_model.CloseTab (index))
    {
        return false;
    }

    return SwitchTab (m_model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::InsertTab
//
//  The tab's cached catalog moves with it, so its disk is not read again.
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserNavigator::InsertTab (size_t index, BrowserModel::Tab tab)
{
    index = m_model.InsertTab (index, std::move (tab));
    SwitchTab (index);

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::OpenInNewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserNavigator::OpenInNewTab (const Location & location)
{
    size_t  index = m_model.OpenTab (location);



    m_listing.ReloadAfterNavigation();

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::DuplicateTab
//
//  A new tab at the same location; its history starts fresh.
//
////////////////////////////////////////////////////////////////////////////////

size_t BrowserNavigator::DuplicateTab (size_t index)
{
    if (index >= m_model.GetTabCount())
    {
        return m_model.GetActiveIndex();
    }

    return OpenInNewTab (m_model.GetTab (index).location);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::CloseOtherTabs
//
//  Closed from the end, so the indices still to close do not move.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::CloseOtherTabs (size_t index)
{
    size_t  i      = m_model.GetTabCount();
    bool    closed = false;



    if (index >= m_model.GetTabCount())
    {
        return false;
    }

    while (i-- > 0)
    {
        if (i != index && m_model.CloseTab (i))
        {
            closed = true;
        }
    }

    if (closed)
    {
        SwitchTab (0);
    }

    return closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::CloseTabsToRight
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::CloseTabsToRight (size_t index)
{
    size_t  i      = m_model.GetTabCount();
    bool    closed = false;



    while (i-- > index + 1)
    {
        if (m_model.CloseTab (i))
        {
            closed = true;
        }
    }

    if (closed)
    {
        SwitchTab (m_model.GetActiveIndex());
    }

    return closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::RestoreTabs
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNavigator::RestoreTabs (const std::vector<Location> & locations)
{
    bool  switched = false;



    //  Nothing saved -- a first run -- opens one tab at Home, as Explorer
    //  opens one, rather than leaving the strip empty.
    if (locations.empty())
    {
        if (m_model.GetTabCount() == 0)
        {
            m_model.OpenTab (Location());
            switched = SwitchTab (0);
            IGNORE_RETURN_VALUE (switched, true);
        }

        return;
    }

    for (bool closed = true; closed && m_model.GetTabCount() > 0; )
    {
        closed = m_model.CloseTab (0);
    }

    for (const Location & location : locations)
    {
        if (!m_listing.IsOnMissingDrive (location))
        {
            m_model.OpenTab (location);
        }
    }

    if (m_model.GetTabCount() == 0)
    {
        m_model.OpenTab (Location());
    }

    switched = SwitchTab (0);
    IGNORE_RETURN_VALUE (switched, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::RestoreTabs
//
//  Whole tabs, history and view included, with `active` active. A tab left
//  closed for a missing drive moves the active index along with the rest; if
//  the active tab itself is left closed, the first tab is active.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNavigator::RestoreTabs (std::vector<BrowserModel::Tab> tabs, size_t active)
{
    size_t  kept     = 0;
    size_t  showing  = 0;
    size_t  i        = 0;
    bool    switched = false;



    if (tabs.empty())
    {
        RestoreTabs (std::vector<Location>());
        return;
    }

    for (bool closed = true; closed && m_model.GetTabCount() > 0; )
    {
        closed = m_model.CloseTab (0);
    }

    for (i = 0; i < tabs.size(); i++)
    {
        if (m_listing.IsOnMissingDrive (tabs[i].location))
        {
            continue;
        }

        if (i == active)
        {
            showing = kept;
        }

        m_model.InsertTab (kept++, std::move (tabs[i]));
    }

    if (m_model.GetTabCount() == 0)
    {
        m_model.OpenTab (Location());
    }

    switched = SwitchTab (showing);
    IGNORE_RETURN_VALUE (switched, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::NavigateToLocation
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNavigator::NavigateToLocation (const Location & location)
{
    if (!m_model.HasTabs())
    {
        m_model.OpenTab (location);
    }
    else if (m_model.GetActiveTab().location != location)
    {
        m_model.NavigateTo (location);
    }

    m_listing.ReloadAfterNavigation();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::NavigateToAddress
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::NavigateToAddress (const std::wstring & text)
{
    std::wstring  rooted = RootOnSystemDrive (ExpandVariables (text));
    Location      location;
    bool          parsed = BrowserModel::ParseAddress (m_fs, rooted, location);



    //  A zip, or a folder inside one, opens as the shell's folder it is.
    for (size_t end = rooted.find (L'\\'); !parsed && end != std::wstring::npos; )
    {
        size_t  next = rooted.find (L'\\', end + 1);

        if (m_hostFolders.IsArchive (rooted.substr (0, next)))
        {
            location = Location::MakeShellFolder (rooted, std::filesystem::path (rooted).filename().wstring());
            parsed   = true;
        }

        end = next;
    }

    //  A shell folder typed by the shell's name for it shows the name it has.
    if (parsed && location.kind == Location::Kind::ShellFolder && location.label.empty() && m_shellVerbs != nullptr)
    {
        HRESULT  hr = m_shellVerbs->GetShellItemName (location.path, location.label);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (parsed)
    {
        NavigateToLocation (location);

        //  After the navigation, so that a path going nowhere is never offered
        //  back.
        m_typedPaths.Add (RootOnSystemDrive (text));
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::RootOnSystemDrive
//
//  A path that starts at a root with no drive -- "\", "\Users" -- is on the
//  system drive, the one Windows booted from, as Explorer reads it. A UNC path
//  and anything else are kept.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserNavigator::RootOnSystemDrive (const std::wstring & text)
{
    wchar_t  windows[MAX_PATH] = {};
    UINT     length            = 0;



    if (text.empty() || text[0] != L'\\' || (text.size() > 1 && text[1] == L'\\'))
    {
        return text;
    }

    length = GetWindowsDirectoryW (windows, MAX_PATH);

    return (length >= 2) ? std::wstring (windows, 2) + text : L"C:" + text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::ExpandVariables
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserNavigator::ExpandVariables (const std::wstring & text)
{
    std::wstring  expanded;
    DWORD         length = 0;



    if (text.find (L'%') == std::wstring::npos)
    {
        return text;
    }

    length = ExpandEnvironmentStringsW (text.c_str(), nullptr, 0);

    if (length == 0)
    {
        return text;
    }

    expanded.resize (length);
    length = ExpandEnvironmentStringsW (text.c_str(), expanded.data(), length);

    if (length == 0 || length > expanded.size())
    {
        return text;
    }

    expanded.resize (length - 1);
    return expanded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::ReloadWithSelection
//
//  The location Back or Forward returned to is listed with the rows that
//  were selected when it was left. Listing it clears the selection, so the
//  keys are copied first.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNavigator::ReloadWithSelection()
{
    std::vector<std::wstring>  keys = m_model.GetActiveTab().selection;



    m_listing.ReloadAfterNavigation();
    m_listing.SelectRowsByKeys (keys);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::GoBackBy
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::GoBackBy (size_t steps)
{
    size_t  moved = 0;



    while (moved < steps && m_model.HasTabs() && m_model.GoBack (moved == 0))
    {
        moved++;
    }

    if (moved > 0)
    {
        ReloadWithSelection();
    }

    return moved > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::GoForwardBy
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::GoForwardBy (size_t steps)
{
    size_t  moved = 0;



    while (moved < steps && m_model.HasTabs() && m_model.GoForward (moved == 0))
    {
        moved++;
    }

    if (moved > 0)
    {
        ReloadWithSelection();
    }

    return moved > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::CanGoUp
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::CanGoUp() const
{
    Location  location = GetLocation();



    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            return !BrowserRowPaths::GetParentFolder (location.path).empty();

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
        case Location::Kind::AppleDrive:
        case Location::Kind::Partition:
            return true;

        case Location::Kind::ShellFolder:
            return SearchQuery::IsId (location.path) || CollectionQuery::IsId (location.path) || m_listing.GetShellParent().kind != Location::Kind::None;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::TryGetUpTarget
//
//  An image's parent is the folder holding it; a directory inside an image
//  goes to the image.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::TryGetUpTarget (Location & outTarget) const
{
    Location  location = GetLocation();



    if (!CanGoUp())
    {
        return false;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        outTarget = Location::MakeDiskImage (location.path);
    }
    else if (location.kind == Location::Kind::Partition)
    {
        //  A partition goes up to the image it is in.
        outTarget = Location::MakeDiskImage (location.path.substr (0, location.path.rfind (L'|')));
    }
    else if (PhysicalCards::IsOnCard (location.path))
    {
        //  A partition goes up to its card, and a card to This PC.
        outTarget = PhysicalCards::IsCardPath (location.path) ? Location::MakeRoot (TreeModel::kThisPcRootId)
                                                              : Location::MakeDiskImage (location.path.substr (0, location.path.find (L'|')));
    }
    else if (location.kind == Location::Kind::DiskImage && BrowserRowPaths::IsPartitionVolume (location.path))
    {
        //  A partition's volume goes up to its partition.
        outTarget = Location::MakePartition (location.path);
    }
    else if (location.kind == Location::Kind::AppleDrive)
    {
        std::wstring  link;
        int           slot  = 0;
        int           drive = 0;
        size_t        slash = location.innerPath.find_last_of ('/');

        TreeModel::TryParseLinkId (location.path, link, slot, drive);

        //  A directory goes up to its parent, and a drive to its link.
        outTarget = location.innerPath.empty()       ? Location::MakeRoot (TreeModel::MakeLinkId (link))
                  : slash == std::string::npos      ? Location::MakeAppleDrive (location.path, std::string())
                                                    : Location::MakeAppleDrive (location.path, location.innerPath.substr (0, slash));
    }
    else if (location.kind == Location::Kind::ShellFolder && SearchQuery::IsId (location.path))
    {
        std::wstring               scope;
        std::wstring               query;
        std::vector<std::wstring>  folders;

        //  A search's results go up to the folder searched, or to Casso's
        //  root for one that searched all of Casso's folders.
        SearchQuery::TryParseId (location.path, scope, query);
        folders   = SearchQuery::SplitScopes (scope);
        outTarget = (folders.size() == 1 && SearchQuery::IsFolderScope (folders[0])) ? Location::MakeHostFolder (folders[0]) : Location::MakeRoot (TreeModel::kCassoRootId);
    }
    else if (location.kind == Location::Kind::ShellFolder && CollectionQuery::IsId (location.path))
    {
        CollectionQuery::Tool  tool = CollectionQuery::Tool::DiskList;
        std::wstring           folder;

        //  Disk list's results go up to the folder walked.
        CollectionQuery::TryParseId (location.path, tool, folder);
        outTarget = Location::MakeHostFolder (folder);
    }
    else if (location.kind == Location::Kind::ShellFolder)
    {
        outTarget = m_listing.GetShellParent();
    }
    else
    {
        outTarget = Location::MakeHostFolder (BrowserRowPaths::GetParentFolder (location.path));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::GoUp
//
//  An image's parent is the folder holding it; a directory inside an image
//  goes to the image.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::GoUp()
{
    Location  target;



    if (!TryGetUpTarget (target))
    {
        return false;
    }

    m_model.NavigateTo (target);
    m_listing.ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::GoBack()
{
    if (!m_model.HasTabs() || !m_model.GoBack())
    {
        return false;
    }

    ReloadWithSelection();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNavigator::GoForward()
{
    if (!m_model.HasTabs() || !m_model.GoForward())
    {
        return false;
    }

    ReloadWithSelection();

    return true;
}





