#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"
#include "Window/IDxuiHostClient.h"


class DxuiDialogController;
class DxuiFocusManager;
class DxuiPopupMenu;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiKeyRouterHost
//
//  What DxuiKeyRouter calls in its window: the modal overlay, the open
//  context menu, the control tree, and the paint batch around each key.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiKeyRouterHost
{
public:
    virtual ~IDxuiKeyRouterHost() = default;

    virtual HWND             GetKeyHwnd          () const                     = 0;
    virtual bool             RouteOverlayKey     (WPARAM vk)                  = 0;   // false when no overlay is up
    virtual bool             RouteOverlayChar    (wchar_t ch)                 = 0;   // false when no overlay is up
    virtual DxuiPopupMenu *  GetOpenContextMenu  ()                           = 0;
    virtual bool             RouteKeyToControls  (const DxuiKeyEvent & ev)    = 0;
    virtual void             BeginKeyBatch       ()                           = 0;
    virtual void             EndKeyBatch         ()                           = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyRouter
//
//  Routes the window's keys and characters: a modal overlay first, then an
//  open context menu, a dialog's navigation, and the controls.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiKeyRouter
{
public:
    DxuiKeyRouter (IDxuiKeyRouterHost & host, DxuiDialogController & dialog, DxuiFocusManager & focus) : m_host (host), m_dialog (dialog), m_focus (focus) {}

    DxuiMessageResult  OnKeyDown (WPARAM vk);
    DxuiMessageResult  OnChar    (wchar_t ch);

    //  Dxui's IsDialogMessage for a modeless dialog: the host's message loop
    //  calls it before TranslateMessage. Only Tab, Enter and Escape for this
    //  window are taken.
    bool  ProcessDialogMessage (const MSG & msg);

private:
    DxuiMessageResult  DispatchKey       (DxuiKeyEventKind kind, WPARAM code);
    DxuiMessageResult  DispatchDialogKey (WPARAM vk);

    IDxuiKeyRouterHost    & m_host;
    DxuiDialogController  & m_dialog;
    DxuiFocusManager      & m_focus;
};
