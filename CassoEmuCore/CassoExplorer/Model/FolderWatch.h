#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/IFolderWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch
//
//  The host folders the browser is showing, watched for changes: the folder in
//  the list and every folder the tree has open.
//
//  WHY IT EXISTS. Coming back to the window used to re-read the folder and
//  rebuild every row, whether or not anything had changed, which on a folder
//  of a few thousand files is a visible stall for nothing. Being told what
//  changed replaces that.
//
//  THE BURST IS COLLAPSED HERE. Copying a hundred files reports a hundred
//  changes, and each would otherwise be a re-read. A directory reported any
//  number of times between one call to TakeChanged and the next comes back
//  once, and the owner decides how long to wait before asking.
//
//  CHANGES ARRIVE ON THE WATCHER'S THREAD, so nothing happens there but
//  recording the directory and waking the owner. Everything else runs on the
//  thread that asked.
//
//  A DIRECTORY THAT CANNOT BE WATCHED IS NOT AN ERROR. A share that refuses
//  is simply not recorded, so the next call that lists it tries again.
//
////////////////////////////////////////////////////////////////////////////////

class FolderWatch
{
public:

    explicit FolderWatch (IFolderWatcher & watcher);
    ~FolderWatch ();

    FolderWatch (const FolderWatch &) = delete;
    FolderWatch &  operator= (const FolderWatch &) = delete;

    //  Woken on the watcher's thread when a watched directory changes. The
    //  handler does no more than post to the owner's thread.
    void  SetOnChanged (std::function<void()> onChanged);

    //  The directories to watch from now on. Ones newly listed are watched,
    //  ones no longer listed are dropped, and the rest are left as they are,
    //  so an unchanged tree costs nothing.
    void  SetWatched (const std::vector<std::wstring> & directories);

    //  The directories reported since this was last called, each once. False
    //  when nothing changed.
    bool  TakeChanged (std::vector<std::wstring> & outDirectories);

    const std::vector<std::wstring> &  GetWatched () const { return m_watched; }

private:

    //  Paths differ only by case on Windows, so one spelling is watched and
    //  compared throughout.
    static std::wstring  Normalize (const std::wstring & directory);

    //  Runs on the watcher's thread: notes the directory and wakes the owner.
    void  Record  (const std::wstring & directory);
    void  StopAll ();

    IFolderWatcher           & m_watcher;
    std::vector<std::wstring>  m_watched;
    std::function<void()>      m_onChanged;

    //  Guards the set below, which the watcher's thread writes and the owner's
    //  thread drains.
    mutable std::mutex         m_lock;
    std::set<std::wstring>     m_changed;
};
