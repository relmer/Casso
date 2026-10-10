#pragma once

#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/IDebuggerHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DebuggerHost
//
//  The shell's side of IDebuggerHost: each call is one thing the shell does
//  for the debugger it holds. A class of the shell's own, so the shell takes
//  on no base class for it, and only EmulatorShell.cpp includes this header.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell::DebuggerHost : public IDebuggerHost
{
public:
    explicit DebuggerHost (EmulatorShell & shell) : m_shell (shell) {}

    HWND                    GetMainWindow       () const override;
    HINSTANCE               GetInstance         () const override;
    const CassoTheme      & GetChromeTheme      () const override;
    const std::wstring    & GetTitlePrefix      () const override;
    void                    ApplyAppIcon        (HWND window) override;
    void                    PresentUiFrame      () override;
    void                    UpdateWindowTitle   () override;
    HRESULT                 SetUiTimer          (UINT_PTR id, UINT ms) override;
    HRESULT                 KillUiTimer         (UINT_PTR id) override;
    void                    PostCommand         (WORD id, const std::string & payload) override;
    void                    DrawFrame           () override;
    void                    RedrawStoppedFrame  () override;
    void                    PrepareFramebuffers () override;
    IFileSystem           & GetUiFileSystem     () override;
    IHostDialogs          & GetHostDialogs      () override;
    WindowPlacementProfile  GetWindowPlacements () override;
    void                    SaveSettings        () override;
    void                    PersistSwitchState  (const char * key, bool value) override;
    HeldHostInputs        & GetHeldHostInputs   () override;
    void                    TruncatePaste       (size_t length) override;

private:
    EmulatorShell  & m_shell;
};
