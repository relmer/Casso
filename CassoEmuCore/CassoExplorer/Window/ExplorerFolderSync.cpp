#include "Pch.h"

#include "CassoExplorer/Window/ExplorerFolderSync.h"
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
//  ExplorerFolderSync::ExplorerFolderSync
//
////////////////////////////////////////////////////////////////////////////////

ExplorerFolderSync::ExplorerFolderSync (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::RefreshAfterHostChange
//
//  The folder watcher sees the change too, a moment later; rereading now
//  shows it at once.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::RefreshAfterHostChange()
{
    HRESULT  hr = m_window.m_browser.Reload (true);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_window.m_listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::ChangeKnownFolder
//
//  The shared file is changed under its lock, then read back whole, so a
//  folder Casso recorded in the meantime shows too.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::ChangeKnownFolder (const std::wstring & folder, bool add)
{
    HRESULT                               hr = S_OK;



    if (m_window.m_context.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_window.m_context.fs, m_window.m_context.baseDir);

    hr = add ? store.Append (folder, (int64_t) _time64 (nullptr)) : store.Remove (folder);

    if (FAILED (hr))
    {
        m_window.ShowMessage (L"Casso's folder list could not be changed.", MB_ICONERROR);
        return;
    }

    ReloadKnownFolders();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::ReloadKnownFolders
//
//  Casso's root lists the folders on record, less any that are gone, which
//  are dropped from the record as they are noticed.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::ReloadKnownFolders()
{
    HRESULT                               hr      = S_OK;
    std::vector<KnownFolderStore::Entry>  entries;
    std::vector<std::wstring>             folders;
    bool                                  removed = false;



    if (m_window.m_context.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_window.m_context.fs, m_window.m_context.baseDir);

    hr = store.RemoveMissing (removed);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = store.Load (entries);
    IGNORE_RETURN_VALUE (hr, S_OK);

    folders = KnownFolderStore::ListRootFolders (*m_window.m_context.fs, m_window.m_context.baseDir, entries);

    m_window.m_browser.GetTreeModel().SetKnownFolders (folders);
    m_window.m_browser.GetTreeModel().Invalidate (TreeModel::kCassoRootId);
    m_window.m_tree->ReplaceChildren (TreeModel::kCassoRootId, m_window.m_browser.GetTreeChildren (TreeModel::kCassoRootId));
    UpdateWatchedFolders();
    m_window.Invalidate();

    hr = m_window.m_context.fs->ReadAllText (KnownFolderStore::GetFilePath (m_window.m_context.baseDir), m_knownFoldersText);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::ReloadKnownFoldersIfChanged
//
//  Casso writes its known folders while this window is open. The folder the
//  file is in also has this window's own prefs, so the file's text is compared
//  before the tree is rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::ReloadKnownFoldersIfChanged()
{
    HRESULT      hr   = S_OK;
    std::string  text;



    if (m_window.m_context.fs == nullptr)
    {
        return;
    }

    hr = m_window.m_context.fs->ReadAllText (KnownFolderStore::GetFilePath (m_window.m_context.baseDir), text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (text != m_knownFoldersText)
    {
        ReloadKnownFolders();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::RefreshQuickAccessIfChanged
//
//  Every program's jump list is written to the same folder, so only a change
//  to Quick access's own file reads the tree's roots again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::RefreshQuickAccessIfChanged()
{
    int64_t  stamp = 0;



    if (m_window.m_quickAccessDir.empty())
    {
        return;
    }

    stamp = GetQuickAccessStamp (m_window.m_quickAccessDir);

    if (stamp != m_window.m_quickAccessStamp)
    {
        m_window.m_quickAccessStamp = stamp;
        m_window.m_browser.RefreshShellRoots();
        m_window.m_listPane.RefreshTree();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::GetQuickAccessStamp
//
//  Quick access's file's last write, or zero where it cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

int64_t ExplorerFolderSync::GetQuickAccessStamp (const std::wstring & directory)
{
    WIN32_FILE_ATTRIBUTE_DATA  data  = {};
    ULARGE_INTEGER             when  = {};



    if (!GetFileAttributesExW ((directory + L"\\" + kQuickAccessFile).c_str(), GetFileExInfoStandard, &data))
    {
        return 0;
    }

    when.LowPart  = data.ftLastWriteTime.dwLowDateTime;
    when.HighPart = data.ftLastWriteTime.dwHighDateTime;

    return (int64_t) when.QuadPart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::CheckDrives
//
//  A drive attached or removed while the window is open changes This PC, and
//  can bring back or take away a known folder.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::CheckDrives()
{
    HRESULT   hr   = S_OK;
    DWORD     mask = GetLogicalDrives();
    Location  showing;



    if (mask == m_window.m_driveMask)
    {
        return;
    }

    m_window.m_driveMask = mask;

    m_window.m_browser.GetTreeModel().SetDrives (CassoExplorerShell::GetDriveRoots());
    m_window.m_browser.GetTreeModel().Invalidate (TreeModel::kThisPcRootId);
    m_window.m_tree->ReplaceChildren (TreeModel::kThisPcRootId, m_window.m_browser.GetTreeChildren (TreeModel::kThisPcRootId));
    ReloadKnownFolders();

    if (!m_window.m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    showing = m_window.m_browser.GetLocation();

    if (showing.kind == Location::Kind::Root && showing.path == TreeModel::kThisPcRootId)
    {
        m_window.m_browser.GetBrowserModel().InvalidateAllCatalogs();
        hr = m_window.m_browser.Refresh();
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_window.m_listPane.FillList();
    }
    else if (m_window.m_browser.LeaveMissingLocation())
    {
        m_window.m_listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::UpdateWatchedFolders
//
//  The folder the list is showing -- for an image, the folder holding it --
//  and every host folder the tree has open, since those are the folders whose
//  contents are on screen.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::UpdateWatchedFolders()
{
    std::vector<std::wstring>  folders;
    Location                   showing = m_window.m_browser.GetLocation();
    int                        row     = 0;



    if (m_window.m_folderWatch == nullptr)
    {
        return;
    }

    //  The shown folder's parent too: deleting the folder is a change there.
    if (showing.kind == Location::Kind::HostFolder && !showing.path.empty())
    {
        folders.push_back (showing.path);

        if (showing.path.size() > 3)
        {
            folders.push_back (CassoExplorerBrowser::GetParentFolder (showing.path));
        }
    }
    else if (!showing.path.empty())
    {
        folders.push_back (CassoExplorerBrowser::GetParentFolder (showing.path));
    }

    for (row = 0; row < m_window.m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_window.m_tree->GetNodeAt (row);
        Location              at;

        if (node != nullptr && node->expanded
            && m_window.m_browser.TryGetNodeLocation (node->id, at)
            && at.kind == Location::Kind::HostFolder && !at.path.empty())
        {
            folders.push_back (at.path);
        }
    }

    //  Casso's known folders file, which Casso writes while this window is open,
    //  and File Explorer's Quick access, which a pin made there changes.
    if (!m_window.m_context.baseDir.empty())
    {
        folders.push_back (m_window.m_context.baseDir);
    }

    if (!m_window.m_quickAccessDir.empty())
    {
        folders.push_back (m_window.m_quickAccessDir);
    }

    m_window.m_folderWatch->SetWatched (folders);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::IsSameFolder
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFolderSync::IsSameFolder (const std::wstring & a, const std::wstring & b)
{
    return CompareStringOrdinal (a.c_str(), (int) a.size(), b.c_str(), (int) b.size(), TRUE) == CSTR_EQUAL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::IsShownFolderIn
//
//  Whether the list shows one of the folders: the folder itself, or, inside a
//  disk image, the folder that holds the image.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFolderSync::IsShownFolderIn (const std::vector<std::wstring> & folders) const
{
    Location      showing = m_window.m_browser.GetLocation();
    std::wstring  shown;



    if (showing.path.empty())
    {
        return false;
    }

    shown = (showing.kind == Location::Kind::HostFolder) ? showing.path : CassoExplorerBrowser::GetParentFolder (showing.path);

    return std::any_of (folders.begin(), folders.end(), [&] (const std::wstring & folder) { return IsSameFolder (folder, shown); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::RefreshOpenTreeFolders
//
//  Reads again, in place, what the tree shows under each open node the changed
//  folders hold: a folder's subfolders, or a disk image's. Returns whether any
//  node was read again.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFolderSync::RefreshOpenTreeFolders (const std::vector<std::wstring> & folders)
{
    std::vector<std::wstring>  stale;
    int                        row = 0;



    for (row = 0; row < m_window.m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_window.m_tree->GetNodeAt (row);
        Location              at;

        //  A closed folder is read again too, so its first subfolder gives it
        //  a chevron.
        if (node != nullptr
            && m_window.m_browser.TryGetNodeLocation (node->id, at) && !at.path.empty()
            && (node->expanded || at.kind == Location::Kind::HostFolder))
        {
            std::wstring  holder = (at.kind == Location::Kind::HostFolder) ? at.path : CassoExplorerBrowser::GetParentFolder (at.path);

            if (std::any_of (folders.begin(), folders.end(), [&] (const std::wstring & folder) { return IsSameFolder (folder, holder); }))
            {
                stale.push_back (node->id);
            }
        }
    }

    //  In row order, so a parent goes first; a node its parent's change took
    //  away has no row by the time its turn comes, and is passed over.
    for (const std::wstring & id : stale)
    {
        if (m_window.m_tree->FindRowById (id) >= 0)
        {
            m_window.m_browser.GetTreeModel().Invalidate (id);
            m_window.m_tree->ReplaceChildren (id, m_window.m_browser.GetTreeChildren (id));
        }
    }

    return !stale.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::RefreshChangedFolders
//
//  Runs once a burst has settled. The same files stay selected, and the view
//  stays on the same files, by the rules RefreshAnchor sets out: a file added
//  or removed elsewhere in the folder is no reason to lose the selection or to
//  move what is on screen.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::RefreshChangedFolders()
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  changed;
    std::vector<std::wstring>  keys;
    RefreshAnchor::Before      before;
    RefreshAnchor::After       after;



    if (m_window.m_folderWatch == nullptr)
    {
        return;
    }

    //  Not while a rename is open or a button is down: the rows under either
    //  would change. The changes wait, and the timer tries again.
    if (m_window.IsRenaming() || m_window.m_dragArmed || m_window.m_list->IsInteracting() || m_window.m_tree->IsInteracting())
    {
        SetTimer (m_window.GetHwnd(), CassoExplorerWindow::kFolderTimerId, CassoExplorerWindow::kFolderSettleMs, nullptr);
        return;
    }

    if (!m_window.m_folderWatch->TakeChanged (changed))
    {
        return;
    }

    ReloadKnownFoldersIfChanged();
    RefreshQuickAccessIfChanged();

    //  A folder written to often -- a disk image Casso has mounted, say --
    //  must not cost the tree or the list anything when neither shows it.
    if (RefreshOpenTreeFolders (changed))
    {
        UpdateWatchedFolders();
    }

    //  The folder shown was deleted: the change is its parent's, so the list
    //  moves up, as Explorer's does, rather than staying on what is gone.
    if (m_window.m_browser.GetBrowserModel().HasTabs() && m_window.m_browser.LeaveMissingLocation())
    {
        m_window.m_listPane.FillList();
        return;
    }

    if (!m_window.m_browser.GetBrowserModel().HasTabs() || !IsShownFolderIn (changed))
    {
        m_window.Invalidate();
        return;
    }

    //  The view as it stands, in keys, before the rows under it change.
    m_window.m_browser.GetRowKeys (before.keys);
    before.topRow   = m_window.m_list->GetTopRow();
    before.capacity = m_window.m_list->GetVisibleRowCapacity();
    before.selected = m_window.m_list->GetSelectedRows();

    //  The focused item counts only as part of the selection it belongs to.
    before.focused  = m_window.m_list->IsRowSelected (m_window.m_list->GetSelectedRow()) ? m_window.m_list->GetSelectedRow() : -1;

    //  Re-read without the browser's own restore: the anchor decides both the
    //  selection and where the view lands.
    hr = m_window.m_browser.Reload (false);
    IGNORE_RETURN_VALUE (hr, S_OK);
    m_window.m_listPane.FillList();

    m_window.m_browser.GetRowKeys (keys);
    after = RefreshAnchor::Compute (before, keys);

    m_window.m_browser.SetSelectedRows (after.selected);
    m_window.m_list->SetSelectedRows   (after.selected, after.focused);

    //  Last, since restoring the selection scrolls it into view. By whole rows,
    //  so an unmoved top leaves the view as it was, and rows a page down or
    //  the list's end lifted stay lifted.
    m_window.m_list->MoveTopRow (after.topRow);

    m_window.m_previewPane.FillPreview();
    m_window.m_layout.FillStatus();
    m_window.Invalidate();
}
