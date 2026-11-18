#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteWindowMouse
//
//  Each stage takes the event or passes it to the next, in the order the
//  window's parts lie over one another.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteWindowMouse (const DxuiMouseEvent & ev)
{
    NotePointer (ev);

    if (RoutePaneDragMouse (ev))
    {
        return true;
    }

    //  A code row's text that its view cuts off is drawn whole over the row
    //  while the pointer is on the row. This runs ahead of everything that
    //  lies over the panes, so IsOperandTipCovered tests for each of them. A
    //  move event's button indicates whether the left button was down when
    //  the pointer moved; that, not the button's state at this moment, marks
    //  a drag over the row.
    m_operandTips->Update (ev, ev.button == DxuiMouseButton::Left);

    return RoutePaneEditMouse (ev) || RouteCtrlMouse (ev) || RouteChromeMouse (ev) || RouteOverPanesMouse (ev) ||
           RouteRightPressMouse (ev) || RouteContentMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NotePointer
//
//  The pointer leaving the window closes its tip. A press hides any tip at
//  once: a button it lands on may open a dialog or a menu, and the tip would
//  stay up over it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NotePointer (const DxuiMouseEvent & ev)
{
    const std::wstring  & routing = m_floating->GetRoutingPane();



    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        GetRoutedTips().OnLeave ((int64_t) GetTickCount64());
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        GetRoutedTips().OnPress();
        m_openingFocus.OnUserInput();
        m_stopFocus.OnInput (IsCommandPane (routing.empty() ? m_dockSite->GetPaneAt (ev.positionDip) : routing));
        m_lastPressPx = POINT { ev.positionDip.x, ev.positionDip.y };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RoutePaneDragMouse
//
//  A pane's drag takes every event until the button comes up, wherever the
//  pointer is, so the menu bar, toolbars and status bar it crosses don't
//  get its moves, and their tips don't open. Off the panes, its cross and
//  its preview go at once.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RoutePaneDragMouse (const DxuiMouseEvent & ev)
{
    if (!m_floating->GetRoutingPane().empty() || !m_dockSite->HasPointerDrag())
    {
        return false;
    }

    GetRoutedTooltip().HideImmediate();
    (void) m_dockSite->OnMouse (ev);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RoutePaneEditMouse
//
//  A pane's box being edited, such as a breakpoint's name, a watch, a
//  register, a heat map range or its set's name. Then a pane's use of the
//  mouse ahead of every other control, such as the wheel over the profiler's
//  timeline, which zooms the timeline rather than scrolling or sizing the
//  text.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RoutePaneEditMouse (const DxuiMouseEvent & ev)
{
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEditMouse (ev))
        {
            return true;
        }
    }

    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEarlyMouse (ev))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteCtrlMouse
//
//  Ctrl+click on a symbol in a code view or a source document goes to its
//  definition, as the menu's Go to definition does.
//
//  Ctrl+wheel anywhere over a pane whose content zooms, such as the heat
//  map, zooms that content, over its bar and its empty space as much as over
//  the content itself. Over any other pane it sizes that pane's text alone,
//  as Ctrl+Plus and Ctrl+Minus do, before a view that would take the wheel
//  for itself. Off the panes it does nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteCtrlMouse (const DxuiMouseEvent & ev)
{
    bool  isCtrlClick = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && ev.ctrl && !ev.shift && !ev.alt;
    bool  isCtrlWheel = ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && ev.wheelDelta != 0.0f;
    Word  address     = 0;
    int   fileId      = -1;
    int   line        = 0;



    if (isCtrlClick && TryGetDefinitionAt (ev.positionDip, address))
    {
        GoToDefinition (address);
        return true;
    }

    if (isCtrlClick && TryGetMacroDefinitionAt (ev.positionDip, fileId, line))
    {
        m_sources->Open (fileId, line, true);
        return true;
    }

    if (isCtrlWheel)
    {
        std::wstring  pane = GetRoutedPaneAt (ev.positionDip);



        if (!ZoomPaneContent (pane, ev.wheelDelta, ev.positionDip))
        {
            StepPaneTextZoom (pane, ev.wheelDelta > 0.0f ? 1 : -1);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteChromeMouse
//
//  The bars around the panes, while no floating pane has the pointer. The
//  status bar and the menu bar take the events over them ahead of the
//  timeline, so the pointer leaving the timeline for either is handled here,
//  which closes the timeline's preview. The status bar's zoom popup lies over
//  everything while it is open. The menu bar comes before the command bar:
//  each has its strip and whatever menu it has open. Then the panes' info
//  buttons at the ends of the toolbars, and each pane's bar.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteChromeMouse (const DxuiMouseEvent & ev)
{
    bool  isDocked = m_floating->GetRoutingPane().empty();



    if (isDocked && !m_timelineHost.IsFloating() && m_timelineBar != nullptr &&
        !DxuiDockSite::Contains (m_timelineBar->GetBounds(), ev.positionDip))
    {
        m_timelineBar->OnToolbarMouseLeave();
    }

    if (isDocked && (RouteStatusBarMouse (ev) || RouteMenuBarMouse (ev)))
    {
        return true;
    }

    if (isDocked && !m_timelineHost.IsFloating() && RouteTimelineMouse (ev))
    {
        return true;
    }

    if (isDocked && !m_barHost.IsFloating() && RouteCommandBarMouse (ev))
    {
        return true;
    }

    if (m_colorKeys->RouteMouse (ev))
    {
        return true;
    }

    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteBarMouse (ev))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteOverPanesMouse
//
//  What lies over the panes' content. A find widget lies over its pane, so
//  a press, a release or the wheel on its plate is the widget's whether or
//  not one of its controls takes it; its moves go on, for the tips and the
//  hover. A press on a disassembly or source tab sets the step mode as a
//  press inside that pane does. A click on a branch arrow goes to where it
//  points, and the next back. Then the dock site: its strips, its sashes and
//  a drag in progress. A tip that is up as a drag starts is hidden, since
//  tips are off during a drag. A pane slid out from an edge lies over
//  others, so its area goes to that pane.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteOverPanesMouse (const DxuiMouseEvent & ev)
{
    bool  isDocked    = m_floating->GetRoutingPane().empty();
    bool  isLeftPress = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left;



    if (ev.kind != DxuiMouseEventKind::Move && ev.kind != DxuiMouseEventKind::Leave && m_finds->IsOverPlate (ev.positionDip))
    {
        RouteFindPlateMouse (ev);
        return true;
    }

    if (isDocked && isLeftPress)
    {
        NoteTabFocus (ev.positionDip);
    }

    if (isDocked && isLeftPress && m_code->GetArrows().ClickBranchArrow (ev.positionDip))
    {
        return true;
    }

    if (isDocked && m_dockSite->OnMouse (ev))
    {
        if (m_dockSite->IsDragging())
        {
            GetRoutedTooltip().HideImmediate();
        }

        return true;
    }

    if (isDocked && !m_dockSite->GetSlidPane().empty() && ev.kind != DxuiMouseEventKind::Move &&
        DxuiDockSite::Contains (m_dockSite->GetSlidRect(), ev.positionDip))
    {
        return m_floating->RouteMouse (m_dockSite->GetSlidPane(), ev);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteRightPressMouse
//
//  A right press over a text box opens the box's edit menu rather than its
//  pane's. Over a pane's content it opens the content's menu, and over a
//  part of it with none, no menu; over its tab or title, the menu of what
//  can be done with the pane.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteRightPressMouse (const DxuiMouseEvent & ev)
{
    const std::wstring  & routing = m_floating->GetRoutingPane();
    std::wstring          pane;



    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Right)
    {
        return false;
    }

    if (DxuiTextInput * box = GetTextBoxAt (ev.positionDip); box != nullptr)
    {
        ShowTextBoxMenu (*box, ev.positionDip);
        return true;
    }

    pane = routing.empty() ? m_dockSite->GetPaneAt (ev.positionDip) : routing;

    if (!pane.empty() && !ShowContentMenu (pane, ev.positionDip) && !IsOverPaneContent (pane, ev.positionDip))
    {
        ShowDockToMenu (pane, ev.positionDip);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteContentMouse
//
//  The panes' content. The find widget floats over its pane's text, so a
//  press on it goes to the widget, not the text. Then what a pane draws, such
//  as the heat map and the flame graph, and last the lists and text views.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteContentMouse (const DxuiMouseEvent & ev)
{
    if (!m_finds->IsOverPlate (ev.positionDip) &&
        (m_sources->RouteMouse (ev) || m_consolePane->GetOutput().RouteMouse (ev, m_consolePane->GetCommandLine().GetBox())))
    {
        return true;
    }

    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteContentMouse (ev))
        {
            return true;
        }
    }

    return m_contentMouse.Route (ev);
}
