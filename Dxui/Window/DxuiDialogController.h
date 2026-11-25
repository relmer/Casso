#pragma once

#include "Pch.h"

#include "Core/DxuiFocusManager.h"

class DxuiButton;
class DxuiPanel;
class IDxuiControl;
class IDxuiDialogHost;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogEndAction
//
//  What the window does after DxuiDialogController::End.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiDialogEndAction
{
    None,
    ExitModalLoop,
    CloseModeless,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogController
//
//  Dialog mode for a window: button wiring and default-button emphasis, Tab,
//  Enter and Escape routing, and the modal or modeless end state. It has no
//  window.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDialogController
{
public:
    DxuiDialogController (IDxuiDialogHost & host, DxuiFocusManager & focus, DxuiPanel & root);

    void                 Begin          (const IDxuiTheme * theme, int defaultButtonId, bool modal, IDxuiControl * initialFocus);
    DxuiDialogEndAction  End            (int result);
    void                 Deactivate     ();
    bool                 HandleKey      (WPARAM vk, bool shift, bool ctrl);
    bool                 TryClickButton (int commandId);

    bool  IsActive       () const { return m_isActive; }
    bool  IsModalDone    () const { return m_isModalDone; }
    int   GetModalResult () const { return m_modalResult; }

private:
    bool  RouteKeyToFocused (WPARAM vk, bool shift);
    void  WireButtons       ();

    static DxuiButton  * FindButtonById (IDxuiControl * node, int commandId);
    static void          ForEachButton  (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn);

    IDxuiDialogHost   & m_host;
    DxuiFocusManager  & m_focus;
    DxuiPanel         & m_root;
    bool                m_isActive        = false;   // dialog behaviors on (modal or modeless)
    bool                m_isModal         = false;   // blocking-modal (owner disabled + private loop)
    bool                m_isModalDone     = false;
    int                 m_modalResult     = 0;
    int                 m_defaultButtonId = 0;
};