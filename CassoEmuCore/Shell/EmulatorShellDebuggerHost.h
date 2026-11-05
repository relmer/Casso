#pragma once

#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Debugger/IMachineControl.h"
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

class EmulatorShell::DebuggerHost : public IDebuggerHost, public IMachineControl
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
    void                    BeginModalKeepAlive () override;
    void                    EndModalKeepAlive   () override;
    void                    PostCommand         (WORD id, const std::string & payload) override;
    void                    DrawFrame           () override;
    void                    RedrawStoppedFrame  () override;
    void                    PrepareFramebuffers () override;
    IFileSystem           & GetUiFileSystem     () override;
    IHostDialogs          & GetHostDialogs      () override;
    WindowPlacementProfile  GetWindowPlacements () override;
    void                    ShowFileInFolder    (const std::wstring & path) override;
    void                    SaveSettings        () override;
    void                    PersistSwitchState  (const char * key, bool value) override;
    HeldHostInputs        & GetHeldHostInputs   () override;
    void                    TruncatePaste       (size_t length) override;
    ITapeControl          & GetTapeControl      () override;
    IMachineControl       & GetMachineControl   () override;

    void                    Reset               (bool isPowerCycle) override;
    void                    InsertDisk          (int drive, const std::wstring & path) override;
    void                    EjectDisk           (int drive) override;
    void                    PasteText           (const std::wstring & text) override;
    bool                    GetScreenText       (int & columns, std::vector<std::string> & rows) override;
    HRESULT                 SaveScreenshot      (const std::wstring & path, int & width, int & height) override;
    void                    Exit                () override;

private:
    EmulatorShell  & m_shell;
};
