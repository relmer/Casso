#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderWatch.h"


struct ExplorerContext;





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
    explicit ExplorerFolderSync (ExplorerContext & context);

    //  How often the drives are polled for one attached or removed.
    static constexpr UINT  kDrivePollMs = 2000;

    //  File Explorer's Quick access, pinned and frequent folders alike, is the
    //  jump list of this application id, in the Recent folder's
    //  AutomaticDestinations.
    static constexpr const wchar_t *  kQuickAccessFile = L"f01b4d95cf55d32a.automaticDestinations-ms";
    static constexpr const wchar_t *  kRecentItemsFile = L"5f7b5f1e01b83767.automaticDestinations-ms";

    void            StartWatch                  ();
    void            StartPolling                ();
    bool            IsWatching                  () const { return m_folderWatch != nullptr; }
    void            UpdateWatchedFolders        ();
    void            RefreshChangedFolders       ();
    bool            IsShownFolderIn             (const std::vector<std::wstring> & folders) const;
    bool            RefreshOpenTreeFolders      (const std::vector<std::wstring> & folders);
    static bool     IsSameFolder                (const std::wstring & a, const std::wstring & b);
    void            RefreshAfterHostChange      ();
    void            ReloadKnownFolders          ();
    void            ReloadKnownFoldersIfChanged ();
    void            RefreshQuickAccessIfChanged ();
    static int64_t  GetQuickAccessStamp         (const std::wstring & directory);
    static int64_t  GetFileStamp                (const std::wstring & path);
    void            CheckDrives                 ();
    void            ChangeKnownFolder           (const std::wstring & folder, bool add);

    void  HandleFolderChanged ();
    void  HandleSettleTimer   ();

private:
    ExplorerContext               & m_context;
    std::unique_ptr<FolderWatch>    m_folderWatch;
    ULONGLONG                       m_firstChangeMs    = 0;   // 0: no change waiting
    DWORD                           m_driveMask        = 0;   // GetLogicalDrives at the last look
    std::wstring                    m_quickAccessDir;   // where File Explorer keeps Quick access
    int64_t                         m_quickAccessStamp = 0;   // its file's last write at the last look
    int64_t                         m_recentStamp      = 0;
    std::string                     m_knownFoldersText;   // KnownFolders.json as last read
};
