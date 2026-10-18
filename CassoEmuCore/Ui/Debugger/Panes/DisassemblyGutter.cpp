#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblyGutter.h"

#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::DisassemblyGutter
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyGutter::DisassemblyGutter (IDebuggerPaneHost & host, const ICodeViews & views, ApplyFn apply) :
    m_host  (host),
    m_views (views),
    m_apply (std::move (apply))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::Click
//
//  The breakpoints pane turns a breakpoint on or off with its checkbox
//  instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGutter::Click (const DxuiMouseEvent & ev)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    int                           row      = -1;
    RECT                          bounds;
    int                           lx       = 0;
    int                           ly       = 0;



    if (snapshot == nullptr)
    {
        return false;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        DxuiListView  * list = m_views.GetCodeList (view);
        int             line = -1;

        bounds = list->GetBounds();
        lx     = ev.positionDip.x - bounds.left;
        ly     = ev.positionDip.y - bounds.top;

        if (!m_host.IsRoutable (list) || !list->IsVisible() || lx < 0 || ly < 0 || ev.positionDip.x >= bounds.right || ev.positionDip.y >= bounds.bottom)
        {
            continue;
        }

        if (lx + list->GetLeftPx() >= list->GetColumnEffectiveWidthPx (DisassemblyCells::kGutterColumn))
        {
            return false;
        }

        row  = list->HitTestRow (lx, ly);
        line = m_views.GetCodeLineOfRow (view, row);

        //  A press on the PC's line may start a drag of its arrow, settled
        //  when the button comes up (see Drop).
        if (line >= 0 && snapshot->isPaused && m_views.GetCodeLines (view)[(size_t) line].isCurrent)
        {
            m_dragView    = view;
            m_dragRow     = row;
            m_dragOverRow = -1;
            return true;
        }

        if (line >= 0)
        {
            m_host.RunAction (DebuggerActions::GetToggleBreakpoint (*snapshot, m_views.GetCodeLines (view)[(size_t) line].address, m_host.GetMode()));
            return true;
        }

        return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::Drag
//
//  The arrow and the PC's row color follow the pointer to the instruction
//  under it. Rows are built again only when that row changes.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGutter::Drag (POINT atDip)
{
    DxuiListView  * list = nullptr;
    RECT            box  = {};
    int             row  = -1;



    if (m_dragView < 0 || m_host.GetLatestSnapshot() == nullptr)
    {
        return;
    }

    list = m_views.GetCodeList (m_dragView);
    box  = list->GetBounds();

    if (DxuiDockSite::Contains (box, atDip))
    {
        row = list->HitTestRow (atDip.x - box.left, atDip.y - box.top);
        row = (m_views.GetCodeLineOfRow (m_dragView, row) >= 0) ? row : m_dragOverRow;
    }

    if (row == m_dragOverRow)
    {
        return;
    }

    m_dragOverRow = row;
    m_apply (m_dragView);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::Drop
//
//  Released on another instruction of the same view, the PC's arrow was
//  dragged there, so that line becomes the next statement, as in Visual
//  Studio. Released on the line it was pressed on, it was a click, which
//  sets or clears the breakpoint there as on any other line.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGutter::Drop (const DxuiMouseEvent & ev)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    int                           view     = m_dragView;
    int                           from     = m_dragRow;
    DxuiListView                * list     = nullptr;
    RECT                          box      = {};
    int                           row      = -1;
    int                           line     = -1;



    if (view < 0 || snapshot == nullptr)
    {
        return false;
    }

    m_dragView    = -1;
    m_dragRow     = -1;
    m_dragOverRow = -1;
    list          = m_views.GetCodeList (view);
    box           = list->GetBounds();

    m_apply (view);

    if (DxuiDockSite::Contains (box, ev.positionDip))
    {
        row  = list->HitTestRow (ev.positionDip.x - box.left, ev.positionDip.y - box.top);
        line = m_views.GetCodeLineOfRow (view, row);
    }

    if (line < 0)
    {
        return true;
    }

    if (row == from)
    {
        m_host.RunAction (DebuggerActions::GetToggleBreakpoint (*snapshot, m_views.GetCodeLines (view)[(size_t) line].address, m_host.GetMode()));
    }
    else if (DisassemblyOptions::CanTakeBreakpoint (m_views.GetCodeLines (view)[(size_t) line]))
    {
        SetNextStatement (m_views.GetCodeLines (view)[(size_t) line].address);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::SetNextStatement
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGutter::SetNextStatement (Word address)
{
    m_host.RunAction (DebuggerActions::GetSetRegister ("PC", std::format ("{:04X}", address), m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter::Hover
//
//  Rows are built again only when the row under the pointer changes.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGutter::Hover (POINT atDip)
{
    int   view   = -1;
    int   row    = -1;
    int   was    = m_hoverView;
    RECT  bounds = {};
    int   lx     = 0;
    int   ly     = 0;



    for (int each = 0; each < DebuggerViewState::kMaxCodeViews && view < 0; each++)
    {
        DxuiListView  * list = m_views.GetCodeList (each);

        bounds = list->GetBounds();
        lx     = atDip.x - bounds.left;
        ly     = atDip.y - bounds.top;

        if (!m_host.IsRoutable (list) || !list->IsVisible() || lx < 0 || ly < 0 || atDip.x >= bounds.right || atDip.y >= bounds.bottom ||
            lx + list->GetLeftPx() >= list->GetColumnEffectiveWidthPx (0))
        {
            continue;
        }

        view = each;
        row  = (m_views.GetCodeLineOfRow (each, list->HitTestRow (lx, ly)) >= 0) ? list->HitTestRow (lx, ly) : -1;
    }

    if (view == m_hoverView && row == m_hoverRow)
    {
        return;
    }

    m_hoverView = view;
    m_hoverRow  = row;

    for (int each : { was, view })
    {
        if (each >= 0 && m_host.GetLatestSnapshot() != nullptr)
        {
            m_apply (each);
        }
    }

    m_host.Repaint();
}
