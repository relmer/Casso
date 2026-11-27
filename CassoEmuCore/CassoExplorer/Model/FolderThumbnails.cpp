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
//  FolderThumbnails::SetWindowTabs
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SetWindowTabs (HWND window, const ThumbnailTabs & tabs)
{
    std::vector<std::wstring>  opened;
    bool                       wasOpen  = false;



    {
        std::lock_guard  lock (m_lock);
        ThumbnailTabs  & before = m_windows[window];

        opened = tabs.others;
        opened.insert (opened.begin(), tabs.active);

        for (const std::wstring & folder : opened)
        {
            wasOpen = IsSameFolder (before.active, folder)
                    || std::any_of (before.others.begin(), before.others.end(), [&folder] (const std::wstring & other) { return IsSameFolder (other, folder); });

            if (!folder.empty() && !wasOpen)
            {
                AddFolderLocked (folder, false);
            }
        }

        before = tabs;
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

        AddFolderLocked (folder, false);
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::SetKnownFolders
//
//  The same folders again aren't swept again.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SetKnownFolders (const std::vector<std::wstring> & folders)
{
    {
        std::lock_guard  lock (m_lock);

        if (IsSameList (folders, m_known))
        {
            return;
        }

        m_known      = folders;
        m_isKnownDue = !folders.empty();
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::SetCassoImages
//
//  The same images again aren't swept again.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::SetCassoImages (const std::vector<std::wstring> & paths)
{
    std::wstring  folder;



    {
        std::lock_guard  lock (m_lock);

        if (IsSameList (paths, m_cassoImages))
        {
            return;
        }

        m_cassoImages = paths;
        m_cassoFolders.clear();

        for (const std::wstring & path : paths)
        {
            folder = std::filesystem::path (path).parent_path().wstring();

            if (std::none_of (m_cassoFolders.begin(), m_cassoFolders.end(), [&folder] (const std::wstring & each) { return IsSameFolder (each, folder); }))
            {
                m_cassoFolders.push_back (folder);
            }
        }

        m_isKnownDue = !paths.empty() || !m_known.empty();
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::StartKnownFolders
//
//  The Casso node is in use, so its folders aren't put off.
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

        for (const auto & [window, tabs] : m_windows)
        {
            if (!tabs.active.empty())
            {
                AddFolderLocked (tabs.active, false);
            }

            for (const std::wstring & folder : tabs.others)
            {
                AddFolderLocked (folder, false);
            }
        }

        m_isKnownDue = !m_known.empty() || !m_cassoImages.empty();
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
    int              rank     = kOtherRank;
    bool             isKnown  = IsCassoLocked (folder);
    bool             isActive = false;
    bool             isOpen   = false;



    for (const auto & [window, tabs] : m_windows)
    {
        isActive = IsSameFolder (tabs.active, folder) || (isKnown && tabs.casso == ThumbnailTabs::CassoNode::Active);

        if (isActive)
        {
            return kActiveRank;
        }

        isOpen = std::any_of (tabs.others.begin(), tabs.others.end(), [&folder] (const std::wstring & other) { return IsSameFolder (other, folder); })
              || (isKnown && tabs.casso == ThumbnailTabs::CassoNode::Other);

        if (isOpen)
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

bool FolderThumbnails::TryTakeFolder (ULONGLONG nowMs, FolderRequest & outRequest)
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
        if (!m_cassoImages.empty())
        {
            m_folders.push_back ({ std::wstring(), false, m_cassoImages });
        }

        for (const std::wstring & folder : m_known)
        {
            AddFolderLocked (folder, true);
        }

        m_isKnownDue = false;
    }

    if (m_folders.empty())
    {
        return false;
    }

    outRequest = std::move (m_folders.front());
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

void FolderThumbnails::Sweep (const FolderRequest & request)
{
    std::vector<ImageFile>  images;
    std::stop_token         stop     = m_thread.get_stop_token();
    bool                    isFloppy = false;



    if (!m_seams.list || !m_seams.hasThumbnail || !m_seams.markQueued || !m_seams.queue)
    {
        return;
    }

    images = m_seams.list (request);

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
//  A folder that can't be read lists nothing. Hidden and system folders are
//  skipped, as the Casso node's list skips them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FolderThumbnails::ImageFile> FolderThumbnails::ListFiles (const FolderRequest & request)
{
    static constexpr DWORD                         kSkipped   = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    std::vector<ImageFile>                         files;
    std::error_code                                failed;
    std::filesystem::recursive_directory_iterator  each (request.folder, std::filesystem::directory_options::skip_permission_denied, failed);
    std::filesystem::recursive_directory_iterator  end;
    ImageFile                                      file;
    DWORD                                          attributes = 0;
    bool                                           isFolder   = false;
    bool                                           isFile     = false;



    for (const std::wstring & path : request.files)
    {
        file.path  = path;
        file.bytes = std::filesystem::file_size (path, failed);
        file.time  = (int64_t) std::filesystem::last_write_time (path, failed).time_since_epoch().count();

        if (!failed)
        {
            files.push_back (file);
        }

        failed.clear();
    }

    if (!request.files.empty())
    {
        return files;
    }

    for (; !failed && each != end; each.increment (failed))
    {
        isFolder = each->is_directory (failed);

        if (isFolder)
        {
            attributes = GetFileAttributesW (each->path().c_str());

            if (!request.withSubfolders || (attributes != INVALID_FILE_ATTRIBUTES && (attributes & kSkipped) != 0))
            {
                each.disable_recursion_pending();
            }

            failed.clear();
            continue;
        }

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
//  FolderThumbnails::IsInFolder
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::IsInFolder (const std::wstring & path, const std::wstring & folder)
{
    std::wstring_view  inner = TrimFolder (path);
    std::wstring_view  outer = TrimFolder (folder);
    bool               isAt  = false;



    if (outer.empty() || inner.size() < outer.size() || _wcsnicmp (inner.data(), outer.data(), outer.size()) != 0)
    {
        return false;
    }

    isAt = inner.size() == outer.size();

    return isAt || inner[outer.size()] == L'\\' || inner[outer.size()] == L'/';
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::Run
//
//  The wait ends at least every kPollMs, for the Casso node's folders' start.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::Run (std::stop_token stop)
{
    static constexpr DWORD  kPollMs = 1000;
    FolderRequest           request;



    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    while (!stop.stop_requested())
    {
        {
            std::unique_lock  lock (m_lock);

            m_wake.wait_for (lock, stop, std::chrono::milliseconds (kPollMs), [this]() { return !m_folders.empty(); });
        }

        while (!stop.stop_requested() && TryTakeFolder (GetTickCount64(), request))
        {
            Sweep (request);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::AddFolderLocked
//
//  A folder already waiting isn't added again. Once it is to be swept with
//  its subfolders, it stays that way.
//
////////////////////////////////////////////////////////////////////////////////

void FolderThumbnails::AddFolderLocked (const std::wstring & folder, bool withSubfolders)
{
    auto  waiting = std::find_if (m_folders.begin(), m_folders.end(), [&folder] (const FolderRequest & each) { return IsSameFolder (each.folder, folder); });



    if (waiting == m_folders.end())
    {
        m_folders.push_back ({ folder, withSubfolders, {} });
        return;
    }

    waiting->withSubfolders = waiting->withSubfolders || withSubfolders;
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
//  FolderThumbnails::IsCassoLocked
//
//  Whether the folder has one of the Casso node's images or is under one of
//  its folders.
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::IsCassoLocked (const std::wstring & folder) const
{
    return std::any_of (m_known.begin(), m_known.end(), [&folder] (const std::wstring & known) { return IsInFolder (folder, known); })
        || std::any_of (m_cassoFolders.begin(), m_cassoFolders.end(), [&folder] (const std::wstring & each) { return IsSameFolder (each, folder); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::IsSameList
//
//  The same paths in the same order, case and trailing separators aside.
//
////////////////////////////////////////////////////////////////////////////////

bool FolderThumbnails::IsSameList (const std::vector<std::wstring> & a, const std::vector<std::wstring> & b)
{
    return std::equal (a.begin(), a.end(), b.begin(), b.end(), IsSameFolder);
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
    std::wstring_view  left  = TrimFolder (a);
    std::wstring_view  right = TrimFolder (b);



    return !left.empty() && left.size() == right.size() && _wcsnicmp (left.data(), right.data(), left.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnails::TrimFolder
//
//  The folder without trailing separators.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring_view FolderThumbnails::TrimFolder (std::wstring_view folder)
{
    while (folder.size() > 1 && (folder.back() == L'\\' || folder.back() == L'/'))
    {
        folder.remove_suffix (1);
    }

    return folder;
}
