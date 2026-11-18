#include "Pch.h"

#include "DxuiDialogMode.h"
#include "IDxuiDialogHost.h"

#include "Core/DxuiFocusManager.h"
#include "Core/DxuiInputState.h"
#include "Core/DxuiPanel.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiInfoTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::ShowModal
//
//  Modal show: emphasize + auto-wire the command buttons, attach the
//  focus manager, disable the owner, and run a private message pump
//  until End() resolves a result (a command button click, Enter on the
//  default button, or Escape / close-box on IDCANCEL). Re-enables the
//  owner and returns the result. Mirrors Win32 DialogBox / EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialogMode::ShowModal (int defaultButtonId)
{
    HRESULT  hr            = S_OK;
    MSG      msg           = {};
    BOOL     gotMessage    = FALSE;
    bool     ownerDisabled = false;
    bool     isCreated     = m_host.IsDialogHostCreated();
    HWND     owner         = m_host.GetDialogOwnerHwnd();
    int      result        = IDCANCEL;



    CBRA (isCreated);

    Begin (defaultButtonId, true);

    m_host.SetDialogTimer (m_tickMs);

    if (owner != nullptr)
    {
        EnableWindow (owner, FALSE);
        ownerDisabled = true;
    }

    m_host.NotifyDialogShowing();
    m_host.ShowDialogWindow (true);

    while (!m_isDone)
    {
        gotMessage = GetMessageW (&msg, nullptr, 0, 0);

        if (gotMessage == 0)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        CWRA (gotMessage != -1);

        if (!ProcessMessage (msg))
        {
            TranslateMessage (&msg);
            DispatchMessageW  (&msg);
        }
    }

    result = m_result;

Error:
    if (ownerDisabled)
    {
        EnableWindow (owner, TRUE);

        //  When the active dialog is hidden, Windows chooses the next active
        //  window, which after an Alt+Tab away and back can be another
        //  program's. The owner is activated first, as DialogBox does.
        if (GetActiveWindow() == m_host.GetDialogHwnd())
        {
            SetActiveWindow (owner);
        }
    }

    //  The owner is activated before the dialog hides; otherwise Windows
    //  activates whatever window was active before the dialog, which can be
    //  another program the user switched to and back from.
    if (owner != nullptr && GetForegroundWindow() == m_host.GetDialogHwnd())
    {
        SetForegroundWindow (owner);
    }

    m_host.KillDialogTimer();

    m_isActive = false;
    m_isModal  = false;
    m_host.HideDialogWindow();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::ShowModeless
//
//  Modeless show: applies the same dialog behaviors as ShowModal (button
//  wiring, focus-manager Tab traversal, default-button emphasis, periodic
//  tick) but does NOT disable the owner and does NOT run a private loop --
//  it shows the window and returns immediately. The host's own message loop
//  drives it; dialog-key nav flows through the window's WndProc (all
//  controls live in one HWND, so no IsDialogMessage pump is needed). Close
//  via End(), which hides the window and fires the SetOnEnd callback.
//  Mirrors Win32 CreateDialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::ShowModeless (int defaultButtonId, bool activate)
{
    if (m_host.IsDialogHostCreated())
    {
        Begin (defaultButtonId, false);

        m_host.SetDialogTimer (m_tickMs);

        m_host.NotifyDialogShowing();
        m_host.ShowDialogWindow (activate);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::ProcessMessage
//
//  Dxui's IsDialogMessage analog for modeless dialogs. Intercepts only
//  the dialog-navigation keys for the window (Tab/Shift+Tab, Enter,
//  Escape) before TranslateMessage can manufacture a WM_CHAR or the
//  normal dispatch path can bypass the focus manager. Everything else
//  falls through to the host's standard Translate/Dispatch path.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogMode::ProcessMessage (const MSG & msg)
{
    bool  isHandled = false;
    HWND  hwnd      = m_host.GetDialogHwnd();



    // Only intercept the dialog-navigation keys while a dialog is active and
    // the message targets THIS window; otherwise fall through to normal
    // dispatch. A message aimed at a sibling window must not be swallowed.
    bool  isOurs = m_isActive && hwnd != nullptr && msg.hwnd == hwnd;

    if (isOurs)
    {
        switch (msg.message)
        {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (msg.wParam == VK_TAB || msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)
                {
                    isHandled = (DispatchKey (msg.wParam) == DxuiMessageResult::Handled);
                }

                break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::End
//
//  Resolves an open dialog. Modal: records the result once and flags the
//  private pump to exit (posts a no-op so a blocked GetMessage re-checks
//  the done flag) -- ShowModal returns the result. Modeless: no loop to
//  break, so it deactivates the dialog behaviors, hides the window, and
//  fires the SetOnEnd callback with the result. A command button, Enter /
//  Escape, or content code calls this. Mirrors Win32 EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::End (int result)
{
    HWND  hwnd = m_host.GetDialogHwnd();



    if (m_isActive)
    {
        if (m_isModal)
        {
            if (!m_isDone)
            {
                m_result = result;
                m_isDone = true;

                if (hwnd != nullptr)
                {
                    PostMessageW (hwnd, WM_NULL, 0, 0);
                }
            }
        }
        else
        {
            m_isActive = false;

            m_host.KillDialogTimer();
            m_host.HideDialogWindow();

            if (m_onEnd)
            {
                m_onEnd (result);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::SetTickIntervalMs
//
//  Re-arming a timer under the same id replaces it, so a dialog already
//  showing moves to the new cadence at once.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::SetTickIntervalMs (UINT ms)
{
    m_tickMs = ms;

    if (m_isActive)
    {
        m_host.SetDialogTimer (m_tickMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::DispatchKey
//
//  Dialog key routing (modal or modeless): Tab / Shift+Tab move focus;
//  Escape fires the IDCANCEL button (or resolves IDCANCEL directly);
//  Enter lets the focused control claim it first, else fires the default
//  button; any other key routes to the focused control.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiDialogMode::DispatchKey (WPARAM vk)
{
    DxuiMessageResult  result    = DxuiMessageResult::NotHandled;
    bool               shift     = DxuiInputState::IsKeyDown (VK_SHIFT);
    bool               isHandled = false;



    // A modal overlay owns every key, Tab / Enter / Escape included. The
    // modeless pre-translate path (ProcessMessage) arrives here without
    // passing through OnKeyDown, so the check has to live here too, or Tab
    // walks the focus of the page hidden behind the overlay.
    if (m_host.TrySendDialogOverlayKey (vk))
    {
        return DxuiMessageResult::Handled;
    }

    switch (vk)
    {
        case VK_TAB:
            // Ctrl(+Shift)+Tab cycles a tabbed window's pages (e.g.
            // DxuiPropertySheet); plain Tab traverses focus within the page.
            if (DxuiInputState::IsKeyDown (VK_CONTROL) && m_host.TrySwitchDialogTab (shift))
            {
                isHandled = true;
                break;
            }

            // The focused control receives Tab FIRST, as it already receives
            // Enter. A control with internal stops, such as a hex view's two
            // columns, returns true while it has another stop and false after
            // the last, and the focus manager then moves focus to the next
            // control.
            isHandled = RouteKeyToFocused (vk, shift)
                     || m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            break;

        case VK_ESCAPE:
            // The focused control gets Escape first, so an open drop-down
            // closes rather than the dialog, and then an open info tip does.
            // Escape cancels the dialog only when neither of those consumes it.
            if (RouteKeyToFocused (vk, shift) || DxuiInfoTip::CloseOpen())
            {
                isHandled = true;
                m_host.InvalidateDialog();
                break;
            }

            if (!TryTriggerButton (IDCANCEL))
            {
                End (IDCANCEL);
            }

            isHandled = true;
            break;

        case VK_RETURN:
            isHandled = RouteKeyToFocused (vk, shift) || TryTriggerButton (m_defaultButtonId);
            break;

        default:
            // An arrow key is the keyboard in use, so a control focused by a
            // click shows its focus rectangle from here on.
            if ((vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) && m_focus.GetFocusedControl() != nullptr)
            {
                m_focus.GetFocusedControl()->SetFocusCueVisible (true);
            }

            isHandled = RouteKeyToFocused (vk, shift);
            break;
    }

    // Any consumed dialog key (Tab focus move, arrow-key slider nudge, etc.)
    // repaints so the change shows at once rather than on the next tick.
    if (isHandled)
    {
        m_host.NotifyDialogKeyHandled (m_focus.GetFocusedControl());
        m_host.InvalidateDialog();
    }

    result = isHandled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::Begin
//
//  Shared setup for modal and modeless dialogs: remember the default
//  button, clear any stale modal result, wire the buttons, and attach /
//  rebuild the focus manager so Tab traversal, Enter, and Escape all
//  run through the dialog contract.
//
//  The order it builds is a SNAPSHOT; a subclass that later shows or hides
//  part of the tree calls DxuiWindow::RefreshFocusOrder.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::Begin (int defaultButtonId, bool modal)
{
    m_defaultButtonId = defaultButtonId;
    m_result          = IDCANCEL;
    m_isDone          = false;
    m_isModal         = modal;
    m_isActive        = true;

    WireButtons();

    m_focus.SetTheme (m_host.GetDialogTheme());
    m_focus.Attach   (&m_root);
    m_focus.Rebuild();

    if (m_initialFocus != nullptr)
    {
        m_focus.SetFocused (m_initialFocus);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::WireButtons
//
//  Walks the panel tree: emphasizes the default-id command button and,
//  for every command button (nonzero id) lacking a custom click handler,
//  installs a click that calls EndDialog(its id). Buttons that must stay
//  open (Download, Apply) carry their own SetOnClick and are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::WireButtons()
{
    int  defaultId = m_defaultButtonId;



    ForEachButton (&m_root, [this, defaultId] (DxuiButton * button)
    {
        int  id = button->GetCommandId();


        button->SetEmphasis (id != 0 && id == defaultId);

        if (id != 0 && !button->HasClickHandler())
        {
            button->SetOnClick ([this, id] () { End (id); });
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::RouteKeyToFocused
//
//  Forwards a key-down to the focused control as a DxuiKeyEvent, with the
//  Ctrl and Alt keys as they are now. Returns true iff a control has focus
//  and consumed the key.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogMode::RouteKeyToFocused (WPARAM vk, bool shift)
{
    HRESULT         hr        = S_OK;
    bool            isHandled = false;
    IDxuiControl *  focused   = m_focus.GetFocusedControl();
    DxuiKeyEvent    ke        = DxuiInputState::MakeKeyEvent (DxuiKeyEventKind::Down, vk);



    BAIL_OUT_IF (focused == nullptr, S_OK);

    ke.shift  = shift;
    isHandled = focused->OnKey (ke);

Error:
    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::TryTriggerButton
//
//  Finds and clicks the enabled / visible button with the given command
//  id. Returns true iff such a button exists and was fired.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogMode::TryTriggerButton (int commandId)
{
    bool          fired  = false;
    DxuiButton *  button = FindButtonById (&m_root, commandId);



    if (button != nullptr && button->IsEnabled() && button->IsVisible())
    {
        button->Click();
        fired = true;
    }

    return fired;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::FindButtonById
//
//  Recursively returns the first button in the tree whose command id
//  matches, or null. A zero commandId never matches (0 = not a command
//  button).
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiDialogMode::FindButtonById (IDxuiControl * node, int commandId)
{
    HRESULT       hr    = S_OK;
    DxuiButton *  found = nullptr;
    DxuiButton *  self  = nullptr;
    size_t        count = 0;
    size_t        i     = 0;



    BAIL_OUT_IF (node == nullptr || commandId == 0, S_OK);

    self = dynamic_cast<DxuiButton *> (node);

    if (self != nullptr && self->GetCommandId() == commandId)
    {
        found = self;
        BAIL_OUT_IF (true, S_OK);
    }

    count = node->GetChildCount();

    for (i = 0; i < count; ++i)
    {
        found = FindButtonById (node->GetChild (i), commandId);

        if (found != nullptr)
        {
            break;
        }
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode::ForEachButton
//
//  Recursively invokes fn for every DxuiButton in the tree rooted at node.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogMode::ForEachButton (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn)
{
    HRESULT       hr    = S_OK;
    DxuiButton *  self  = nullptr;
    size_t        count = 0;
    size_t        i     = 0;



    BAIL_OUT_IF (node == nullptr, S_OK);

    self = dynamic_cast<DxuiButton *> (node);

    if (self != nullptr)
    {
        fn (self);
    }

    count = node->GetChildCount();

    for (i = 0; i < count; ++i)
    {
        ForEachButton (node->GetChild (i), fn);
    }

Error:
    return;
}
