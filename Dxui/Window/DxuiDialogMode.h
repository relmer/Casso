#pragma once

#include "Pch.h"
#include "Window/IDxuiHostClient.h"


class DxuiButton;
class IDxuiControl;
class IDxuiDialogHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogMode
//
//  Dialog behavior for a window, modal or modeless: command buttons wired
//  to end the dialog, the default button emphasized, Tab, Enter and Escape
//  routed through the focus, a periodic tick, and for a modal dialog the
//  owner disabled and a private message loop until the dialog ends. Mirrors
//  Win32 DialogBox, CreateDialog, IsDialogMessage and EndDialog.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDialogMode
{
public:
    static constexpr UINT  kDefaultTickMs = 250;

    explicit DxuiDialogMode (IDxuiDialogHost & host) : m_host (host) {}

    int                ShowModal         (int defaultButtonId);
    void               ShowModeless      (int defaultButtonId, bool activate);
    bool               ProcessMessage    (const MSG & msg);
    void               End               (int result);
    DxuiMessageResult  DispatchKey       (WPARAM vk);
    bool               TryTriggerButton  (int commandId);
    void               SetTickIntervalMs (UINT ms);

    void               SetOnEnd          (std::function<void (int)> fn) { m_onEnd = std::move (fn); }
    void               SetInitialFocus   (IDxuiControl * ctl)           { m_initialFocus = ctl; }
    bool               IsActive          () const                       { return m_isActive; }

private:
    void                 Begin             (int defaultButtonId, bool modal);
    void                 WireButtons       ();
    bool                 RouteKeyToFocused (WPARAM vk, bool shift);
    static DxuiButton  * FindButtonById    (IDxuiControl * node, int commandId);
    static void          ForEachButton     (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn);

    IDxuiDialogHost            & m_host;
    IDxuiControl               * m_initialFocus    = nullptr;
    bool                         m_isActive        = false;   // dialog behaviors on (modal or modeless)
    bool                         m_isModal         = false;   // blocking-modal (owner disabled + private loop)
    bool                         m_isDone          = false;
    int                          m_result          = 0;
    int                          m_defaultButtonId = 0;
    UINT                         m_tickMs          = kDefaultTickMs;   // repaint / tick cadence (caret-blink default)
    std::function<void (int)>    m_onEnd;                              // modeless close callback
};
