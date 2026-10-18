#pragma once

#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGutter
//
//  The disassembly views' first column, as in an editor: a press sets or
//  clears the breakpoint on the row, the row under the pointer has a gray
//  breakpoint where a click would set one, and the PC's arrow is dragged to
//  another instruction to make it the next statement, as in Visual Studio.
//  The rest of each view is text (FR-076).
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyGutter
{
public:
    using ApplyFn = std::function<void (int view)>;

    //  `apply` builds a view's rows again, with the hover and the drag the
    //  gutter has now.
    DisassemblyGutter (IDebuggerPaneHost & host, const ICodeViews & views, ApplyFn apply);

    DisassemblyGutter (const DisassemblyGutter &)             = delete;
    DisassemblyGutter & operator= (const DisassemblyGutter &) = delete;

    //  The row with the gray breakpoint in a view, and the row the PC's
    //  arrow is dragged over in a view; -1 for neither.
    int                  GetHoverRow    (int view) const { return (view == m_hoverView) ? m_hoverRow : -1; }
    int                  GetDragRow     (int view) const { return (view == m_dragView) ? m_dragOverRow : -1; }
    bool                 IsDragging     () const { return m_dragView >= 0; }

    //  A press on a view's first column, true when the gutter took it; the
    //  pointer moving while the PC's arrow is dragged, and the button coming
    //  up after a press on the PC's line, true when the gutter took it; and
    //  the pointer moving over the views.
    bool                 Click          (const DxuiMouseEvent & ev);
    void                 Drag           (POINT atDip);
    bool                 Drop           (const DxuiMouseEvent & ev);
    void                 Hover          (POINT atDip);

    //  The PC moved to a line without running anything, as Visual Studio's
    //  Set Next Statement does.
    void                 SetNextStatement (Word address);

private:
    IDebuggerPaneHost    & m_host;
    const ICodeViews     & m_views;
    ApplyFn                m_apply;
    int                    m_hoverView   = -1;
    int                    m_hoverRow    = -1;
    int                    m_dragView    = -1;
    int                    m_dragRow     = -1;
    int                    m_dragOverRow = -1;
};
