#include "Pch.h"

#include "CassoExplorer/ExplorerUpdaterHost.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "Config/IFileSystem.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Shell/Components/ShellUpdater.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Update/UpdateResult.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ExplorerUpdaterHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerUpdaterHost::ExplorerUpdaterHost (IFileSystem & fs, const std::wstring & baseDir, const std::wstring & titlePrefix)
    : m_fs          (fs),
      m_store       (baseDir),
      m_titlePrefix (titlePrefix),
      m_updater     (std::make_unique<ShellUpdater> (*this))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::~ExplorerUpdaterHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerUpdaterHost::~ExplorerUpdaterHost()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::Create
//
//  With a prefs file that cannot be read the defaults apply, and the first save
//  then fails rather than writing over it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUpdaterHost::Create (HINSTANCE instance)
{
    HRESULT      hr       = S_OK;
    HRESULT      hrRead   = m_store.ReadGlobalPrefs (m_fs, m_prefs);
    WNDCLASSEXW  wc       = { sizeof (wc) };
    ATOM         atom     = 0;



    IGNORE_RETURN_VALUE (hrRead, S_OK);

    m_instance = instance;

    wc.lpfnWndProc   = &ExplorerUpdaterHost::WindowProc;
    wc.hInstance     = instance;
    wc.lpszClassName = kpszMessageClass;

    atom = RegisterClassExW (&wc);
    CWRA (atom != 0);

    m_messageWindow = CreateWindowExW (0, kpszMessageClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
    CWRA (m_messageWindow != nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::Destroy
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::Destroy()
{
    m_updater->StopUpdateService();

    if (m_messageWindow != nullptr)
    {
        DestroyWindow (m_messageWindow);
        m_messageWindow = nullptr;

        UnregisterClassW (kpszMessageClass, m_instance);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::PostAutomaticCheck
//
//  Posted, so the first windows paint before anything about updates runs.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::PostAutomaticCheck()
{
    PostMessageW (m_messageWindow, (UINT) AppMessage::ExplorerUpdateStart, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::SetWindow
//
//  Moves the indicator to `window`'s caption, or takes it out of the caption
//  of the one that is closing when no window remains.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::SetWindow (CassoExplorerWindow * window)
{
    if (window == m_window)
    {
        return;
    }

    if (m_window != nullptr)
    {
        m_window->HideUpdateTip();
        m_window->GetPopupHost()->SetCaptionAccessory (nullptr);
    }

    m_window = window;

    if (m_window != nullptr)
    {
        m_updater->RebindIndicator();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::GetOwnerWindow
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerUpdaterHost::GetOwnerWindow()
{
    return (m_window != nullptr) ? m_window->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::GetDialogTheme
//
////////////////////////////////////////////////////////////////////////////////

const IDxuiTheme * ExplorerUpdaterHost::GetDialogTheme()
{
    return (m_window != nullptr) ? m_window->GetDialogTheme() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::SavePrefs
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::SavePrefs()
{
    HRESULT  hr = m_store.SaveUpdateFields (m_prefs, m_fs);



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ReadStoredPrefs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUpdaterHost::ReadStoredPrefs (GlobalUserPrefs & outPrefs)
{
    return m_store.ReadGlobalPrefs (m_fs, outPrefs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::HasCaption
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerUpdaterHost::HasCaption()
{
    return m_window != nullptr && m_window->GetCaptionHeightPx() > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::TryGetClientWidthPx
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerUpdaterHost::TryGetClientWidthPx (int & outWidthPx)
{
    RECT  client   = {};
    bool  hasWidth = m_window != nullptr && GetClientRect (m_window->GetHwnd(), &client) != FALSE;



    outWidthPx = client.right - client.left;

    return hasWidth;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::GetDpi
//
////////////////////////////////////////////////////////////////////////////////

UINT ExplorerUpdaterHost::GetDpi()
{
    return (m_window != nullptr) ? m_window->GetDpi() : USER_DEFAULT_SCREEN_DPI;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::SetCaptionAccessory
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::SetCaptionAccessory (IDxuiControl * accessory)
{
    if (m_window != nullptr)
    {
        m_window->GetPopupHost()->SetCaptionAccessory (accessory);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::SetCaptionAccessoryWidth
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::SetCaptionAccessoryWidth (int widthDip)
{
    if (m_window != nullptr)
    {
        m_window->GetPopupHost()->SetCaptionAccessoryWidth (widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::GetCaptionReservedWidthDip
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerUpdaterHost::GetCaptionReservedWidthDip()
{
    return (m_window != nullptr) ? m_window->GetPopupHost()->GetCaptionReservedWidthDip() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::RequestRepaint
//
//  Every change to the indicator comes through here, so this also starts the
//  tick that drives a visible indicator.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::RequestRepaint()
{
    if (m_window != nullptr)
    {
        m_window->Invalidate();
    }

    if (!m_isTickArmed && m_updater->IsIndicatorVisible())
    {
        ArmIndicatorTick (kFrameMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ShowIndicatorTip
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::ShowIndicatorTip (const RECT & anchorPx, const std::wstring & text, int64_t nowMs)
{
    UNREFERENCED_PARAMETER (nowMs);

    if (m_window != nullptr)
    {
        m_window->ShowUpdateTip (anchorPx, text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::HideIndicatorTip
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::HideIndicatorTip()
{
    if (m_window != nullptr)
    {
        m_window->HideUpdateTip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ShowNotice
//
//  The window has no notice band, so a notice is a message box.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::ShowNotice (const std::wstring & text)
{
    if (m_window != nullptr)
    {
        m_window->ShowMessage (text, MB_ICONINFORMATION);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ShowNotification
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::ShowNotification (const std::wstring & text)
{
    if (m_window != nullptr)
    {
        m_window->ShowMessage (text, MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ShowModalDialog
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerUpdaterHost::ShowModalDialog (const DialogDefinition & def)
{
    return MessageDialog::ShowDefinition (def, m_instance, GetOwnerWindow(), GetDialogTheme(), nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::StartSharedCheckTimer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerUpdaterHost::StartSharedCheckTimer (UINT periodMs)
{
    HRESULT    hr    = S_OK;
    UINT_PTR   timer = SetTimer (m_messageWindow, (UINT_PTR) AppTimerId::ShellSharedCheck, periodMs, nullptr);



    CWR (timer != 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::StopSharedCheckTimer
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::StopSharedCheckTimer()
{
    BOOL  isKilled = KillTimer (m_messageWindow, (UINT_PTR) AppTimerId::ShellSharedCheck);



    IGNORE_RETURN_VALUE (isKilled, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::PrepareForDeploy
//
//  The session is written as each window changes, so only the last state is
//  left to save.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::PrepareForDeploy()
{
    if (m_window != nullptr)
    {
        m_window->SaveSession();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::RequestClose
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::RequestClose()
{
    if (m_window != nullptr)
    {
        PostMessageW (m_window->GetHwnd(), (UINT) AppMessage::ExplorerRunCommand, (WPARAM) CassoExplorerCommands::kExit, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::GetRelaunchArguments
//
//  The label only. The window and path of this launch come back in the session.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerUpdaterHost::GetRelaunchArguments()
{
    std::vector<std::wstring>  arguments;



    if (!m_titlePrefix.empty())
    {
        arguments.push_back (L"--title");
        arguments.push_back (m_titlePrefix);
    }

    return arguments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::ArmIndicatorTick
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::ArmIndicatorTick (UINT delayMs)
{
    m_isTickArmed = SetTimer (m_messageWindow, (UINT_PTR) AppTimerId::ExplorerUpdateIndicator, delayMs, nullptr) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::HandleIndicatorTick
//
//  Frames during a sweep, a wait for the next sweep between them, and no timer
//  once the indicator is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerUpdaterHost::HandleIndicatorTick()
{
    int64_t  nowMs    = (int64_t) GetTickCount64();
    bool     isSweep  = m_updater->TickUpdateIndicator (nowMs);
    int64_t  delayMs  = kFrameMs;
    BOOL     isKilled = FALSE;



    if (isSweep && m_window != nullptr)
    {
        m_window->Invalidate();
    }

    if (!m_updater->IsIndicatorVisible())
    {
        isKilled = KillTimer (m_messageWindow, (UINT_PTR) AppTimerId::ExplorerUpdateIndicator);
        IGNORE_RETURN_VALUE (isKilled, TRUE);

        m_isTickArmed = false;
    }
    else
    {
        if (!isSweep)
        {
            delayMs = std::clamp (m_updater->GetMsUntilShimmer (nowMs).value_or (kIdleTickMs), (int64_t) kFrameMs, kIdleTickMs);
        }

        ArmIndicatorTick ((UINT) delayMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost::WindowProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK ExplorerUpdaterHost::WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ExplorerUpdaterHost  * self = reinterpret_cast<ExplorerUpdaterHost *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));



    if (message == WM_NCCREATE)
    {
        self = static_cast<ExplorerUpdaterHost *> (reinterpret_cast<CREATESTRUCTW *> (lParam)->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (self));
    }

    if (self == nullptr)
    {
        return DefWindowProcW (hwnd, message, wParam, lParam);
    }

    if (message == (UINT) AppMessage::ShellUpdateResult)
    {
        std::unique_ptr<UpdateResult>  result (reinterpret_cast<UpdateResult *> (lParam));

        if (result != nullptr)
        {
            self->m_updater->HandleUpdateResult (*result);
        }

        return 0;
    }

    if (message == (UINT) AppMessage::ExplorerUpdateStart)
    {
        self->m_updater->StartAutomaticUpdateCheck();
        return 0;
    }

    if (message == WM_TIMER && wParam == (WPARAM) AppTimerId::ShellSharedCheck)
    {
        self->m_updater->PollSharedCheckRecord();
        return 0;
    }

    if (message == WM_TIMER && wParam == (WPARAM) AppTimerId::ExplorerUpdateIndicator)
    {
        self->HandleIndicatorTick();
        return 0;
    }

    return DefWindowProcW (hwnd, message, wParam, lParam);
}
