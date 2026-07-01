#pragma once

#include "Pch.h"
#include "Window/IDxuiHostClient.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiEvents.h"


class DxuiDialog;
class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiModalDialogClient
//
//  IDxuiHostClient for DxuiDialogManager::ShowModal's blocking modal
//  pump. Owns a DxuiFocusManager over the hosted DxuiDialog so Tab /
//  typing / Enter reach the content controls, routes mouse (incl. wheel)
//  into the dialog's control tree, answers WM_GETMINMAX for resizable
//  dialogs, and maps Escape / a window-close gesture to the cancel
//  result -- recording the chosen return code so the pump can exit.
//
//  All entry points run on the UI thread (the modal pump's thread).
//
////////////////////////////////////////////////////////////////////////////////

class DxuiModalDialogClient : public IDxuiHostClient
{
public:
    void  Bind               (DxuiDialog * dialog, int cancelResult);
    void  SetHwnd            (HWND hwnd)     { m_hwnd = hwnd; }
    void  SetMinClientSizeDip (SIZE sizeDip) { m_minClientSizeDip = sizeDip; }
    void  SetupFocus         (DxuiDialog * dialog, const IDxuiTheme * theme);

    void  Resolve            (int returnCode);
    bool  Done               () const { return m_done;   }
    int   Result             () const { return m_result; }

    DxuiMessageResult  OnKeyDown     (WPARAM vk, LPARAM lParam)              override;
    DxuiMessageResult  OnChar        (WPARAM ch, LPARAM lParam)             override;
    DxuiMessageResult  OnClose       ()                                     override;
    DxuiMessageResult  OnGetMinMax   (MINMAXINFO * info)                    override;
    DxuiMessageResult  OnMouseMove   (WPARAM wParam, LPARAM lParam)         override;
    DxuiMessageResult  OnLButtonDown (WPARAM wParam, LPARAM lParam)         override;
    DxuiMessageResult  OnLButtonUp   (WPARAM wParam, LPARAM lParam)         override;
    DxuiMessageResult  OnMouseWheel  (WPARAM wParam, LPARAM lParam, bool horizontal) override;
    DxuiMessageResult  OnTimer       (UINT_PTR timerId)                     override;


private:
    bool               RouteKeyToFocused (WPARAM vk, bool shift);
    DxuiMessageResult  RouteMouse        (DxuiMouseEventKind kind, DxuiMouseButton button, LPARAM lParam);


    DxuiFocusManager  m_focus;
    DxuiDialog *      m_dialog           = nullptr;
    HWND              m_hwnd             = nullptr;
    SIZE              m_minClientSizeDip = { 0, 0 };
    int               m_result           = -1;
    int               m_cancelResult     = -1;
    bool              m_done             = false;
};
