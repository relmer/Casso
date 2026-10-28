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
    explicit ExplorerInput (ExplorerContext & context);

    bool    HandleMouse           (const DxuiMouseEvent & ev);
    bool    HandleKey             (const DxuiKeyEvent & ev);
    LPCWSTR GetCursorForPoint (POINT clientPx) const;

    //  Keyboard focus. The toolbar is one pane, with its focused button in
    //  m_toolbarFocus.
    void          SetFocusPane (ExplorerPane pane);
    ExplorerPane  GetFocusPane () const { return m_focus; }
    bool          HasCaretFocus () const { return m_focus == ExplorerPane::Search || m_focus == ExplorerPane::GoTo || m_focus == ExplorerPane::LocationSearch; }

    FocusStop               GetFocusStop     () const;
    void                    SetFocusStop     (const FocusStop & stop);
    std::vector<FocusStop>  BuildFocusStops  () const;
    bool                    RouteToolbarKey  (bool preview, const DxuiKeyEvent & ev);
    void                    StepToolbarFocus (bool preview, bool forward);

    //  The focused pane's control, where routing of a standard command such as
    //  Copy or Select all starts.
    IDxuiControl *  GetFocusedControl() const;
    bool  RouteCommandBarKey      (const DxuiKeyEvent & ev);

    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

private:

    ExplorerContext  & m_context;

    ExplorerPane                              m_focus               = ExplorerPane::Tree;
    POINT                                     m_dragStart           = {};

    int       m_toolbarFocus    = 0;
    int       m_commandBarFocus = 0;

    RightPress            m_rightPress;
};
