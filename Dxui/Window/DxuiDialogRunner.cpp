#include "Pch.h"

#include "Window/DxuiDialogRunner.h"
#include "Window/DxuiDialogController.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiKeyRouter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner::RunModal
//
//  The owner is disabled and a private message loop runs until End resolves
//  a result: a command button, Enter on the default button, or Escape or the
//  close box for IDCANCEL. The owner is enabled again before the result is
//  returned.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialogRunner::RunModal (int defaultButtonId, HWND owner)
{
    HRESULT           hr            = S_OK;
    DxuiHwndSource  * source        = m_host.GetDialogSource();
    MSG               msg           = {};
    BOOL              gotMessage    = FALSE;
    bool              ownerDisabled = false;
    int               result        = IDCANCEL;



    CBRA (source != nullptr);

    m_dialog.Begin (m_host.GetDialogTheme(), defaultButtonId, true, m_initialFocus);
    hr = source->SetTimer (kDialogTimerId, m_tickMs);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (owner != nullptr)
    {
        EnableWindow (owner, FALSE);
        ownerDisabled = true;
    }

    m_host.NotifyDialogShowing();
    m_host.ShowDialogWindow (true);

    while (!m_dialog.IsModalDone())
    {
        gotMessage = GetMessageW (&msg, nullptr, 0, 0);

        if (gotMessage == 0)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        CWRA (gotMessage != -1);

        if (!m_keys.ProcessDialogMessage (msg))
        {
            TranslateMessage (&msg);
            DispatchMessageW  (&msg);
        }
    }

    result = m_dialog.GetModalResult();

Error:
    if (ownerDisabled)
    {
        EnableWindow (owner, TRUE);

        //  When the active dialog is hidden, Windows chooses the next active
        //  window, which after an Alt+Tab away and back can be another
        //  program's. The owner is activated first, as DialogBox does.
        if (source != nullptr && GetActiveWindow() == source->GetHwnd())
        {
            SetActiveWindow (owner);
        }
    }

    if (source != nullptr)
    {
        hr = source->KillTimer (kDialogTimerId);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_dialog.Deactivate();
    m_host.HideDialogWindow();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner::ShowModeless
//
//  The dialog behaviors of RunModal without disabling the owner or running a
//  loop: the host's message loop drives the window.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogRunner::ShowModeless (int defaultButtonId, bool activate)
{
    HRESULT           hr     = S_OK;
    DxuiHwndSource  * source = m_host.GetDialogSource();



    if (source == nullptr)
    {
        return;
    }

    m_dialog.Begin (m_host.GetDialogTheme(), defaultButtonId, false, m_initialFocus);
    hr = source->SetTimer (kDialogTimerId, m_tickMs);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_host.NotifyDialogShowing();
    m_host.ShowDialogWindow (activate);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner::End
//
//  A modal dialog's loop is told to exit, with a no-op message so a blocked
//  GetMessage checks again. A modeless dialog is hidden and its close
//  callback gets the result.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogRunner::End (int result)
{
    HRESULT                hr     = S_OK;
    DxuiHwndSource       * source = m_host.GetDialogSource();
    HWND                   hwnd   = source != nullptr ? source->GetHwnd() : nullptr;
    DxuiDialogEndAction    action = m_dialog.End (result);



    if (action == DxuiDialogEndAction::ExitModalLoop && hwnd != nullptr)
    {
        PostMessageW (hwnd, WM_NULL, 0, 0);
    }
    else if (action == DxuiDialogEndAction::CloseModeless)
    {
        if (source != nullptr)
        {
            hr = source->KillTimer (kDialogTimerId);
        IGNORE_RETURN_VALUE (hr, S_OK);
        }

        m_host.HideDialogWindow();

        if (m_onDialogEnd)
        {
            m_onDialogEnd (result);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogRunner::OnTimer (UINT_PTR timerId)
{
    if (!m_dialog.IsActive() || timerId != kDialogTimerId)
    {
        return false;
    }

    m_host.NotifyDialogTick();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner::SetTickIntervalMs
//
//  Arming the timer again under the same id replaces it, so a dialog already
//  showing moves to the new cadence at once.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogRunner::SetTickIntervalMs (UINT ms)
{
    HRESULT           hr     = S_OK;
    DxuiHwndSource  * source = m_host.GetDialogSource();



    m_tickMs = ms;

    if (m_dialog.IsActive() && source != nullptr)
    {
        hr = source->SetTimer (kDialogTimerId, m_tickMs);
    IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
