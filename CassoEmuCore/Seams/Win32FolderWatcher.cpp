#include "Pch.h"

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
    std::lock_guard<std::mutex>  held (m_lock);
    HANDLE                       notification = INVALID_HANDLE_VALUE;



    if (m_folders.find (directory) != m_folders.end())
    {
        return true;
    }

    if (m_folders.size() >= kMaxFolders)
    {
        return false;
    }

    notification = FindFirstChangeNotificationW (directory.c_str(), FALSE, s_kFilter);

    if (notification == INVALID_HANDLE_VALUE || notification == nullptr)
    {
        return false;
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
    std::lock_guard<std::mutex>  held  = std::lock_guard<std::mutex> (m_lock);
    auto                         found = m_folders.find (directory);



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

    m_service = std::thread (&Win32FolderWatcher::RunService, this);
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

        if (signaled == WAIT_OBJECT_0 || signaled == WAIT_FAILED)
        {
            running = false;
            break;
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
                    //  runs is still reported.
                    FindNextChangeNotification (found->second.notification);
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
//  Win32FolderWatcher::Stop
//
////////////////////////////////////////////////////////////////////////////////

void Win32FolderWatcher::Stop()
{
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
