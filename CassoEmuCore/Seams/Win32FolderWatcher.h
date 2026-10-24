#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/IFolderWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FolderWatcher
//
//  IFolderWatcher over FindFirstChangeNotification.
//
//  ONE THREAD FOR EVERY FOLDER, not one thread each. A change notification is
//  a waitable handle, so a single WaitForMultipleObjects covers all of them at
//  once. The alternative, ReadDirectoryChangesW, reports which file changed
//  and what happened to it -- detail a browser throws away, since it re-reads
//  the folder regardless -- and buying it costs either a thread parked per
//  folder or a completion port to avoid that. A file browser with a tree open
//  watches a dozen folders, and a dozen parked threads to learn something
//  already known is a poor trade.
//
//  THE LIMIT IS THE WAIT'S. WaitForMultipleObjects takes 64 handles, two of
//  which are spent here on stopping and on waking the thread when the set
//  changes. Past that a folder is refused, which the caller already has to
//  handle for a share that cannot be watched at all.
//
//  EVERY CALLBACK ARRIVES ON THE SERVICE THREAD, so a handler does no more
//  than record. The re-read happens on the thread that owns the window.
//
//  A NETWORK FOLDER IS OPENED ON A THREAD OF ITS OWN. Opening one waits on
//  the share until the network times out on an unreachable one,
//  so Watch returns at once and the folder joins the set when its handle
//  arrives. That thread shares only the state below, so it can outlive the
//  watcher and close its handle itself.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FolderWatcher : public IFolderWatcher
{
public:

    Win32FolderWatcher () = default;
    ~Win32FolderWatcher () override;

    bool  Watch   (const std::wstring & directory, Callback callback) override;
    void  Unwatch (const std::wstring & directory) override;

    //  Folders watched at once, after the stop and wake handles are taken out
    //  of the wait's 64.
    static constexpr size_t  kMaxFolders = MAXIMUM_WAIT_OBJECTS - 2;

private:

    struct FolderEntry
    {
        HANDLE    notification = INVALID_HANDLE_VALUE;
        Callback  callback;
    };

    //  Waits on every notification at once, re-arming whichever fired. Runs on
    //  the one service thread.
    void  RunService ();

    //  Starts the service thread if it is not already running. Called with the
    //  lock held.
    void  EnsureService ();
    void  DropUnusable  (const std::vector<std::wstring> & directories);

    //  Adds a folder whose handle is open, closing the handle instead when the
    //  folder is already watched or the set is full.
    bool  AddFolder (const std::wstring & directory, HANDLE notification, Callback callback);

    void  Stop ();

    //  Network folders being opened, and whether the watcher is there to
    //  receive them.
    struct NetworkOpens
    {
        std::mutex              lock;
        bool                    alive = true;
        std::set<std::wstring>  pending;
    };

    std::mutex                                     m_lock;
    std::map<std::wstring, FolderEntry>            m_folders;
    std::wstring                                   m_calling;   // the folder whose callback is running
    std::condition_variable                        m_called;
    std::thread                                    m_service;
    HANDLE                                         m_stop    = nullptr;
    HANDLE                                         m_wake    = nullptr;
    std::shared_ptr<NetworkOpens>                  m_network = std::make_shared<NetworkOpens>();
};
