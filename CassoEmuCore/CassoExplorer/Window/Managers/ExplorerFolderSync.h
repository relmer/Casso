#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderWatch.h"
#include "Seams/Win32DeviceChangeWatch.h"


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

    //  How often the drive letters are compared for one attached or removed.
    //  The comparison doesn't read a device. Windows sends a notification
    //  for a card or a reader attached or removed, and the cards are listed
    //  again kDeviceSoonMs and kDeviceLateMs after it, since a device just
    //  attached may not be readable yet.
    static constexpr UINT  kDrivePollMs   = 2000;
    static constexpr UINT  kDeviceSoonMs  = 500;
    static constexpr UINT  kDeviceLateMs  = 3000;

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
    void            RefreshAfterHostChange      (const std::wstring & select = std::wstring());
    void            ReloadKnownFolders          ();
    void            ReloadKnownFoldersIfChanged ();
    void            RefreshQuickAccessIfChanged ();
    static int64_t  GetQuickAccessStamp         (const std::wstring & directory);
    static int64_t  GetFileStamp                (const std::wstring & path);
    void            CheckDrives                 ();

    //  The timer's look: the drives and cards listed again only when the
    //  drive letters changed. And a device attached or removed, which lists
    //  them again shortly after.
    void            PollDrives                  ();
    void            HandleDeviceChange          ();
    void            HandleDeviceTimer           (UINT_PTR timerId);

    //  For a list just filled: a storage card that opened for it has its
    //  partitions listed in the tree at once, rather than at the next poll.
    void            CheckOpenedCard             ();
    void            ChangeKnownFolder           (const std::wstring & folder, bool add);

    void  HandleFolderChanged ();
    void  HandleSettleTimer   ();

private:
    bool  UpdateStorageCards ();

    ExplorerContext               & m_context;
    std::unique_ptr<FolderWatch>    m_folderWatch;
    Win32DeviceChangeWatch          m_deviceWatch;
    ULONGLONG                       m_firstChangeMs    = 0;   // 0: no change waiting
    DWORD                           m_driveMask        = 0;   // GetLogicalDrives at the last look
    std::wstring                    m_quickAccessDir;   // where File Explorer keeps Quick access
    int64_t                         m_quickAccessStamp = 0;   // its file's last write at the last look
    int64_t                         m_recentStamp      = 0;
    std::string                     m_knownFoldersText;   // KnownFolders.json as last read
    std::wstring                    m_storageCardKey;     // the storage cards at the last look, as text
    std::vector<int>                m_storageCardDrives;  // their drive numbers
    std::vector<int>                m_openCardDrives;     // the cards open at the last look
    std::vector<std::wstring>       m_expandIds;          // card nodes to open once the tree has them
};
