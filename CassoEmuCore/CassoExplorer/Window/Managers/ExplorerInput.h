#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"

struct ExplorerContext;
struct FocusStop;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput
//
//  Pointer and keyboard input: each press, move and key routed to the
//  pane, toolbar or widget under it, and keyboard focus across the panes
//  and the toolbars' buttons.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerInput
{
public:
    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

    explicit ExplorerInput (ExplorerContext & context);

    LPCWSTR                   GetCursorForPoint  (POINT clientPx) const;
    void                      SetFocusPane       (ExplorerPane pane);
    ExplorerPane              GetFocusPane       () const { return m_focus; }
    bool                      HasCaretFocus      () const { return m_focus == ExplorerPane::Search || m_focus == ExplorerPane::GoTo || m_focus == ExplorerPane::LocationSearch; }
    FocusStop                 GetFocusStop       () const;
    void                      SetFocusStop       (const FocusStop & stop);
    std::vector<FocusStop>    BuildFocusStops    () const;
    bool                      RouteToolbarKey    (bool preview, const DxuiKeyEvent & ev);
    void                      StepToolbarFocus   (bool preview, bool forward);
    IDxuiControl            * GetFocusedControl  () const;
    bool                      RouteCommandBarKey (const DxuiKeyEvent & ev);

    bool     HandleMouse       (const DxuiMouseEvent & ev);
    bool     HandleKey         (const DxuiKeyEvent & ev);

private:
    ExplorerContext  & m_context;
    ExplorerPane       m_focus            = ExplorerPane::Tree;
    POINT              m_dragStart        = {};
    int                m_toolbarFocus     = 0;
    int                m_commandBarFocus  = 0;
    float              m_iconWheelNotches = 0.0f;   // a touchpad's part notches of Ctrl with the wheel, until they make one
    RightPress         m_rightPress;
};
