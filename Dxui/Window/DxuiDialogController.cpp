#include "Pch.h"

#include "Window/DxuiDialogController.h"

#include "Core/DxuiEvents.h"
#include "Core/DxuiInputState.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiInfoTip.h"
#include "Window/IDxuiDialogHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::DxuiDialogController
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialogController::DxuiDialogController (IDxuiDialogHost & host, DxuiFocusManager & focus, DxuiPanel & root)
    : m_host  (host),
      m_focus (focus),
      m_root  (root)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::Begin
//
//  Shared setup for modal and modeless dialogs: record the default button,
//  clear any stale modal result, wire the buttons, and attach / rebuild the
//  focus manager so Tab traversal, Enter, and Escape all run through the
//  dialog contract.
//
//  The order it builds is a SNAPSHOT; a window that later hides or reveals
//  part of the tree rebuilds it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogController::Begin (const IDxuiTheme * theme, int defaultButtonId, bool modal, IDxuiControl * initialFocus)
{
    m_defaultButtonId = defaultButtonId;
    m_modalResult     = IDCANCEL;
    m_isModalDone     = false;
    m_isModal         = modal;
    m_isActive        = true;

    WireButtons();

    m_focus.SetTheme (theme);
    m_focus.Attach   (&m_root);
    m_focus.Rebuild();

    if (initialFocus != nullptr)
    {
        m_focus.SetFocused (initialFocus);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::End
//
//  Resolves an open dialog. A modal dialog stores the result once, and the
//  return value is ExitModalLoop that once, so the window ends its private
//  loop. A modeless dialog is deactivated, and the return value is
//  CloseModeless, so the window hides itself and reports the result. With no
//  dialog open, or a modal dialog already ended, nothing is left to do and
//  the value is DxuiDialogEndAction::None.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialogEndAction DxuiDialogController::End (int result)
{
    DxuiDialogEndAction  action = DxuiDialogEndAction::None;



    if (m_isActive)
    {
        if (m_isModal)
        {
            if (!m_isModalDone)
            {
                m_modalResult = result;
                m_isModalDone = true;
                action        = DxuiDialogEndAction::ExitModalLoop;
            }
        }
        else
        {
            m_isActive = false;
            action     = DxuiDialogEndAction::CloseModeless;
        }
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::Deactivate
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogController::Deactivate()
{
    m_isActive = false;
    m_isModal  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::HandleKey
//
//  Dialog key routing (modal or modeless): Tab / Shift+Tab move focus;
//  Escape fires the IDCANCEL button (or resolves IDCANCEL directly);
//  Enter lets the focused control claim it first, else fires the default
//  button; any other key routes to the focused control. Returns true when
//  the key was consumed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogController::HandleKey (WPARAM vk, bool shift, bool ctrl)
{
    bool  isHandled = false;



    switch (vk)
    {
        case VK_TAB:
            // Ctrl(+Shift)+Tab cycles a tabbed window's pages (e.g.
            // DxuiPropertySheet); plain Tab traverses focus within the page.
            if (ctrl && m_host.OnDialogTabSwitch (shift))
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
                m_host.Invalidate();
                break;
            }

            if (!TryClickButton (IDCANCEL))
            {
                m_host.EndDialog (IDCANCEL);
            }

            isHandled = true;
            break;

        case VK_RETURN:
            isHandled = RouteKeyToFocused (vk, shift) || TryClickButton (m_defaultButtonId);
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
        m_host.OnDialogKeyHandled (m_focus.GetFocusedControl());
        m_host.Invalidate();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::TryClickButton
//
//  Finds and clicks the enabled / visible button with the given command
//  id. Returns true iff such a button exists and was fired.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogController::TryClickButton (int commandId)
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
//  DxuiDialogController::WireButtons
//
//  Walks the panel tree: emphasizes the default-id command button and,
//  for every command button (nonzero id) lacking a custom click handler,
//  installs a click that calls EndDialog(its id). Buttons that must stay
//  open (Download, Apply) carry their own SetOnClick and are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogController::WireButtons()
{
    int  defaultId = m_defaultButtonId;



    ForEachButton (&m_root, [this, defaultId] (DxuiButton * button)
    {
        int  id = button->GetCommandId();


        button->SetEmphasis (id != 0 && id == defaultId);

        if (id != 0 && !button->HasClickHandler())
        {
            button->SetOnClick ([this, id] () { m_host.EndDialog (id); });
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::RouteKeyToFocused
//
//  Forwards a key-down to the focused control as a DxuiKeyEvent, with the
//  Ctrl and Alt keys as they are now. Returns true iff a control has focus
//  and consumed the key.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogController::RouteKeyToFocused (WPARAM vk, bool shift)
{
    HRESULT         hr        = S_OK;
    bool            isHandled = false;
    IDxuiControl *  focused   = m_focus.GetFocusedControl();
    DxuiKeyEvent    ke        = DxuiInputState::MakeKeyEvent (DxuiKeyEventKind::Down, vk);



    BAIL_OUT_IF (focused == nullptr, S_OK);

    ke.shift = shift;

    isHandled = focused->OnKey (ke);

Error:
    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController::FindButtonById
//
//  Recursively returns the first button in the tree whose command id
//  matches, or null. A zero commandId never matches (0 = not a command
//  button).
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiDialogController::FindButtonById (IDxuiControl * node, int commandId)
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
//  DxuiDialogController::ForEachButton
//
//  Recursively invokes fn for every DxuiButton in the tree rooted at node.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogController::ForEachButton (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn)
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





