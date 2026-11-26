#pragma once

#include "Pch.h"


class DxuiDialogController;
class DxuiHwndSource;
class DxuiKeyRouter;
class IDxuiControl;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDialogRunnerHost
//
//  What DxuiDialogRunner calls in its window: showing and hiding it, and
//  the dialog hooks a subclass overrides.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDialogRunnerHost
{
public:
    virtual ~IDxuiDialogRunnerHost() = default;

    virtual DxuiHwndSource *    GetDialogSource     () const        = 0;
    virtual const IDxuiTheme *  GetDialogTheme      () const        = 0;
    virtual void                ShowDialogWindow    (bool activate) = 0;
    virtual void                HideDialogWindow    ()              = 0;
    virtual void                NotifyDialogShowing ()              = 0;
    virtual void                NotifyDialogTick    ()              = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogRunner
//
//  Shows a window as a dialog, as Win32's DialogBox and CreateDialog do. A
//  modal dialog disables its owner and runs a message loop until EndDialog;
//  a modeless one returns at once and its close callback gets the result.
//  While either is showing, a timer repaints it, so a focused caret blinks,
//  and runs its tick.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDialogRunner
{
public:
    static constexpr UINT_PTR  kDialogTimerId       = 1;
    static constexpr UINT      kDefaultDialogTickMs = 250;

    DxuiDialogRunner (IDxuiDialogRunnerHost & host, DxuiDialogController & dialog, DxuiKeyRouter & keys) : m_host (host), m_dialog (dialog), m_keys (keys) {}

    int   RunModal     (int defaultButtonId, HWND owner);
    void  ShowModeless (int defaultButtonId, bool activate);
    void  End          (int result);

    //  A timer tick for the dialog; false for another timer.
    bool  OnTimer      (UINT_PTR timerId);

    void  SetTickIntervalMs (UINT ms);
    void  SetInitialFocus   (IDxuiControl * control)          { m_initialFocus = control; }
    void  SetOnDialogEnd    (std::function<void (int)> fn)    { m_onDialogEnd = std::move (fn); }

private:
    IDxuiDialogRunnerHost      & m_host;
    DxuiDialogController       & m_dialog;
    DxuiKeyRouter              & m_keys;
    IDxuiControl               * m_initialFocus = nullptr;
    UINT                         m_tickMs       = kDefaultDialogTickMs;
    std::function<void (int)>    m_onDialogEnd;
};
