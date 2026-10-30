#include "Pch.h"

#include "Shell/Components/EmulatorUpdaterHost.h"
#include "CommandLineParser.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Settings/SettingsSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::EmulatorUpdaterHost
//
////////////////////////////////////////////////////////////////////////////////

EmulatorUpdaterHost::EmulatorUpdaterHost (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetResultWindow
//
////////////////////////////////////////////////////////////////////////////////

HWND EmulatorUpdaterHost::GetResultWindow()
{
    return m_shell.m_hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetOwnerWindow
//
////////////////////////////////////////////////////////////////////////////////

HWND EmulatorUpdaterHost::GetOwnerWindow()
{
    return m_shell.m_hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetInstance
//
////////////////////////////////////////////////////////////////////////////////

HINSTANCE EmulatorUpdaterHost::GetInstance()
{
    return m_shell.m_hInstance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetDialogTheme
//
////////////////////////////////////////////////////////////////////////////////

const IDxuiTheme * EmulatorUpdaterHost::GetDialogTheme()
{
    return &m_shell.m_chrome->GetTheme();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

GlobalUserPrefs & EmulatorUpdaterHost::GetPrefs()
{
    return m_shell.m_settings->GetPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::SavePrefs
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::SavePrefs()
{
    HRESULT            hr    = S_OK;
    UserConfigStore  * store = m_shell.m_settings->GetConfigStore();



    if (store != nullptr)
    {
        hr = store->SaveUpdateFields (m_shell.m_settings->GetPrefs(), m_shell.m_settings->GetFileSystem());
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::ReadStoredPrefs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorUpdaterHost::ReadStoredPrefs (GlobalUserPrefs & outPrefs)
{
    HRESULT            hr    = S_OK;
    UserConfigStore  * store = m_shell.m_settings->GetConfigStore();



    CBRA (store != nullptr);

    hr = store->ReadGlobalPrefs (m_shell.m_settings->GetFileSystem(), outPrefs);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::HasCaption
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorUpdaterHost::HasCaption()
{
    return m_shell.m_host != nullptr && m_shell.m_host->GetCaptionHeightPx() > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::TryGetClientWidthPx
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorUpdaterHost::TryGetClientWidthPx (int & outWidthPx)
{
    RECT  client   = {};
    bool  hasWidth = m_shell.m_host != nullptr && m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &client) != FALSE;



    outWidthPx = client.right - client.left;

    return hasWidth;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetDpi
//
////////////////////////////////////////////////////////////////////////////////

UINT EmulatorUpdaterHost::GetDpi()
{
    return m_shell.m_scaler.GetDpi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::SetCaptionAccessory
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::SetCaptionAccessory (IDxuiControl * accessory)
{
    if (m_shell.m_host != nullptr)
    {
        m_shell.m_host->SetCaptionAccessory (accessory);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::SetCaptionAccessoryWidth
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::SetCaptionAccessoryWidth (int widthDip)
{
    if (m_shell.m_host != nullptr)
    {
        m_shell.m_host->SetCaptionAccessoryWidth (widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetCaptionReservedWidthDip
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorUpdaterHost::GetCaptionReservedWidthDip()
{
    return (m_shell.m_host != nullptr) ? m_shell.m_host->GetCaptionReservedWidthDip() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::RequestRepaint
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::RequestRepaint()
{
    if (m_shell.m_hwnd != nullptr)
    {
        InvalidateRect (m_shell.m_hwnd, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::ShowIndicatorTip
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::ShowIndicatorTip (const RECT & anchorPx, const std::wstring & text, int64_t nowMs)
{
    m_shell.m_chrome->GetCaptionTooltip().RequestShow (anchorPx, text.c_str(), nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::HideIndicatorTip
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::HideIndicatorTip()
{
    m_shell.m_chrome->GetCaptionTooltip().HideImmediate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::ShowNotice
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::ShowNotice (const std::wstring & text)
{
    m_shell.m_chrome->ShowNotice (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::ShowNotification
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::ShowNotification (const std::wstring & text)
{
    m_shell.ShowNotification (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::ShowModalDialog
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorUpdaterHost::ShowModalDialog (const DialogDefinition & def)
{
    return m_shell.ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::RefreshUpdateStatus
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::RefreshUpdateStatus()
{
    if (m_shell.m_settings->GetSheet() != nullptr)
    {
        m_shell.m_settings->GetSheet()->RefreshUpdateStatus();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::StartSharedCheckTimer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorUpdaterHost::StartSharedCheckTimer (UINT periodMs)
{
    HRESULT  hr = S_OK;



    CBR (m_shell.m_host != nullptr);

    hr = m_shell.m_host->SetTimer ((UINT_PTR) AppTimerId::ShellSharedCheck, periodMs);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::StopSharedCheckTimer
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::StopSharedCheckTimer()
{
    HRESULT  hr = S_OK;



    if (m_shell.m_host != nullptr)
    {
        hr = m_shell.m_host->KillTimer ((UINT_PTR) AppTimerId::ShellSharedCheck);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::PrepareForDeploy
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::PrepareForDeploy()
{
    HRESULT  hr = m_shell.m_machine.GetDiskStore().FlushAllForShutdown();



    IGNORE_RETURN_VALUE (hr, S_OK);

    m_shell.m_settings->FlushDeferredGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::RequestClose
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorUpdaterHost::RequestClose()
{
    PostMessageW (m_shell.m_hwnd, WM_CLOSE, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost::GetRelaunchArguments
//
//  The options this process was started with that a relaunch after an update
//  repeats, as CommandLineParser selects them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> EmulatorUpdaterHost::GetRelaunchArguments()
{
    int                        argc  = 0;
    LPWSTR                   * argvW = CommandLineToArgvW (GetCommandLineW(), &argc);
    std::vector<std::string>   narrow;
    std::vector<char *>        argv;
    std::vector<std::string>   kept;
    std::vector<std::wstring>  wide;



    // argv[0] is the program itself.
    for (int i = 1; argvW != nullptr && i < argc; i++)
    {
        narrow.push_back (TextEncoding::WideToUtf8 (argvW[i]));
    }

    if (argvW != nullptr)
    {
        LocalFree (argvW);
    }

    for (std::string & arg : narrow)
    {
        argv.push_back (arg.data());
    }

    kept = CommandLineParser::SelectRelaunchArguments ((int) argv.size(), argv.data());

    for (const std::string & arg : kept)
    {
        wide.push_back (TextEncoding::Utf8ToWide (arg));
    }

    return wide;
}
