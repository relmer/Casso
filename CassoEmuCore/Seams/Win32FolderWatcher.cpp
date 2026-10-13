#include "Pch.h"

#include "Core/PathResolver.h"
#include "Core/ThreadName.h"

#include "Seams/Win32FolderWatcher.h"




static constexpr DWORD  s_kFilter = FILE_NOTIFY_CHANGE_FILE_NAME
                                    | FILE_NOTIFY_CHANGE_DIR_NAME
                                    | FILE_NOTIFY_CHANGE_LAST_WRITE
                                    | FILE_NOTIFY_CHANGE_SIZE;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::~Win32FolderWatcher
//
////////////////////////////////////////////////////////////////////////////////

Win32FolderWatcher::~Win32FolderWatcher()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::Watch
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FolderWatcher::Watch (const std::wstring & directory, Callback callback)
{
    HANDLE                         notification = INVALID_HANDLE_VALUE;
    std::shared_ptr<NetworkOpens>  network      = m_network;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        if (m_folders.find (directory) != m_folders.end())
        {
            return true;
        }

        if (m_folders.size() >= kMaxFolders)
        {
            return false;
        }
    }

    if (PathResolver::IsNetworkPath (directory))
    {
        {
            std::lock_guard<std::mutex>  held (network->lock);

            if (!network->pending.insert (directory).second)
            {
                return true;
            }
        }

        std::thread ([this, network, directory, callback]()
        {
            HRESULT                       hrName = ThreadName::SetForCurrentThread (L"Casso Explorer network folder watch");
            HANDLE                        opened = FindFirstChangeNotificationW (directory.c_str(), FALSE, s_kFilter);
            bool                          valid  = opened != INVALID_HANDLE_VALUE && opened != nullptr;
            std::unique_lock<std::mutex>  held   (network->lock);
            bool                          wanted = network->alive && network->pending.erase (directory) > 0;



            IGNORE_RETURN_VALUE (hrName, S_OK);

            //  Under the shared lock the watcher cannot be stopping, so it
            //  takes the handle; otherwise, or if the folder was unwatched
            //  meanwhile, the handle is closed here.
            if (valid && wanted)
            {
                AddFolder (directory, opened, callback);
            }
            else if (valid)
            {
                FindCloseChangeNotification (opened);
            }
        }).detach();

        return true;
    }

    notification = FindFirstChangeNotificationW (directory.c_str(), FALSE, s_kFilter);

    if (notification == INVALID_HANDLE_VALUE || notification == nullptr)
    {
        return false;
    }

    return AddFolder (directory, notification, std::move (callback));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::AddFolder
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FolderWatcher::AddFolder (const std::wstring & directory, HANDLE notification, Callback callback)
{
    std::lock_guard<std::mutex>  held (m_lock);



    if (m_folders.find (directory) != m_folders.end() || m_folders.size() >= kMaxFolders)
    {
        FindCloseChangeNotification (notification);
        return m_folders.find (directory) != m_folders.end();
    }

    m_folders[directory] = FolderEntry { notification, std::move (callback) };

    EnsureService();

    //  The service thread is waiting on the set as it was; wake it to take in
    //  the one just added.
    if (m_wake != nullptr)
    {
        SetEvent (m_wake);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::Unwatch
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::Unwatch (const std::wstring & directory)
{
    std::unique_lock<std::mutex>  pending (m_network->lock);
    std::unique_lock<std::mutex>  held    (m_lock, std::defer_lock);
    auto                          found   = m_folders.end();



    //  A network folder being opened is dropped when its handle arrives.
    m_network->pending.erase (directory);
    pending.unlock();

    held.lock();
    found = m_folders.find (directory);

    if (found == m_folders.end())
    {
        return;
    }

    FindCloseChangeNotification (found->second.notification);
    m_folders.erase (found);

    if (m_wake != nullptr)
    {
        SetEvent (m_wake);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::EnsureService
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::EnsureService()
{
    if (m_service.joinable())
    {
        return;
    }

    m_stop = CreateEventW (nullptr, TRUE,  FALSE, nullptr);
    m_wake = CreateEventW (nullptr, FALSE, FALSE, nullptr);

    if (m_stop == nullptr || m_wake == nullptr)
    {
        return;
    }

    m_service = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso Explorer folder watcher"); IGNORE_RETURN_VALUE (hrName, S_OK); RunService(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::RunService
//
//  One wait over every folder. A handle that fires is reported and re-armed;
//  the wake handle means the set changed and the wait is rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::RunService()
{
    bool  running = true;



    while (running)
    {
        std::vector<HANDLE>        waits;
        std::vector<std::wstring>  directories;
        DWORD                      signaled = 0;
        size_t                     index    = 0;

        {
            std::lock_guard<std::mutex>  held (m_lock);

            waits.push_back (m_stop);
            waits.push_back (m_wake);

            for (const auto & folder : m_folders)
            {
                waits.push_back (folder.second.notification);
                directories.push_back (folder.first);
            }
        }

        signaled = WaitForMultipleObjects ((DWORD) waits.size(), waits.data(), FALSE, INFINITE);

        if (signaled == WAIT_OBJECT_0)
        {
            running = false;
            break;
        }

        //  A folder moved away with its handle open -- into the Recycle Bin,
        //  say -- can leave that handle unusable, which fails the whole wait.
        //  Each unusable one is dropped and reported, so the rest keep being
        //  watched and the folder's loss is still seen.
        if (signaled == WAIT_FAILED)
        {
            DropUnusable (directories);
            continue;
        }

        if (signaled == (WAIT_OBJECT_0 + 1))
        {
            //  The set changed; wait again over the new one.
            continue;
        }

        index = (size_t) (signaled - WAIT_OBJECT_0) - 2;

        if (index >= directories.size())
        {
            continue;
        }

        //  Copied under the lock so the callback runs without it, and so a
        //  folder dropped meanwhile is not reported.
        {
            std::wstring  directory = directories[index];
            Callback      callback;

            {
                std::lock_guard<std::mutex>  held  = std::lock_guard<std::mutex> (m_lock);
                auto                         found = m_folders.find (directory);

                if (found != m_folders.end())
                {
                    callback = found->second.callback;

                    //  Re-armed before the callback, so a change made while it
                    //  runs is still reported. A folder that is gone, or whose
                    //  handle cannot be re-armed, is dropped: its handle can stay
                    //  signaled and would spin this loop.
                    if (!FindNextChangeNotification (found->second.notification) ||
                        GetFileAttributesW (directory.c_str()) == INVALID_FILE_ATTRIBUTES)
                    {
                        FindCloseChangeNotification (found->second.notification);
                        m_folders.erase (found);
                    }
                }
            }

            if (callback)
            {
                callback (directory);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::DropUnusable
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::DropUnusable (const std::vector<std::wstring> & directories)
{
    std::vector<std::pair<std::wstring, Callback>>  dropped;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        for (const std::wstring & directory : directories)
        {
            auto  found = m_folders.find (directory);

            if (found != m_folders.end() && WaitForSingleObject (found->second.notification, 0) == WAIT_FAILED)
            {
                dropped.emplace_back (directory, found->second.callback);
                FindCloseChangeNotification (found->second.notification);
                m_folders.erase (found);
            }
        }
    }

    for (const auto & [directory, callback] : dropped)
    {
        if (callback)
        {
            callback (directory);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher::Stop
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::Stop()
{
    //  A network folder being opened has its handle closed by its own thread.
    {
        std::lock_guard<std::mutex>  held (m_network->lock);

        m_network->alive = false;
        m_network->pending.clear();
    }

    if (m_stop != nullptr)
    {
        SetEvent (m_stop);
    }

    if (m_service.joinable())
    {
        m_service.join();
    }

    {
        std::lock_guard<std::mutex>  held (m_lock);

        for (auto & folder : m_folders)
        {
            FindCloseChangeNotification (folder.second.notification);
        }

        m_folders.clear();
    }

    if (m_stop != nullptr)
    {
        CloseHandle (m_stop);
        m_stop = nullptr;
    }

    if (m_wake != nullptr)
    {
        CloseHandle (m_wake);
        m_wake = nullptr;
    }
}
