#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackageWatcher
//
//  Calls a function on a worker thread after a package is installed, updated
//  or removed, or a file association changes. A package change appears as
//  changes under the AppModel package repository key, which lists every
//  installed package by full name. Installing a package changes the key many
//  times over a few seconds, so the call waits until the key has been quiet
//  for s_kQuietMs, and one call covers them all.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PackageWatcher
{
public:
    ~Win32PackageWatcher();

    //  The shell sends `assocMessage` to `notify` when a file association
    //  changes; its handler calls Refresh.
    void  Start   (HWND notify, UINT assocMessage, std::function<void ()> onChanged);
    void  Refresh ();

private:
    static constexpr DWORD  s_kQuietMs = 2000;

    void  Watch ();

    std::thread             m_watcher;
    std::function<void ()>  m_onChanged;
    HANDLE                  m_stop       = nullptr;
    HANDLE                  m_reread     = nullptr;
    ULONG                   m_assocWatch = 0;
};
