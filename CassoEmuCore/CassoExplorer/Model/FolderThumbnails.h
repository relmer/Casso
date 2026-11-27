#pragma once

#include "Pch.h"

#include "Seams/IFolderThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails
//
//  IFolderThumbnails over a thread that lists each folder to sweep and
//  queues each floppy image in it with no thumbnail. The folders open in a
//  window's tabs are swept first, without their subfolders. The Casso
//  node's folders are swept with their subfolders, kSettleMs after the
//  start so a launch isn't slowed by boots, or at once after
//  StartKnownFolders.
//
////////////////////////////////////////////////////////////////////////////////

class FolderThumbnails : public IFolderThumbnails
{
public:
    struct ImageFile
    {
        std::wstring  path;
        uint64_t      bytes = 0;
        int64_t       time  = 0;
    };

    //  A folder to list, or with files, those files alone.
    struct FolderRequest
    {
        std::wstring               folder;
        bool                       withSubfolders = false;
        std::vector<std::wstring>  files;
    };

    struct Seams
    {
        std::function<std::vector<ImageFile> (const FolderRequest & request)>  list;
        std::function<bool (const std::wstring & path)>                        hasThumbnail;
        std::function<bool (const ImageFile & image)>                          markQueued;    // false for an image queued before at its size and time
        std::function<void (const std::wstring & path)>                        queue;
        std::function<void()>                                                  removeQueued;  // the images waiting for a sweep
        std::function<bool()>                                                  isEnabled;
    };

    static constexpr int        kActiveRank = 0;
    static constexpr int        kTabRank    = 1;
    static constexpr int        kOtherRank  = 2;
    static constexpr ULONGLONG  kSettleMs   = 30000;

    FolderThumbnails (Seams seams, ULONGLONG startMs);
    ~FolderThumbnails() override;

    FolderThumbnails (const FolderThumbnails &)             = delete;
    FolderThumbnails & operator= (const FolderThumbnails &) = delete;

    //  Starts the thread that sweeps the folders.
    void  Start ();

    void  SetWindowTabs     (HWND window, const ThumbnailTabs & tabs) override;
    void  RemoveWindow      (HWND window) override;
    void  SweepFolder       (const std::wstring & folder) override;
    void  SetKnownFolders   (const std::vector<std::wstring> & folders) override;
    void  SetCassoImages    (const std::vector<std::wstring> & paths) override;
    void  StartKnownFolders () override;
    void  ApplySetting      () override;

    //  kActiveRank for a folder open in an active tab, or with one of the
    //  Casso node's images or under one of its folders, with the node in an
    //  active tab; kTabRank for the same in another tab; kOtherRank for any
    //  other folder. Called on the boot worker's thread.
    int   GetRank (const std::wstring & folder) const;

    //  The next folder to sweep, removed: a folder open in a tab, then the
    //  Casso node's folders once nowMs is kSettleMs past the start. False
    //  for none, or with the setting off. The thread's, and a test's.
    bool  TryTakeFolder (ULONGLONG nowMs, FolderRequest & outRequest);

    //  Each floppy image in the folder with no thumbnail, passed to the
    //  queue function. The thread's, and a test's.
    void  Sweep (const FolderRequest & request);

    //  The files in a host folder, and with withSubfolders, in each of its
    //  subfolders that isn't hidden or a system folder; or the request's
    //  files alone. As Seams::list.
    static std::vector<ImageFile>  ListFiles (const FolderRequest & request);

    //  Whether the path is the folder or inside it, case aside.
    static bool  IsInFolder (const std::wstring & path, const std::wstring & folder);

private:
    void  Run              (std::stop_token stop);
    void  AddFolderLocked  (const std::wstring & folder, bool withSubfolders);
    bool  IsKnownDueLocked (ULONGLONG nowMs) const;
    bool  IsCassoLocked    (const std::wstring & folder) const;

    static bool  IsSameList (const std::vector<std::wstring> & a, const std::vector<std::wstring> & b);

    static bool               IsSameFolder (const std::wstring & a, const std::wstring & b);
    static std::wstring_view  TrimFolder   (std::wstring_view folder);

    Seams                            m_seams;
    ULONGLONG                        m_settleAtMs    = 0;
    mutable std::mutex               m_lock;
    std::condition_variable_any      m_wake;
    std::map<HWND, ThumbnailTabs>    m_windows;
    std::deque<FolderRequest>        m_folders;               // the folders to sweep
    std::vector<std::wstring>        m_known;
    std::vector<std::wstring>        m_cassoImages;
    std::vector<std::wstring>        m_cassoFolders;          // the folders m_cassoImages are in
    bool                             m_isKnownDue    = false;  // the Casso node's folders are to be swept
    bool                             m_hasKnownStart = false;  // StartKnownFolders was called
    std::jthread                     m_thread;
};
