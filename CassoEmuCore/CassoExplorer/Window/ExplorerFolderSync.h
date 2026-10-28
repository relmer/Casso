#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderWatch.h"


class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFolderSync
//
//  What changes outside the window: the host folders watched and read
//  again once a burst of changes settles, the known folders, Quick
//  access, and drives attached or removed.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerFolderSync
{
public:
    explicit ExplorerFolderSync (CassoExplorerWindow & window);

    //  How often the drives are polled for one attached or removed.
    static constexpr UINT  kDrivePollMs = 2000;

    //  The watch on the folders on display, and the polls for drives and
    //  Quick access; a change posted by the watch, and its settle timer.
    void  StartWatch    ();
    void  StartPolling  ();
    void  OnFolderChanged ();
    void  OnSettleTimer ();
    bool  IsWatching    () const { return m_folderWatch != nullptr; }

    //  File Explorer's Quick access, pinned and frequent folders alike, is the
    //  jump list of this application id, in the Recent folder's
    //  AutomaticDestinations.
    static constexpr const wchar_t *  kQuickAccessFile = L"f01b4d95cf55d32a.automaticDestinations-ms";

    //  The host folders worth watching: the one the list is showing and every
    //  one the tree has open. Cheap enough to call after anything that could
    //  have changed either.
    void  UpdateWatchedFolders ();

    //  Re-reads whatever the watcher reported, once the burst has settled.
    void  RefreshChangedFolders ();
    bool  IsShownFolderIn       (const std::vector<std::wstring> & folders) const;
    bool  RefreshOpenTreeFolders (const std::vector<std::wstring> & folders);
    static bool  IsSameFolder   (const std::wstring & a, const std::wstring & b);
    void  RefreshAfterHostChange ();
    void  ReloadKnownFolders     ();
    void  ReloadKnownFoldersIfChanged ();
    void  RefreshQuickAccessIfChanged ();
    static int64_t  GetQuickAccessStamp (const std::wstring & directory);
    void  CheckDrives            ();
    void  ChangeKnownFolder   (const std::wstring & folder, bool add);

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    std::unique_ptr<FolderWatch>  m_folderWatch;
    ULONGLONG                     m_firstChangeMs    = 0;   // 0: no change waiting
    DWORD                         m_driveMask        = 0;   // GetLogicalDrives at the last look
    std::wstring                  m_quickAccessDir;         // where File Explorer keeps Quick access
    int64_t                       m_quickAccessStamp = 0;   // its file's last write at the last look

    std::string                               m_knownFoldersText;          // KnownFolders.json as last read
};
