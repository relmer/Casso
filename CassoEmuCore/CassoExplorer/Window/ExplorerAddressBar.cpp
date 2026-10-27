#include "Pch.h"

#include "CassoExplorer/Window/ExplorerAddressBar.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/ILinkAnnouncements.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Devices/Disk/DiskImageStore.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Config/GlobalUserPrefs.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32UserClasses.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ExplorerAddressBar
//
////////////////////////////////////////////////////////////////////////////////

ExplorerAddressBar::ExplorerAddressBar (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GoToLocation
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::GoToLocation (const Location & location)
{
    if (m_window.m_browser.GetLocation() != location)
    {
        m_window.m_browser.NavigateToLocation (location);
        m_window.m_listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetAddressSegmentsFor
//
//  The address bar's segments, as Explorer shows a path on a drive: This PC,
//  then the drive under its name, then the folders. Never empty.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BrowserModel::AddressSegment> ExplorerAddressBar::GetAddressSegmentsFor (const Location & location)
{
    std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (location, m_window.m_addressRoot);
    IShellIcons::DriveInfo                     drive;
    bool                                       onDrive  = false;



    if (!segments.empty() && segments.front().location.kind == Location::Kind::HostFolder)
    {
        const std::wstring &  root = segments.front().location.path;

        onDrive = root.size() == 3 && root[1] == L':' && root[2] == L'\\';
    }

    if (onDrive)
    {
        if (m_window.m_shellIcons.GetDriveInfo (segments.front().location.path, drive) && !drive.name.empty())
        {
            segments.front().label = drive.name;
        }

        segments.insert (segments.begin(), BrowserModel::AddressSegment { TreeModel::GetRootLabel (TreeModel::kThisPcRootId),
                                                                          Location::MakeRoot (TreeModel::kThisPcRootId) });
    }

    if (segments.empty())
    {
        segments.push_back (BrowserModel::AddressSegment { std::wstring(), location });
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetNavLabel
//
//  A location as the address bar shows it: a drive by its name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::GetNavLabel (const Location & location)
{
    std::vector<BrowserModel::AddressSegment>  segments = GetAddressSegmentsFor (location);



    return segments.back().label.empty() ? CassoExplorerBrowser::GetLocationLabel (location) : segments.back().label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::FillNavTips
//
//  File Explorer's navigation tips say where each button goes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::FillNavTips()
{
    const BrowserModel &  model = m_window.m_browser.GetBrowserModel();
    Location              up;
    std::wstring          back    = L"Back";
    std::wstring          forward = L"Forward";
    std::wstring          upTip   = L"Up (Alt + Up Arrow)";



    if (model.HasTabs() && !model.GetActiveTab().back.empty())
    {
        back = L"Back to " + GetNavLabel (model.GetActiveTab().back.back());
    }

    if (model.HasTabs() && !model.GetActiveTab().forward.empty())
    {
        forward = L"Forward to " + GetNavLabel (model.GetActiveTab().forward.back());
    }

    if (m_window.m_browser.TryGetUpTarget (up))
    {
        upTip = L"Up to \"" + GetNavLabel (up) + L"\" (Alt + Up Arrow)";
    }

    m_window.m_commands.SetTip (CassoExplorerCommands::kBack,    back);
    m_window.m_commands.SetTip (CassoExplorerCommands::kForward, forward);
    m_window.m_commands.SetTip (CassoExplorerCommands::kUp,      upTip);
    m_window.m_commands.SetTip (CassoExplorerCommands::kRefresh, L"Refresh \"" + GetNavLabel (m_window.m_browser.GetLocation()) + L"\" (F5)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::FillAddress
//
//  The segments and the path of where the active tab is.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::FillAddress()
{
    Location                   location = m_window.m_browser.GetLocation();
    std::vector<std::wstring>  labels;



    m_window.m_addressSegments = GetAddressSegmentsFor (location);

    for (const BrowserModel::AddressSegment & segment : m_window.m_addressSegments)
    {
        labels.push_back (segment.label);
    }

    m_window.m_address->SetSegments (std::move (labels));
    m_window.m_address->SetPath (BrowserModel::FormatAddress (location));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::SubmitAddress
//
//  A path that goes nowhere says so and leaves the text as typed.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::SubmitAddress (const std::wstring & text)
{
    if (m_window.m_browser.NavigateToAddress (text))
    {
        m_window.m_input.SetFocusPane (ExplorerInput::Pane::List);
        m_window.m_listPane.FillList();

        //  The typed path joins the history now, not only at a clean exit, and
        //  a folder joins Explorer's list too. A path into a disk image stays
        //  in Casso Explorer's own: Explorer would open the image file.
        AddTypedPath (CassoExplorerBrowser::RootOnSystemDrive (text));
        m_window.SaveSession();

        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
        {
            WriteExplorerTypedPath (CassoExplorerBrowser::RootOnSystemDrive (text));
        }
    }
    else
    {
        m_window.ShowMessage (L"Casso Explorer can't find \"" + text + L"\". Check the path and try again.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::AddTypedPath
//
//  Every window shares the preferences' typed paths, so a path goes on top of
//  those rather than replacing them with this window's list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::AddTypedPath (const std::wstring & text)
{
    TypedPathHistory  shared;



    shared.Reset (m_window.m_prefs.typedPaths);
    shared.Add (text);
    m_window.m_prefs.typedPaths = shared.GetEntries();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressMenu
//
//  The folders and images inside a segment's location, hung from the
//  separator after it, as Explorer's address bar lists a folder's children.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressMenu (int index, const RECT & anchor)
{
    std::vector<BrowserModel::AddressSegment>  children;
    std::vector<DxuiPopupMenuItem>             items;



    if (index < 0 || index >= (int) m_window.m_addressSegments.size())
    {
        return;
    }

    m_window.m_browser.GetFolderChildren (m_window.m_addressSegments[(size_t) index].location, children);
    m_window.m_menuCommands.clear();

    for (const BrowserModel::AddressSegment & child : children)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = child.location;

        command->label = EscapeMnemonics (child.label);

        command->dispatch = [this, target]()
        {
            m_window.m_browser.NavigateToLocation (target);
            m_window.m_listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        m_window.m_address->SetOpenSeparator (index);
        DxuiContextMenu::ShowUnder (*m_window.GetPopupHost(), anchor, std::move (items), [this] (bool)
        {
            m_window.m_address->SetOpenSeparator (-1);
            m_window.Invalidate();
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressHistoryMenu
//
//  The paths typed into the bar, newest first.
//
//  EACH PICK GOES BACK THROUGH THE TEXT, not through the location it parses
//  to. The history records a path where a typed navigation succeeds, so
//  submitting the text is what moves the pick to the top of the list; handing
//  the parsed location straight to the browser would navigate correctly and
//  silently leave the order alone.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressHistoryMenu (const RECT & anchor)
{
    UNREFERENCED_PARAMETER (anchor);

    //  The caret opens the history and, pressed again, closes it.
    if (m_window.m_suggest.IsOpen())
    {
        m_window.m_suggest.Close();
        return;
    }

    ShowAddressSuggestions (std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressSuggestions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressSuggestions (const std::wstring & typed)
{
    m_window.m_suggest.Show (m_window.m_address->GetBounds(), typed.empty() ? GetAddressHistory() : GetCompletions (typed));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::OnAddressKey
//
//  While the list shows, Up and Down walk it, the box showing the row walked
//  to, or what was typed once the walk leaves the list; Escape closes it and
//  leaves the edit open. Enter goes to whatever the box shows.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::OnAddressKey (WPARAM vk)
{
    std::wstring  shown;



    if (!m_window.m_suggest.IsOpen())
    {
        return false;
    }

    if (vk == VK_DOWN || vk == VK_UP)
    {
        shown = m_window.m_suggest.MoveHighlight ((vk == VK_DOWN) ? 1 : -1);
        m_window.m_address->SetEditText (shown.empty() ? m_window.m_addressTyped : shown);
        m_window.Invalidate();
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        m_window.m_suggest.Close();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetAddressHistory
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::GetAddressHistory() const
{
    return TypedPathHistory::Merge (ReadExplorerTypedPaths(), m_window.m_prefs.typedPaths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetCompletions
//
//  What is in the folder the typed path has reached, whose names start with
//  what follows its last backslash, as Explorer lists them: folders and
//  files, hidden ones left out, sorted by name.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::GetCompletions (const std::wstring & typed)
{
    std::vector<std::wstring>  out;
    std::wstring               rooted = CassoExplorerBrowser::RootOnSystemDrive (typed);
    size_t                     slash  = rooted.find_last_of (L'\\');
    std::wstring               folder;
    std::wstring               prefix;
    std::wstring               shownFolder;
    WIN32_FIND_DATAW           found  = {};
    HANDLE                     search = INVALID_HANDLE_VALUE;



    if (slash == std::wstring::npos)
    {
        return out;
    }

    folder      = rooted.substr (0, slash + 1);
    prefix      = rooted.substr (slash + 1);
    shownFolder = typed.substr (0, typed.find_last_of (L'\\') + 1);

    search = FindFirstFileExW ((folder + L"*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (search == INVALID_HANDLE_VALUE)
    {
        return out;
    }

    do
    {
        std::wstring  name = found.cFileName;

        if (name == L"." || name == L".." || (found.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0)
        {
            continue;
        }

        if (_wcsnicmp (name.c_str(), prefix.c_str(), prefix.size()) == 0)
        {
            out.push_back (shownFolder + name);
        }
    }
    while (out.size() < kMaxCompletions && FindNextFileW (search, &found));

    FindClose (search);

    std::sort (out.begin(), out.end(), [] (const std::wstring & a, const std::wstring & b) { return _wcsicmp (a.c_str(), b.c_str()) < 0; });

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressContextMenu
//
//  Explorer's: the location as text (both copies), editing it, and clearing
//  the typed history -- Explorer's own list too, since the two are shown as
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    Location                        location = m_window.m_browser.GetLocation();
    std::wstring                    address  = BrowserModel::FormatAddress (location);



    m_window.m_menuCommands.clear();

    //  Explorer's two both put the path on as text; neither puts the folder.
    m_window.m_menus.AddMenuCommand (items, L"&Copy address", [this, address]() { DxuiClipboard::SetText (m_window.GetHwnd(), address); });

    m_window.m_menus.AddMenuCommand (items, L"Copy address as &text", [this, address]() { DxuiClipboard::SetText (m_window.GetHwnd(), address); });

    m_window.m_menus.AddMenuCommand (items, L"&Edit address", [this]()
    {
        m_window.m_input.SetFocusPane (ExplorerInput::Pane::Address);
        m_window.m_address->BeginEdit();
        m_window.Invalidate();
    });

    m_window.m_menus.AddMenuCommand (items, L"&Delete history", [this]()
    {
        HKEY  key = nullptr;

        m_window.m_browser.GetTypedPaths().Clear();
        m_window.m_prefs.typedPaths.clear();

        if (RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
        {
            RegDeleteTreeW (key, nullptr);
            RegCloseKey (key);
        }
    });

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ReadExplorerTypedPaths
//
//  Explorer's typed paths, url1 the newest.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::ReadExplorerTypedPaths()
{
    std::vector<std::wstring>  out;
    HKEY                       key    = nullptr;
    LSTATUS                    status = RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_READ, &key);
    size_t                     i      = 0;



    if (status != ERROR_SUCCESS)
    {
        return out;
    }

    for (i = 1; i <= TypedPathHistory::kMaxEntries; i++)
    {
        wchar_t  value[MAX_PATH * 2] = {};
        DWORD    bytes               = sizeof (value) - sizeof (wchar_t);
        DWORD    type                = 0;

        status = RegQueryValueExW (key, std::format (L"url{}", i).c_str(), nullptr, &type, (BYTE *) value, &bytes);

        if (status == ERROR_SUCCESS && type == REG_SZ && value[0] != L'\0')
        {
            out.push_back (value);
        }
    }

    RegCloseKey (key);

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::WriteExplorerTypedPath
//
//  Puts a folder at the top of Explorer's typed paths, as Explorer does when
//  one is typed there: a repeat moves up, and the oldest past the limit goes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::WriteExplorerTypedPath (const std::wstring & path)
{
    std::vector<std::wstring>  paths  = ReadExplorerTypedPaths();
    HKEY                       key    = nullptr;
    LSTATUS                    status = ERROR_SUCCESS;
    size_t                     i      = 0;



    paths.erase (std::remove_if (paths.begin(), paths.end(),
                                 [&path] (const std::wstring & held) { return _wcsicmp (held.c_str(), path.c_str()) == 0; }),
                 paths.end());
    paths.insert (paths.begin(), path);

    if (paths.size() > TypedPathHistory::kMaxEntries)
    {
        paths.resize (TypedPathHistory::kMaxEntries);
    }

    status = RegCreateKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);

    if (status != ERROR_SUCCESS)
    {
        return;
    }

    for (i = 0; i < paths.size(); i++)
    {
        status = RegSetValueExW (key, std::format (L"url{}", i + 1).c_str(), 0, REG_SZ,
                                 (const BYTE *) paths[i].c_str(), (DWORD) ((paths[i].size() + 1) * sizeof (wchar_t)));
        IGNORE_RETURN_VALUE (status, ERROR_SUCCESS);
    }

    RegCloseKey (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressOverflowMenu
//
//  The segments collapsed behind the address bar's overflow button, nearest
//  first, as Explorer lists them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressOverflowMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    int                             i = 0;



    m_window.m_menuCommands.clear();

    for (i = (std::min) (m_window.m_address->GetFirstShown(), (int) m_window.m_addressSegments.size()) - 1; i >= 0; i--)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = m_window.m_addressSegments[(size_t) i].location;

        command->label    = EscapeMnemonics (m_window.m_addressSegments[(size_t) i].label);
        command->dispatch = [this, target]()
        {
            m_window.m_browser.NavigateToLocation (target);
            m_window.m_listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::LeaveSearch
//
//  Back to the folder a search covered: by Back when the search was run here,
//  and by going to the folder when the results tab was restored, with no
//  history to go back through.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::LeaveSearch()
{
    Location  searched;



    if (!SearchQuery::IsId (m_window.m_browser.GetLocation().path))
    {
        return;
    }

    if (!m_window.m_browser.GoBack() && m_window.m_browser.TryGetUpTarget (searched))
    {
        m_window.m_browser.NavigateToLocation (searched);
    }

    m_window.m_listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::OnFindBoxKey
//
//  Enter searches, Escape empties the box and leaves a search's results, as
//  Explorer's box does; every other key edits the query.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::OnFindBoxKey (const DxuiKeyEvent & ev)
{
    bool  down = ev.kind == DxuiKeyEventKind::Down;



    if (down && ev.vk == VK_RETURN)
    {
        SearchLocation (m_window.m_findBox->GetText());
        return true;
    }

    if (down && ev.vk == VK_ESCAPE)
    {
        m_window.m_findBox->SetText (L"");
        LeaveSearch();

        m_window.m_input.SetFocusPane (ExplorerInput::Pane::List);
        return true;
    }

    return m_window.m_findBox->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetCassoSearchScope
//
//  A search from Casso's root covers each of Casso's own folders on the host.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::GetCassoSearchScope()
{
    std::vector<TreeNode>      nodes;
    std::vector<std::wstring>  folders;
    HRESULT                    hr = m_window.m_browser.GetTreeModel().GetChildren (TreeModel::kCassoRootId, nodes);



    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const TreeNode & node : nodes)
    {
        if (node.location.kind == Location::Kind::HostFolder && !node.location.path.empty())
        {
            folders.push_back (node.location.path);
        }
    }

    return SearchQuery::JoinScopes (folders);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::SearchLocation
//
//  The folder shown and everything below it; a search from a search's
//  results searches the same folder again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::SearchLocation (const std::wstring & query)
{
    Location      at    = m_window.m_browser.GetLocation();
    std::wstring  scope;
    std::wstring  previous;



    //  Enter in an empty box goes back to the list, as File Explorer's does.
    if (query.empty())
    {
        m_window.m_input.SetFocusPane (ExplorerInput::Pane::List);
        return;
    }

    if (at.kind == Location::Kind::HostFolder)
    {
        scope = at.path;
    }
    else if (SearchQuery::IsId (at.path))
    {
        SearchQuery::TryParseId (at.path, scope, previous);
    }
    else if (at.kind == Location::Kind::ShellFolder && !at.path.empty() && !Location::IsShellName (at.path))
    {
        scope = at.path;
    }
    else if (at.kind == Location::Kind::Root && at.path == TreeModel::kCassoRootId)
    {
        scope = GetCassoSearchScope();
    }

    if (scope.empty())
    {
        m_window.ShowMessage (L"Search covers folders on a disk; this location has none.", MB_ICONINFORMATION);
        return;
    }

    m_window.m_browser.NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (scope, query), SearchQuery::GetLabel (scope)));
    m_window.m_listPane.FillList();
    m_window.m_input.SetFocusPane (ExplorerInput::Pane::List);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressRootsMenu
//
//  The chevron after the location's icon: the desktop's roots, then the
//  folders on the user's desktop, each with its icon, as Explorer's has.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressRootsMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>                 items;
    std::vector<IShellItemVerbs::ShellFolderItem>  folders;
    HRESULT                                        hr = m_window.m_shellVerbs.ListDesktopFolders (folders);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_window.m_menuCommands.clear();

    for (const IShellItemVerbs::ShellFolderItem & folder : folders)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        std::wstring                  target  = folder.path.empty() ? folder.id : folder.path;

        //  This PC and the Recycle Bin open as Casso Explorer's own, with
        //  their drives and their Restore.
        Location  own = folder.isRecycleBin ? Location::MakeRecycleBin()
                      : folder.isThisPc     ? Location::MakeRoot (TreeModel::kThisPcRootId)
                                            : Location();

        command->label     = EscapeMnemonics (folder.name);
        command->menuImage = m_window.m_shellIcons.GetForPath (target, true);
        command->dispatch  = [this, target, own]()
        {
            if (own.kind != Location::Kind::None)
            {
                m_window.m_browser.NavigateToLocation (own);
            }
            else
            {
                m_window.m_browser.NavigateToAddress (target);
            }

            m_window.m_listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowHistoryMenu
//
//  Where Back or Forward would go, nearest first; picking one takes as many
//  steps as it is away.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowHistoryMenu (bool forward, const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::vector<Location>           stack;
    size_t                          i     = 0;



    if (!m_window.m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    stack = forward ? m_window.m_browser.GetBrowserModel().GetActiveTab().forward
                    : m_window.m_browser.GetBrowserModel().GetActiveTab().back;

    m_window.m_menuCommands.clear();

    for (i = 0; i < stack.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        size_t                        steps   = i + 1;

        command->label    = EscapeMnemonics (CassoExplorerBrowser::GetLocationLabel (stack[stack.size() - 1 - i]));
        command->dispatch = [this, forward, steps]()
        {
            if (forward ? m_window.m_browser.GoForwardBy (steps) : m_window.m_browser.GoBackBy (steps))
            {
                m_window.m_listPane.FillList();
            }
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::EscapeMnemonics
//
//  A menu label reads an ampersand as a mnemonic marker, so a name's own is
//  doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::EscapeMnemonics (const std::wstring & text)
{
    std::wstring  escaped;



    for (wchar_t ch : text)
    {
        escaped += (ch == L'&') ? L"&&" : std::wstring (1, ch);
    }

    return escaped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetProfileRoot
//
//  The user's profile folder and the name the shell gives it, which is the
//  user's own name; empty when the shell will not say.
//
////////////////////////////////////////////////////////////////////////////////

BrowserModel::AddressRoot ExplorerAddressBar::GetProfileRoot()
{
    BrowserModel::AddressRoot  root;
    IShellItem               * item = nullptr;
    PWSTR                      path = nullptr;
    PWSTR                      name = nullptr;
    HRESULT                    hr   = S_OK;



    //  The profile as the shell's namespace holds it, where its display name
    //  is the user's name. The item for its file system path is displayed as
    //  the folder's own name instead.
    hr = SHCreateItemFromParsingName (L"shell:UsersFilesFolder", nullptr, IID_PPV_ARGS (&item));

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_FILESYSPATH, &path);
    }

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_NORMALDISPLAY, &name);
    }

    if (SUCCEEDED (hr))
    {
        root.path  = path;
        root.label = name;
    }

    CoTaskMemFree (name);
    CoTaskMemFree (path);

    if (item != nullptr)
    {
        item->Release();
    }

    return root;
}
