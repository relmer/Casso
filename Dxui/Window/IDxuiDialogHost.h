#pragma once

#include "Pch.h"


class DxuiFocusManager;
class DxuiPanel;
class IDxuiControl;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDialogHost
//
//  The window a DxuiDialogMode runs a dialog in: its window and owner, the
//  controls and focus the dialog walks, the timer behind the dialog's tick,
//  and the window's dialog hooks.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDialogHost
{
public:
    virtual                      ~IDxuiDialogHost() = default;

    virtual bool                 IsDialogHostCreated    () const                 = 0;
    virtual HWND                 GetDialogHwnd          () const                 = 0;
    virtual HWND                 GetDialogOwnerHwnd     () const                 = 0;
    virtual DxuiPanel          & GetDialogRoot          ()                       = 0;
    virtual DxuiFocusManager   & GetDialogFocus         ()                       = 0;
    virtual const IDxuiTheme   * GetDialogTheme         () const                 = 0;
    virtual void                 SetDialogTimer         (UINT ms)                = 0;
    virtual void                 KillDialogTimer        ()                       = 0;
    virtual void                 ShowDialogWindow       (bool activate)          = 0;
    virtual void                 HideDialogWindow       ()                       = 0;
    virtual void                 InvalidateDialog       ()                       = 0;
    virtual bool                 HasDialogOverlay       () const                 = 0;
    virtual void                 SendDialogOverlayKey   (WPARAM vk)              = 0;
    virtual void                 NotifyDialogShowing    ()                       = 0;
    virtual bool                 TrySwitchDialogTab     (bool backward)          = 0;
    virtual void                 NotifyDialogKeyHandled (IDxuiControl * focused) = 0;
};
