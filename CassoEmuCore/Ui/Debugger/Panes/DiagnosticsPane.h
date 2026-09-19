#pragma once

#include "Debugger/DiagnosticsSnapshot.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane
//
//  One device's panel: its groups and rows in a list, with the bit decode of a
//  row beside its value, and above them the graphic the device's payload asks
//  for, if any.
//
//  NOTHING HERE KNOWS A DEVICE. The rows are drawn as published and the
//  graphic is chosen by the payload's type, so a device on a future machine
//  gets a panel by publishing rows. The panel is read-only: it shows state and
//  changes none.
//
//  The window owns the list and the three graphics as child controls; the pane
//  holds pointers to them and a frame that stacks them into one dockable pane.
//
////////////////////////////////////////////////////////////////////////////////

class DiagnosticsPane
{
public:
    DiagnosticsPane (std::string id, std::wstring title, DxuiListView * list, MemoryMapBar * map, DiskHeadView * head, MeterBar * meters);

    DiagnosticsPane (const DiagnosticsPane &)             = delete;
    DiagnosticsPane & operator= (const DiagnosticsPane &) = delete;

    const std::string            &  GetId       () const { return m_id; }
    const std::wstring           &  GetTitle    () const { return m_title; }
    DxuiListView                  * GetList     () const { return m_list; }
    DebuggerPaneFrame             * GetFrame    () const { return m_frame.get(); }

    //  Every control of the pane, graphics first, in the order they stack.
    std::vector<IDxuiControl *>     GetControls () const;

    //  The list's columns; the window makes it dense as it does every pane.
    void  Configure ();

    //  Shows a snapshot, and returns whether the graphic changed kind, which
    //  the frame has to lay out again to show.
    bool  Apply     (const DiagnosticsSnapshot & snapshot);

    //  The list rows for a snapshot: a row for each group's title, then its
    //  rows as label, value and bits, a clear bit's name dimmed.
    static std::vector<std::vector<DxuiListView::Cell>>  MakeRows (const DiagnosticsSnapshot & snapshot);

    //  The bit decode as one cell: the names in order, each clear one dimmed.
    static DxuiListView::Cell  MakeBitsCell (const std::vector<DiagnosticsBit> & bits);

private:
    enum class Visual
    {
        None,
        MemoryMap,
        DiskHead,
        Meters,
    };

    std::string                         m_id;
    std::wstring                        m_title;
    DxuiListView                      * m_list   = nullptr;
    MemoryMapBar                      * m_map    = nullptr;
    DiskHeadView                      * m_head   = nullptr;
    MeterBar                          * m_meters = nullptr;
    std::unique_ptr<DebuggerPaneFrame>  m_frame;
    Visual                              m_visual = Visual::None;
};
