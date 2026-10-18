#pragma once

#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/WatchHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane
//
//  What the instruction at the PC and the one just executed touch, then the
//  watches the user added, then the zero-page pointers, each section under a
//  heading of its own, and while the machine is stopped in a running
//  Applesoft program, its variables. The last row adds a watch. A watch is
//  edited in place, as in Visual Studio's watch window, and the pane's undo
//  is its own, apart from every memory window's.
//
////////////////////////////////////////////////////////////////////////////////

class WatchesPane : public UndoListPane
{
public:
    explicit WatchesPane (IDebuggerPaneHost & host);
    ~WatchesPane() override;

    WatchHistory                 & GetHistory     () { return m_history; }
    DxuiTextInput                * GetEditor      () const { return m_editor; }

    //  A box over a cell, holding what the cell shows, all of it selected so
    //  typing replaces it; Enter commits what was typed, Escape cancels the
    //  edit, and a click anywhere else commits it.
    void                           BeginEdit      (int row, int column);
    void                           EndEdit        (bool commit);

    void                           CreateOverlays () override;
    std::vector<IDxuiControl *>    GetOverlays    () const override { return { m_editor }; }
    void                           Configure      () override;
    std::vector<IDxuiControl *>    GetControls    () const override;
    void                           Apply          (const DebuggerViewSnapshot & snapshot) override;
    bool                           RouteEditMouse (const DxuiMouseEvent & ev) override;
    bool                           RouteEditKey   (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                           AddMenuItems   (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                           CanUndo        (bool redo) const override;
    std::wstring                   GetUndoText    (bool redo) const override;
    void                           Undo           (bool redo) override;

private:
    //  What each row is, since the list mixes headings, automatic watches and
    //  the user's own: an automatic row has its index into the snapshot and a
    //  manual one its watch id. A BASIC variable listed under its heading or
    //  as an automatic watch is a BasicItem.
    enum class RowKind { Heading, Automatic, Manual, ZeroPage, Add, BasicHeading, BasicItem };

    struct Row
    {
        RowKind  kind  = RowKind::Heading;
        int      index = 0;
    };

    //  The watch being edited: column 0 is the expression, 1 the value. An
    //  automatic watch is found again by its key, since a snapshot taken
    //  during the edit can list the automatic watches in another order.
    struct Edit
    {
        int          row    = -1;
        int          column = 0;
        Row          what;
        std::string  autoKey;
    };

    static constexpr int  kEditMaxChars = 64;

    std::vector<DxuiListView::Cell>  MakeHeading    (const std::wstring & title) const;
    void                             ApplyRows      (const DebuggerViewSnapshot & snapshot);
    void                             RemoveSelected ();

    WatchHistory        m_history;
    DxuiTextInput     * m_editor                   = nullptr;
    Edit                m_edit;
    std::vector<Row>    m_rows;
    bool                m_isBasicVariablesExpanded = false;
};
