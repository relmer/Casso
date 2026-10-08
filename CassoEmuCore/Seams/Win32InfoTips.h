#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips
//
//  The shell's own tip for a file or folder, as Explorer shows it on hover:
//  type, size and date for a file; date created, size and some of the files
//  for a folder.
//
//  A FOLDER'S TIP COUNTS WHAT IS IN IT, which can take a while, so tips are
//  read on a thread of their own. Only the latest request is kept: the item
//  under the pointer is the only one wanted. The window is posted a message
//  when a tip is ready and reads it from the cache.
//
////////////////////////////////////////////////////////////////////////////////

class Win32InfoTips
{
public:
    Win32InfoTips() = default;
    ~Win32InfoTips();

    Win32InfoTips (const Win32InfoTips &)             = delete;
    Win32InfoTips & operator= (const Win32InfoTips &) = delete;

    //  Starts the thread; `message` goes to `hwnd` as each tip is ready.
    void  Start (HWND hwnd, UINT message);

    //  The tip for a path when it has been read; otherwise asks for it and
    //  answers false.
    bool  TryGet (const std::wstring & path, std::wstring & outTip);

    //  Forgets every tip, as a folder's change may have changed them.
    void  Clear();

private:
    void  Run();

    static std::wstring  Read (const std::wstring & path);

    HWND                                            m_hwnd     = nullptr;
    UINT                                            m_message  = 0;
    std::thread                                     m_thread;
    std::mutex                                      m_lock;
    std::condition_variable                         m_wake;
    std::wstring                                    m_wanted;
    std::unordered_map<std::wstring, std::wstring>  m_tips;
    bool                                            m_stopping = false;
};
