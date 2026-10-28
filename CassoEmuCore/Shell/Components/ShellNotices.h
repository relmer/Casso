#pragma once

#include "Pch.h"

class EmulatorShell;
class ShellChrome;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellNotices
//
//  The transient notices: a screenshot's filename or the reason it failed,
//  which write-protect mechanism a Disk menu command changed, a controller
//  that left. Message bars across the top of the picture, hung under the
//  docked chrome, stacked in arrival order, each up for its full time.
//  ShowNotice is the UI thread's; PostNotice is any other thread's way in.
//
////////////////////////////////////////////////////////////////////////////////

class ShellNotices
{
public:
    ShellNotices (EmulatorShell & shell, ShellChrome & chrome) : m_shell (shell), m_chrome (chrome) {}

    void  ShowNotice (const std::wstring & text);
    void  PostNotice (const std::wstring & text);
    void  SyncNotice ();

private:
    friend class ShellChrome;

    EmulatorShell    & m_shell;
    ShellChrome      & m_chrome;
    DxuiNoticeStack    m_notices;
};
