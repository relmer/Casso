#include "Pch.h"

#include "Win32ImageWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher::~Win32ImageWatcher
//
//  Every watch stops before the object goes.
//
////////////////////////////////////////////////////////////////////////////////

Win32ImageWatcher::~Win32ImageWatcher()
{
    std::lock_guard<std::mutex>  guard (m_mutex);



    for (auto & entry : m_watches)
    {
        CloseWatch (*entry.second);
    }

    m_watches.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher::Watch
//
//  Begins reporting changes under a directory.
//
//  FALSE IS A STATE, NOT A FAILURE. A path that cannot be opened for
//  notification -- a share that does not support it, a folder that has gone --
//  leaves the caller unwatched and knowing it, and the check made before every
//  write is what carries the guarantee either way.
//
//  A DIRECTORY ALREADY WATCHED SUCCEEDS WITHOUT A SECOND THREAD. Two disks out
//  of one folder is ordinary, and it is one watch.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ImageWatcher::Watch (const std::string & directory, Callback callback)
{
    std::lock_guard<std::mutex>  guard (m_mutex);
    std::wstring                 wide  = fs::path (directory).wstring();
    bool                         began = false;



    if (m_watches.find (directory) != m_watches.end())
    {
        return true;
    }

    {
        auto    watch  = std::make_unique<DirectoryWatch> ();
        HANDLE  opened = CreateFileW (wide.c_str(),
                                      FILE_LIST_DIRECTORY,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                      nullptr);

        if (opened != INVALID_HANDLE_VALUE)
        {
            watch->directory = opened;
            watch->stop      = CreateEventW (nullptr, TRUE, FALSE, nullptr);
            watch->callback  = std::move (callback);

            if (watch->stop != nullptr)
            {
                DirectoryWatch *  raw = watch.get();

                raw->worker = std::thread (&Win32ImageWatcher::RunWatch, raw, directory);

                m_watches[directory] = std::move (watch);
                began                = true;
            }
            else
            {
                CloseHandle (opened);
            }
        }
    }

    return began;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher::Unwatch
//
//  Stops reporting. A directory that was never watched is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ImageWatcher::Unwatch (const std::string & directory)
{
    std::lock_guard<std::mutex>  guard (m_mutex);
    auto                         found = m_watches.find (directory);



    if (found != m_watches.end())
    {
        CloseWatch (*found->second);
        m_watches.erase (found);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher::CloseWatch
//
//  Stops one watch and waits for its thread.
//
//  THE STOP EVENT IS SET BEFORE THE I/O IS CANCELLED, so the worker sees the
//  reason it woke rather than treating a cancelled read as a real change.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ImageWatcher::CloseWatch (DirectoryWatch & watch)
{
    BOOL  cancelled = FALSE;



    if (watch.stop != nullptr)
    {
        SetEvent (watch.stop);
    }

    if (watch.directory != INVALID_HANDLE_VALUE)
    {
        //  A read that had already completed cancels as "nothing to cancel",
        //  which is the ordinary case rather than a failure: the stop event
        //  above is what actually ends the loop.
        cancelled = CancelIoEx (watch.directory, nullptr);
        IGNORE_RETURN_VALUE (cancelled, TRUE);
    }

    if (watch.worker.joinable())
    {
        watch.worker.join();
    }

    if (watch.directory != INVALID_HANDLE_VALUE)
    {
        CloseHandle (watch.directory);
        watch.directory = INVALID_HANDLE_VALUE;
    }

    if (watch.stop != nullptr)
    {
        CloseHandle (watch.stop);
        watch.stop = nullptr;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher::RunWatch
//
//  Reads change records until told to stop.
//
//  RENAMES ARE WATCHED ALONGSIDE WRITES, and that is the case that matters
//  rather than an extra. Both writers here commit by renaming a temporary over
//  the image, so the record that says the image changed is a rename record and
//  not a write one.
//
//  A RECORD IS REPORTED WITHOUT BEING JUDGED. Whether the named file is one
//  this session cares about, and whether its contents actually differ, are both
//  decided above -- here by the recorded identity, which is what stops the
//  emulator's own commit from coming back as somebody else's change.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ImageWatcher::RunWatch (DirectoryWatch * watch, std::string directory)
{
    std::vector<Byte>  buffer (kChangeBufferBytes);
    OVERLAPPED         overlapped = {};
    HANDLE             waits[2]   = {};
    bool               running    = true;



    overlapped.hEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);

    if (overlapped.hEvent == nullptr)
    {
        return;
    }

    waits[0] = watch->stop;
    waits[1] = overlapped.hEvent;

    while (running)
    {
        DWORD  transferred = 0;
        DWORD  waited      = 0;
        BOOL   queued      = FALSE;

        ResetEvent (overlapped.hEvent);

        queued = ReadDirectoryChangesW (watch->directory,
                                        buffer.data(),
                                        (DWORD) buffer.size(),
                                        FALSE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME
                                            | FILE_NOTIFY_CHANGE_LAST_WRITE
                                            | FILE_NOTIFY_CHANGE_SIZE,
                                        nullptr,
                                        &overlapped,
                                        nullptr);

        if (!queued)
        {
            running = false;
            break;
        }

        waited = WaitForMultipleObjects (2, waits, FALSE, INFINITE);

        if (waited != (WAIT_OBJECT_0 + 1))
        {
            running = false;
            break;
        }

        if (!GetOverlappedResult (watch->directory, &overlapped, &transferred, FALSE))
        {
            running = false;
            break;
        }

        //  Zero bytes means the platform's buffer overflowed and records were
        //  dropped. Nothing is retried: the check before every write still
        //  holds, and the next record about the same file arrives anyway.
        {
            size_t  offset = 0;

            while (transferred > 0 && offset + sizeof (FILE_NOTIFY_INFORMATION) <= transferred)
            {
                const FILE_NOTIFY_INFORMATION *  record =
                    reinterpret_cast<const FILE_NOTIFY_INFORMATION *> (buffer.data() + offset);

                std::wstring  name (record->FileName,
                                    record->FileNameLength / sizeof (wchar_t));

                if (watch->callback)
                {
                    fs::path  full = fs::path (directory) / name;

                    watch->callback (full.string());
                }

                if (record->NextEntryOffset == 0)
                {
                    break;
                }

                offset += record->NextEntryOffset;
            }
        }
    }

    CloseHandle (overlapped.hEvent);

    return;
}
