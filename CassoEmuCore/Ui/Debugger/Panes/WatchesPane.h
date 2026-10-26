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
//  edited in place, as in Visual Studio's watch window, on the same editable
//  grid as the heat map's ranges, and the pane's undo is its own, apart from
//  every memory window's.
//
////////////////////////////////////////////////////////////////////////////////

class WatchesPane : public UndoListPane
{
public:
    explicit WatchesPane (IDebuggerPaneHost & host);
    ~WatchesPane() override;

    //  The clock the wait after a Tab's add is timed by, the system's
    //  unless a test sets one.
    using Clock = std::function<ULONGLONG()>;

    void                           SetClock       (Clock clock) { m_clock = std::move (clock); }

    WatchHistory                 & GetHistory     () { return m_history; }
    DxuiTextInput                * GetEditor      () const { return &m_grid->GetEditor(); }

    //  The grid's box over a cell, with the cell's text in it, all of it
    //  selected so typing replaces it; Enter commits what was typed, Escape
    //  cancels the edit, and a click anywhere else commits it.
    void                           BeginEdit      (int row, int column);
    void                           EndEdit        (bool commit);

    void                           Configure      () override;
    std::vector<IDxuiControl *>    GetControls    () const override;
    void                           Apply          (const DebuggerViewSnapshot & snapshot) override;
    bool                           RouteEditKey   (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                           AddMenuItems   (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                           CanUndo        (bool redo) const override;
    std::wstring                   GetUndoText    (bool redo) const override;
    void                           Undo           (bool redo) override;

protected:
    DxuiListView *                 CreateList     () override;

private:
    //  What each row is, since the list mixes headings, automatic watches and
    //  the user's own: an automatic row has its index into the snapshot and a
    //  manual one its watch id. A BASIC variable listed under its heading or
    //  as an automatic watch is a BasicItem. The grid's own last row adds a
    //  watch, and has no entry.
    enum class RowKind { Heading, Automatic, Manual, ZeroPage, BasicHeading, BasicItem };

    struct Row
    {
        RowKind  kind  = RowKind::Heading;
        int      index = 0;
    };

    //  The watch an edit began on. An automatic watch is found again by its
    //  key, since a snapshot taken during the edit can list the automatic
    //  watches in another order.
    struct Edit
    {
        Row          what;
        std::string  autoKey;
        bool         isOpen = false;
    };

    static constexpr int  kEditMaxChars     = 64;
    static constexpr int  kFirstFieldColumn = 1;      // the ID column comes before the watch's cells
    static constexpr int  kAddedWaitMs      = 2000;   // the milliseconds after a Tab's add before its value edit is dropped

    std::vector<DxuiListView::Cell>  MakeHeading    (const std::wstring & title) const;
    DxuiListView::Cell               MakeIdCell     (const std::wstring & text) const;
    void                             ApplyRows      (const DebuggerViewSnapshot & snapshot);
    void                             RemoveSelected ();

    //  The grid's commit of a watch's cell, column 0 the expression and 1
    //  the value, and of the add row.
    bool                             CommitCell     (int row, int column, const std::wstring & typed, std::wstring & outError);
    bool                             AddWatch       (const std::wstring & typed, std::wstring & outError);
    void                             SetError       (const std::wstring & text);
    void                             NoteEditBegun  (int row);
    void                             EditAddedValue (const DebuggerViewSnapshot & snapshot);

    WatchHistory         m_history;
    DxuiEditableGrid   * m_grid                     = nullptr;
    std::vector<int>     m_idsBeforeAdd;
    bool                 m_isValueEditAfterAdd      = false;
    ULONGLONG            m_addedBy                  = 0;        // when the value edit after an add is dropped
    Clock                m_clock                    = [] { return GetTickCount64(); };
    std::vector<Row>     m_rows;
    Edit                 m_edit;
    bool                 m_isBasicVariablesExpanded = false;
};
