#include "Pch.h"

#include "CassoExplorer/Model/DiskImagePictures.h"
#include "CassoExplorer/Model/FolderThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::FolderThumbnails
//
////////////////////////////////////////////////////////////////////////////////

FolderThumbnails::FolderThumbnails (Seams seams, ULONGLONG startMs) :
    m_seams      (std::move (seams)),
    m_settleAtMs (startMs + kSettleMs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::~FolderThumbnails
//
//  The folder being swept is left part way through.
//
////////////////////////////////////////////////////////////////////////////////

FolderThumbnails::~FolderThumbnails()
{
    if (m_thread.joinable())
    {
        m_thread.request_stop();
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::Start
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::Start()
{
    m_thread = std::jthread ([this] (std::stop_token stop) { Run (stop); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::SetWindowFolders
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SetWindowFolders (HWND window, const std::wstring & active, const std::vector<std::wstring> & others)
{
    std::vector<std::wstring>  opened;
    bool                       wasOpen  = false;



    {
        std::lock_guard  lock (m_lock);
        WindowFolders  & folders = m_windows[window];

        opened = others;
        opened.insert (opened.begin(), active);

        for (const std::wstring & folder : opened)
        {
            wasOpen = IsSameFolder (folders.active, folder)
                    || std::any_of (folders.others.begin(), folders.others.end(), [&folder] (const std::wstring & other) { return IsSameFolder (other, folder); });

            if (!folder.empty() && !wasOpen)
            {
                AddFolderLocked (folder);
            }
        }

        folders.active = active;
        folders.others = others;
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::RemoveWindow
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::RemoveWindow (HWND window)
{
    std::lock_guard  lock (m_lock);



    m_windows.erase (window);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::SweepFolder
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SweepFolder (const std::wstring & folder)
{
    if (folder.empty())
    {
        return;
    }

    {
        std::lock_guard  lock (m_lock);

        AddFolderLocked (folder);
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::SetKnownFolders
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SetKnownFolders (const std::vector<std::wstring> & folders)
{
    {
        std::lock_guard  lock (m_lock);

        m_known      = folders;
        m_isKnownDue = !folders.empty();
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::StartKnownFolders
//
//  The known folders list is in view, so the known folders aren't put off.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::StartKnownFolders()
{
    {
        std::lock_guard  lock (m_lock);

        m_hasKnownStart = true;
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::ApplySetting
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::ApplySetting()
{
    bool  isEnabled = m_seams.isEnabled && m_seams.isEnabled();



    if (!isEnabled)
    {
        {
            std::lock_guard  lock (m_lock);

            m_folders.clear();
        }

        if (m_seams.removeQueued)
        {
            m_seams.removeQueued();
        }

        return;
    }

    {
        std::lock_guard  lock (m_lock);

        for (const auto & [window, folders] : m_windows)
        {
            if (!folders.active.empty())
            {
                AddFolderLocked (folders.active);
            }

            for (const std::wstring & folder : folders.others)
            {
                AddFolderLocked (folder);
            }
        }

        m_isKnownDue = !m_known.empty();
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::GetRank
//
////////////////////////////////////////////////////////////////////////////////

int FolderThumbnails::GetRank (const std::wstring & folder) const
{
    std::lock_guard  lock (m_lock);
    int              rank = kOtherRank;



    for (const auto & [window, folders] : m_windows)
    {
        if (IsSameFolder (folders.active, folder))
        {
            return kActiveRank;
        }

        if (std::any_of (folders.others.begin(), folders.others.end(), [&folder] (const std::wstring & other) { return IsSameFolder (other, folder); }))
        {
            rank = kTabRank;
        }
    }

    return rank;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::TryTakeFolder
//
//  With the setting off, the folders waiting are dropped; ApplySetting adds
//  them again once it is on.
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::TryTakeFolder (ULONGLONG nowMs, std::wstring & outFolder)
{
    bool              isEnabled = m_seams.isEnabled && m_seams.isEnabled();
    std::lock_guard   lock (m_lock);



    if (!isEnabled)
    {
        m_folders.clear();
        return false;
    }

    if (m_folders.empty() && IsKnownDueLocked (nowMs))
    {
        for (const std::wstring & folder : m_known)
        {
            AddFolderLocked (folder);
        }

        m_isKnownDue = false;
    }

    if (m_folders.empty())
    {
        return false;
    }

    outFolder = std::move (m_folders.front());
    m_folders.pop_front();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::Sweep
//
//  The size is checked before the thumbnail, as finding a thumbnail reads
//  the file.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::Sweep (const std::wstring & folder)
{
    std::vector<ImageFile>  images;
    std::stop_token         stop     = m_thread.get_stop_token();
    bool                    isFloppy = false;



    if (!m_seams.list || !m_seams.hasThumbnail || !m_seams.markQueued || !m_seams.queue)
    {
        return;
    }

    images = m_seams.list (folder);

    for (const ImageFile & image : images)
    {
        if (stop.stop_requested())
        {
            break;
        }

        isFloppy = DiskImagePictures::IsFloppySize (std::filesystem::path (image.path).extension().wstring(), image.bytes);

        if (!isFloppy || m_seams.hasThumbnail (image.path) || !m_seams.markQueued (image))
        {
            continue;
        }

        m_seams.queue (image.path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::ListFiles
//
//  A folder that can't be read lists nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FolderThumbnails::ImageFile> FolderThumbnails::ListFiles (const std::wstring & folder)
{
    std::vector<ImageFile>                 files;
    std::error_code                        failed;
    std::filesystem::directory_iterator    each (folder, std::filesystem::directory_options::skip_permission_denied, failed);
    std::filesystem::directory_iterator    end;
    ImageFile                              file;
    bool                                   isFile = false;



    for (; !failed && each != end; each.increment (failed))
    {
        isFile = each->is_regular_file (failed);

        if (failed || !isFile)
        {
            failed.clear();
            continue;
        }

        file.path  = each->path().wstring();
        file.bytes = each->file_size (failed);
        file.time  = (int64_t) each->last_write_time (failed).time_since_epoch().count();

        if (!failed)
        {
            files.push_back (file);
        }

        failed.clear();
    }

    return files;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::Run
//
//  The wait ends at least every kPollMs, for the known folders' start.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::Run (std::stop_token stop)
{
    static constexpr DWORD  kPollMs = 1000;
    std::wstring            folder;



    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    while (!stop.stop_requested())
    {
        {
            std::unique_lock  lock (m_lock);

            m_wake.wait_for (lock, stop, std::chrono::milliseconds (kPollMs), [this]() { return !m_folders.empty(); });
        }

        while (!stop.stop_requested() && TryTakeFolder (GetTickCount64(), folder))
        {
            Sweep (folder);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::AddFolderLocked
//
//  A folder already waiting isn't added again.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::AddFolderLocked (const std::wstring & folder)
{
    bool  isWaiting = std::any_of (m_folders.begin(), m_folders.end(), [&folder] (const std::wstring & each) { return IsSameFolder (each, folder); });



    if (!isWaiting)
    {
        m_folders.push_back (folder);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::IsKnownDueLocked
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::IsKnownDueLocked (ULONGLONG nowMs) const
{
    return m_isKnownDue && (m_hasKnownStart || nowMs >= m_settleAtMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::IsSameFolder
//
//  Case aside, and a trailing separator aside.
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::IsSameFolder (const std::wstring & a, const std::wstring & b)
{
    std::wstring_view  left  = a;
    std::wstring_view  right = b;



    while (left.size() > 1 && (left.back() == L'\\' || left.back() == L'/'))
    {
        left.remove_suffix (1);
    }

    while (right.size() > 1 && (right.back() == L'\\' || right.back() == L'/'))
    {
        right.remove_suffix (1);
    }

    return !left.empty() && left.size() == right.size() && _wcsnicmp (left.data(), right.data(), left.size()) == 0;
}
