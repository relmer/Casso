#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellVerbsTypes
//
//  The types Win32ShellItemVerbs and its parts share. Win32ShellItemVerbs derives from it, so
//  each is also Win32ShellItemVerbs::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct Win32ShellVerbsTypes
{
    //  The shell's confirmation and property dialogs center on the screen;
    //  while one of these is alive, each dialog that opens on this thread is
    //  moved over the window that asked for it instead.
    class CenterDialogsOver
    {
    public:
        explicit CenterDialogsOver (HWND owner);
        ~CenterDialogsOver();

        CenterDialogsOver (const CenterDialogsOver &)             = delete;
        CenterDialogsOver & operator= (const CenterDialogsOver &) = delete;

    private:
        static LRESULT CALLBACK  OnCbt (int code, WPARAM wParam, LPARAM lParam);

        static thread_local HWND  s_owner;
        HHOOK                     m_hook  = nullptr;
        HWND                      m_prior = nullptr;
    };
};
