#pragma once

#include "Pch.h"


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

    std::string                               m_knownFoldersText;          // KnownFolders.json as last read
};
