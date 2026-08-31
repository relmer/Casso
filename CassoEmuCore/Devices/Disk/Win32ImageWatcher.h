#pragma once

#include "Pch.h"
#include "IImageWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ImageWatcher
//
//  The platform half of change notification, over ReadDirectoryChangesW.
//
//  IN CORE RATHER THAN IN THE SHELL, and the constitution is explicit about
//  why: the criterion is testability, not a platform boundary, and "calling
//  Win32 is not a reason to live in the exe". Win32DiskFileIo sits beside it
//  for the same reason. What stays in the executable is the handing-over, and
//  nothing else.
//
//  IT WATCHES A DIRECTORY, NOT A FILE, AND THAT IS LOAD-BEARING. Both writers
//  in this system commit by renaming a temporary over the target, so a handle
//  held on the image itself sees its own replacement as a deletion and goes
//  deaf at exactly the moment it was there to report.
//
//  A THREAD PER DIRECTORY. The call blocks, and the alternative -- one thread
//  multiplexing several with overlapped I/O -- buys nothing here: a session
//  watches one or two folders, and the cost of a waiting thread is a stack.
//
//  EVERY CALLBACK ARRIVES ON THAT THREAD, so a handler must do no more than
//  record. DiskImageStore's does exactly that, and the work happens later on
//  the thread that owns disk writes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ImageWatcher : public IImageWatcher
{
public:

    Win32ImageWatcher () = default;
    ~Win32ImageWatcher () override;

    bool  Watch   (const std::string & directory, Callback callback) override;
    void  Unwatch (const std::string & directory) override;

private:

    //  One watched directory: the thread reading it, the handle it reads, and
    //  the event that tells it to stop.
    struct DirectoryWatch
    {
        std::thread  worker;
        HANDLE       directory = INVALID_HANDLE_VALUE;
        HANDLE       stop      = nullptr;

        //  Set once the worker has actually asked the platform to report
        //  changes. See kArmTimeoutMs.
        HANDLE       armed     = nullptr;

        Callback     callback;
    };

    //  Reads change records until the stop event is set. Runs on the watch's
    //  own thread.
    static void  RunWatch (DirectoryWatch * watch, std::string directory);

    //  Stops and joins one watch. Called from Unwatch and from the destructor,
    //  so shutting down is written once.
    static void  CloseWatch (DirectoryWatch & watch);

    //  How long Watch waits for its worker to be listening before returning.
    //
    //  IT HAS TO WAIT AT ALL, and that is the whole reason this exists.
    //  ReadDirectoryChangesW reports what happens AFTER it is called -- the
    //  platform starts buffering at the first call, not when the handle is
    //  opened. Returning as soon as the thread was created left a window in
    //  which a write was simply not seen, and a mount followed straight away by
    //  a build lands squarely in it. Measured: a commit two lines after Watch
    //  returned was never reported.
    //
    //  A timeout rather than an unbounded wait, because failing to arm must
    //  degrade to "not watching" -- which the write-time check still covers --
    //  rather than hanging a mount.
    static constexpr DWORD  kArmTimeoutMs = 2000;

    //  How much change data the platform may buffer before records are dropped.
    //  A dropped record is survivable here -- the check before every write is
    //  the guarantee -- but a build writing several files at once should not
    //  reach it.
    static constexpr DWORD  kChangeBufferBytes = 32 * 1024;

    std::unordered_map<std::string, std::unique_ptr<DirectoryWatch>>  m_watches;

    //  Guards the map alone. Watch and Unwatch are called from the thread that
    //  mounts and ejects; the worker threads never touch it.
    std::mutex                                               m_mutex;
};
