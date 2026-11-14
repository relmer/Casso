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

    //  The event offered to the window's controls that no pane routes,
    //  front to back, through a pane's mouse handler where it set one.
    bool     RouteToControls   (const DxuiMouseEvent & ev);

    //  After a control takes a press, each move and the release that follow
    //  go to it, wherever the pointer is.
    bool     RouteToPressed    (const DxuiMouseEvent & ev);

    //  Ends the press, for a pane whose press opened a menu or began a drag,
    //  whose loop takes the release.
    void     ReleasePressed    () { m_pressed = nullptr; }
    bool     HandleKey         (const DxuiKeyEvent & ev);

private:
    ExplorerContext  & m_context;
    ExplorerPane       m_focus            = ExplorerPane::Tree;
    IDxuiControl     * m_pressed          = nullptr;   // the control with the press from RouteToControls, until the release
    bool               m_overControl      = false;     // the pointer was over one of those controls at the last move
    int                m_toolbarFocus     = 0;
    int                m_commandBarFocus  = 0;
    float              m_iconWheelNotches = 0.0f;   // a touchpad's part notches of Ctrl with the wheel, until they make one
};
