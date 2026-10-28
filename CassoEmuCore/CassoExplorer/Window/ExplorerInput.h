#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Window/ExplorerPane.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;
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
    explicit ExplorerInput (CassoExplorerWindow & window);

    bool    OnMouse           (const DxuiMouseEvent & ev);
    bool    OnKey             (const DxuiKeyEvent & ev);
    LPCWSTR GetCursorForPoint (POINT clientPx) const;

    static DxuiToolbar &  GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar);

    //  Keyboard focus. The toolbar is one pane, with its focused button in
    //  m_toolbarFocus.
    void  SetFocusPane (ExplorerPane pane);

    FocusStop               GetFocusStop     () const;
    void                    SetFocusStop     (const FocusStop & stop);
    std::vector<FocusStop>  BuildFocusStops  () const;
    bool                    RouteToolbarKey  (bool preview, const DxuiKeyEvent & ev);
    void                    StepToolbarFocus (bool preview, bool forward);

    //  The focused pane's control, where routing of a standard command such as
    //  Copy or Select all starts.
    IDxuiControl *  GetFocusedControl() const;
    bool  RouteToolbarMouse   (DxuiToolbar & toolbar, const DxuiMouseEvent & ev);
    bool  RouteCommandBarKey      (const DxuiKeyEvent & ev);

    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    POINT                                     m_dragStart           = {};

    int       m_toolbarFocus    = 0;
    int       m_commandBarFocus = 0;

    RightPress            m_rightPress;
    bool                  m_tipMuted    = false;
    RECT                  m_tipMuteRect = {};
};
