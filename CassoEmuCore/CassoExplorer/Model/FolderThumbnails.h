#pragma once

#include "Pch.h"

#include "Seams/IFolderThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails
//
//  IFolderThumbnails over a thread that lists each folder to sweep and
//  queues each floppy image in it with no thumbnail. The folders open in a
//  window's tabs are swept first. The known folders wait kSettleMs from the start,
//  so a launch isn't slowed by boots, unless StartKnownFolders is called.
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

    struct Seams
    {
        std::function<std::vector<ImageFile> (const std::wstring & folder)>  list;          // the files in a folder, not in its subfolders
        std::function<bool (const std::wstring & path)>                      hasThumbnail;
        std::function<bool (const ImageFile & image)>                        markQueued;    // false for an image queued before at its size and time
        std::function<void (const std::wstring & path)>                      queue;
        std::function<void()>                                                removeQueued;  // the images waiting for a sweep
        std::function<bool()>                                                isEnabled;
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

    void  SetWindowFolders  (HWND window, const std::wstring & active, const std::vector<std::wstring> & others) override;
    void  RemoveWindow      (HWND window) override;
    void  SweepFolder       (const std::wstring & folder) override;
    void  SetKnownFolders   (const std::vector<std::wstring> & folders) override;
    void  StartKnownFolders () override;
    void  ApplySetting      () override;

    //  kActiveRank for an active tab's folder, kTabRank for another tab's,
    //  kOtherRank for any other folder. Called on the boot worker's thread.
    int   GetRank (const std::wstring & folder) const;

    //  The next folder to sweep, removed: a folder open in a tab, then a
    //  known folder once nowMs is kSettleMs past the start. False for none,
    //  or with the setting off. The thread's, and a test's.
    bool  TryTakeFolder (ULONGLONG nowMs, std::wstring & outFolder);

    //  Each floppy image in the folder with no thumbnail, passed to the
    //  queue function. The thread's, and a test's.
    void  Sweep (const std::wstring & folder);

    //  The files in a host folder, not in its subfolders, as Seams::list.
    static std::vector<ImageFile>  ListFiles (const std::wstring & folder);

private:
    struct WindowFolders
    {
        std::wstring               active;
        std::vector<std::wstring>  others;
    };

    void  Run              (std::stop_token stop);
    void  AddFolderLocked  (const std::wstring & folder);
    bool  IsKnownDueLocked (ULONGLONG nowMs) const;

    static bool  IsSameFolder (const std::wstring & a, const std::wstring & b);

    Seams                          m_seams;
    ULONGLONG                      m_settleAtMs    = 0;
    mutable std::mutex             m_lock;
    std::condition_variable_any    m_wake;
    std::map<HWND, WindowFolders>  m_windows;
    std::deque<std::wstring>       m_folders;               // the folders to sweep
    std::vector<std::wstring>      m_known;
    bool                           m_isKnownDue    = false;  // the known folders are to be swept
    bool                           m_hasKnownStart = false;  // StartKnownFolders was called
    std::jthread                   m_thread;
};
