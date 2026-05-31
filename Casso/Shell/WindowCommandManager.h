#pragma once

#include "Pch.h"


class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowCommandManager
//
//  Owner of the Win32 command dispatch path: the public command pump
//  (HandleCommand), the WM_COMMAND id-range demux (OnCommand), every
//  per-menu-group OnFooCommand handler, the WM_INITMENUPOPUP menu
//  state-caching tick, and the file-open shell dialog for drive-
//  widget click-to-browse (PromptForDiskImage). Holds a back-reference
//  to EmulatorShell and is declared a friend of that class so it can
//  reach the shell members the command handlers operate on. No new
//  global state is added; the back-reference is the only coupling.
//
////////////////////////////////////////////////////////////////////////////////

class WindowCommandManager
{
public:
    explicit WindowCommandManager (EmulatorShell & shell);

    void  HandleCommand        (WORD commandId);
    bool  OnCommand            (HWND hwnd, int id);

    void  OnFileCommand        (int id);
    void  OnEditCommand        (int id);
    void  OnMachineCommand     (int id);
    void  OnViewCommand        (int id);
    void  OnDiskCommand        (int id);
    void  OnHelpCommand        (int id);

    bool  OnInitMenuPopup      (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu);

    HRESULT  PromptForDiskImage   (int drive);
    HRESULT  PromptInsertDiskMru  (int drive);

private:
    EmulatorShell &  m_shell;
};
