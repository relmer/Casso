#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/RefreshAnchor.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "Core/AppTimerIds.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::ExplorerFolderSync
//
////////////////////////////////////////////////////////////////////////////////

ExplorerFolderSync::ExplorerFolderSync (ExplorerContext & context)
    : m_context (context)
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
    HRESULT  hr = m_context.browser.Reload (true);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_context.listPane.FillList();
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



    if (m_context.app.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_context.app.fs, m_context.app.baseDir);

    hr = add ? store.Append (folder, (int64_t) _time64 (nullptr)) : store.Remove (folder);

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"Casso's folder list could not be changed.", MB_ICONERROR);
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



    if (m_context.app.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_context.app.fs, m_context.app.baseDir);

    hr = store.RemoveMissing (removed);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = store.Load (entries);
    IGNORE_RETURN_VALUE (hr, S_OK);

    folders = KnownFolderStore::ListRootFolders (*m_context.app.fs, m_context.app.baseDir, entries);

    m_context.browser.GetTreeModel().SetKnownFolders (folders);
    m_context.browser.GetTreeModel().Invalidate (TreeModel::kCassoRootId);
    m_context.treePane.ReloadChildren (TreeModel::kCassoRootId);
    UpdateWatchedFolders();
    m_context.window.Invalidate();

    hr = m_context.app.fs->ReadAllText (KnownFolderStore::GetFilePath (m_context.app.baseDir), m_knownFoldersText);
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



    if (m_context.app.fs == nullptr)
    {
        return;
    }

    hr = m_context.app.fs->ReadAllText (KnownFolderStore::GetFilePath (m_context.app.baseDir), text);
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



    if (m_quickAccessDir.empty())
    {
        return;
    }

    stamp = GetQuickAccessStamp (m_quickAccessDir);

    if (stamp != m_quickAccessStamp)
    {
        m_quickAccessStamp = stamp;
        m_context.browser.RefreshShellRoots();
        m_context.treePane.RefreshTree();
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



    if (mask == m_driveMask)
    {
        return;
    }

    m_driveMask = mask;

    m_context.browser.GetTreeModel().SetDrives (CassoExplorerShell::GetDriveRoots());
    m_context.browser.GetTreeModel().Invalidate (TreeModel::kThisPcRootId);
    m_context.treePane.ReloadChildren (TreeModel::kThisPcRootId);
    ReloadKnownFolders();

    if (!m_context.browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    showing = m_context.browser.GetLocation();

    if (showing.kind == Location::Kind::Root && showing.path == TreeModel::kThisPcRootId)
    {
        m_context.browser.GetBrowserModel().InvalidateAllCatalogs();
        hr = m_context.browser.Refresh();
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_context.listPane.FillList();
    }
    else if (m_context.browser.LeaveMissingLocation())
    {
        m_context.listPane.FillList();
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
//  The host folders worth watching: the one the list is showing and every
//  one the tree has open. Cheap enough to call after anything that could
//  have changed either.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::UpdateWatchedFolders()
{
    std::vector<std::wstring>  folders;
    Location                   showing = m_context.browser.GetLocation();
    int                        row     = 0;



    if (m_folderWatch == nullptr)
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

    for (row = 0; row < m_context.treePane.GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_context.treePane.GetNodeAt (row);
        Location              at;

        if (node != nullptr && node->expanded
            && m_context.browser.TryGetNodeLocation (node->id, at)
            && at.kind == Location::Kind::HostFolder && !at.path.empty())
        {
            folders.push_back (at.path);
        }
    }

    //  Casso's known folders file, which Casso writes while this window is open,
    //  and File Explorer's Quick access, which a pin made there changes.
    if (!m_context.app.baseDir.empty())
    {
        folders.push_back (m_context.app.baseDir);
    }

    if (!m_quickAccessDir.empty())
    {
        folders.push_back (m_quickAccessDir);
    }

    m_folderWatch->SetWatched (folders);
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
    Location      showing = m_context.browser.GetLocation();
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



    for (row = 0; row < m_context.treePane.GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_context.treePane.GetNodeAt (row);
        Location              at;

        //  A closed folder is read again too, so its first subfolder gives it
        //  a chevron.
        if (node != nullptr
            && m_context.browser.TryGetNodeLocation (node->id, at) && !at.path.empty()
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
        if (m_context.treePane.HasNode (id))
        {
            m_context.browser.GetTreeModel().Invalidate (id);
            m_context.treePane.ReloadChildren (id);
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
//  Re-reads whatever the watcher reported, once the burst has settled.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::RefreshChangedFolders()
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  changed;
    std::vector<std::wstring>  keys;
    RefreshAnchor::Before      before;
    RefreshAnchor::After       after;



    if (m_folderWatch == nullptr)
    {
        return;
    }

    //  Not while a rename is open or a button is down: the rows under either
    //  would change. The changes wait, and the timer tries again.
    if (m_context.fileOps.IsRenaming() || m_context.listPane.IsBusy() || m_context.treePane.IsInteracting())
    {
        SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerFolder, CassoExplorerWindow::kFolderSettleMs, nullptr);
        return;
    }

    if (!m_folderWatch->TakeChanged (changed))
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
    if (m_context.browser.GetBrowserModel().HasTabs() && m_context.browser.LeaveMissingLocation())
    {
        m_context.listPane.FillList();
        return;
    }

    if (!m_context.browser.GetBrowserModel().HasTabs() || !IsShownFolderIn (changed))
    {
        m_context.window.Invalidate();
        return;
    }

    //  The view as it stands, in keys, before the rows under it change.
    m_context.browser.GetRowKeys (before.keys);
    m_context.listPane.CaptureView (before);

    //  Re-read without the browser's own restore: the anchor decides both the
    //  selection and where the view lands.
    hr = m_context.browser.Reload (false);
    IGNORE_RETURN_VALUE (hr, S_OK);
    m_context.listPane.FillList();

    m_context.browser.GetRowKeys (keys);
    after = RefreshAnchor::Compute (before, keys);

    m_context.browser.SetSelectedRows (after.selected);
    m_context.listPane.RestoreView (after);

    m_context.preview.FillPreview();
    m_context.statusBar.Fill();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::StartWatch
//
//  Posting is all that happens on the watcher's thread; the settle timer and
//  the re-read run here.
//
//  The watch on the folders on display, and the polls for drives and
//  Quick access; a change posted by the watch, and its settle timer.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::StartWatch()
{
    if (m_context.app.watcher == nullptr)
    {
        return;
    }

    m_folderWatch = std::make_unique<FolderWatch> (*m_context.app.watcher);

    m_folderWatch->SetOnChanged ([this] ()
    {
        PostMessageW (m_context.window.GetHwnd(), (UINT) ExplorerMessage::FolderChanged, 0, 0);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::StartPolling
//
//  The drives attached at the start, polled for others, and the folder of
//  File Explorer's Quick access, with its file's last write.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::StartPolling()
{
    PWSTR    recent = nullptr;
    HRESULT  hr     = S_OK;



    m_driveMask = GetLogicalDrives();
    SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerDrive, kDrivePollMs, nullptr);

    hr = SHGetKnownFolderPath (FOLDERID_Recent, 0, nullptr, &recent);

    if (SUCCEEDED (hr))
    {
        m_quickAccessDir   = std::wstring (recent) + L"\\AutomaticDestinations";
        m_quickAccessStamp = GetQuickAccessStamp (m_quickAccessDir);
    }

    CoTaskMemFree (recent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::HandleFolderChanged
//
//  A burst of changes settles before a re-read, for no longer than
//  kFolderMaxSettleMs from its first.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::HandleFolderChanged()
{
    ULONGLONG  now = GetTickCount64();



    if (m_firstChangeMs == 0)
    {
        m_firstChangeMs = now;
    }

    if (now - m_firstChangeMs < CassoExplorerWindow::kFolderMaxSettleMs)
    {
        SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerFolder, CassoExplorerWindow::kFolderSettleMs, nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync::HandleSettleTimer
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFolderSync::HandleSettleTimer()
{
    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerFolder);
    m_firstChangeMs = 0;
    RefreshChangedFolders();
}