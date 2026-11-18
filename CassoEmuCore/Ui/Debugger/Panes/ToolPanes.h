#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/ListValueFilters.h"

class DebuggerPaneFrame;





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane
//
//  A tool pane that is one list in a frame: the Symbols, Bookmarks and data
//  ranges panes, and the Profiler, which adds its bar, its timeline and its
//  flame graph.
//
////////////////////////////////////////////////////////////////////////////////

class ToolListPane : public IDebuggerPane
{
public:
    ToolListPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title);
    ~ToolListPane() override;

    DxuiListView *               GetList        () const { return m_list; }

    const std::wstring         & GetId          () const override { return m_id; }
    std::wstring                 GetTitle       () const override { return m_title; }
    void                         CreateControls () override;
    void                         AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl               * GetContent     () const override;
    std::vector<IDxuiControl *>  GetControls    () const override { return { m_list }; }
    std::vector<DxuiListView *>  GetLists       () const override { return { m_list }; }
    IDxuiFindTarget            * GetFindView    () const override { return m_list; }
    DebuggerPaneFrame          * GetFindFrame   () const override { return m_frame.get(); }

protected:
    //  Copy, for the rows selected in the list.
    void                         AddCopyItem    (PaneMenuItems & items);

    //  The path the user picks to save to; empty when the user backs out or
    //  the window has no file pickers.
    std::wstring                 PickSaveFile   (const wchar_t * filterName, const wchar_t * filter, const wchar_t * extension, const std::wstring & fileName);

    //  One muted cell across the row.
    static std::vector<DxuiListView::Cell>  MakeNote (const std::wstring & text);

    //  Scrolls to a row added since the last call.
    void                         ShowNewListRow ();

    //  The rows a menu's Delete acts on: every selected row when the one
    //  clicked is among them, otherwise the one clicked. With no row given,
    //  the selection.
    std::vector<int>             GetActedRows   (int row) const;

    std::wstring                         m_id;
    std::wstring                         m_title;
    DxuiListView                       * m_list = nullptr;
    std::unique_ptr<DebuggerPaneFrame>   m_frame;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane
//
//  The user's symbol tables or the machine's, each a heading with its name,
//  its count and a check that turns it on or off, and its symbols under it,
//  each with its address. A user table loaded from a file has the file's
//  name. A press on a heading opens or closes it; every heading starts
//  closed. The pane's Find searches the open tables. Activating a symbol
//  moves the disassembly to its address. The rows are built again only when
//  the symbols change or a table opens or closes.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolsPane : public ToolListPane
{
public:
    //  Which tables the pane lists: those the user loads and adds to, or the
    //  ROMs', the machine's and BASIC's.
    enum class Kind
    {
        User,
        Machine,
    };

    //  What a row is: a table's heading, or one of its symbols.
    struct Row
    {
        std::string  table;
        std::string  name;
        Word         address    = 0;
        bool         isHeading  = false;
        bool         isConstant = false;
    };

    //  The column a table's symbols are sorted by.
    enum class Sort
    {
        Name,
        Address,
    };

    SymbolsPane (IDebuggerPaneHost & host, Kind kind);

    void                 Configure         () override;
    void                 Apply             (const DebuggerViewSnapshot & snapshot) override;
    bool                 AddMenuItems      (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                 RouteContentMouse (const DxuiMouseEvent & ev) override;
    const Row          * GetRow            (int row) const;

    //  A table's heading opened or closed, by the table's name.
    void                 ToggleTable       (const std::string & table);

    static bool          IsUserTable       (SymbolTableId table);

    //  Each table's symbols are sorted by sort, by name ignoring case or by
    //  address, then by the other, in reverse when isReverse.
    static std::vector<Row>                              GetRows  (const DebuggerViewSnapshot::SymbolsState & symbols, Kind kind, const std::set<std::string> & open,
                                                                   Sort sort = Sort::Name, bool isReverse = false);
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const DebuggerViewSnapshot::SymbolsState & symbols, const std::vector<Row> & rows, const std::set<std::string> & open);

private:
    void  SaveTable    (const std::string & table);
    void  RebuildRows  ();
    void  SortBy       (int column);

    //  The file a table was loaded from, or empty for a table filled any
    //  other way.
    static std::string  GetFileOf (const DebuggerViewSnapshot::SymbolsState & symbols, const std::string & table);

    Kind                                                         m_kind;
    std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>    m_symbols;
    std::vector<Row>                                             m_rows;
    std::set<std::string>                                        m_open;   // the tables whose headings are open
    Sort                                                         m_sort      = Sort::Name;
    bool                                                         m_isReverse = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane
//
//  The bookmarks, each its number, its address and the symbol there, sorted
//  by the heading last clicked. Activating one moves the disassembly to its
//  address.
//
////////////////////////////////////////////////////////////////////////////////

class BookmarksPane : public ToolListPane
{
public:
    explicit BookmarksPane (IDebuggerPaneHost & host);

    void  Configure    () override;
    void  Apply        (const DebuggerViewSnapshot & snapshot) override;
    bool  AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool  RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused) override;

    //  The bookmark a row has, none for no such row.
    std::optional<DebuggerViewSnapshot::WatchLine>  GetBookmark (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks,
                                                                    const DebuggerViewSnapshot::SymbolsState      * symbols);

    //  The bookmark after `from`, or before it with `isBackward`, round
    //  from the last to the first; none when there are none.
    static std::optional<Word>  GetNext (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, Word from, bool isBackward);

    //  The bookmarks in a column's order: by number, by address, or by the
    //  symbol's text ignoring case; backward with `isReverse`, and as given
    //  for no column. Bookmarks that compare equal stay in the order given.
    static std::vector<DebuggerViewSnapshot::WatchLine>  GetSorted (std::vector<DebuggerViewSnapshot::WatchLine>   bookmarks,
                                                                     const DebuggerViewSnapshot::SymbolsState     * symbols, int column, bool isReverse);

private:
    void  SortBy     (int column);

    //  The bookmarks of the rows, from the highest number down, so no clear
    //  moves another one's number first.
    void  DeleteRows (const std::vector<int> & rows);

    std::vector<DebuggerViewSnapshot::WatchLine>      m_bookmarks;
    int                                               m_sort      = -1;
    bool                                              m_isReverse = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane
//
//  The ranges marked as data, each its name, its range and its kind, sorted
//  by the heading last clicked and filtered by kind from the Kind heading.
//  Activating one moves the disassembly to its first address.
//
////////////////////////////////////////////////////////////////////////////////

class DataBlocksPane : public ToolListPane
{
public:
    explicit DataBlocksPane (IDebuggerPaneHost & host);

    void  Configure    () override;
    void  Apply        (const DebuggerViewSnapshot & snapshot) override;
    bool  AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool  RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused) override;

    //  The block a row has, none for no such row.
    std::optional<DataBlockEntry>  GetBlock (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DataBlockEntry> & blocks);

    //  The blocks in a column's order: by name ignoring case, by first
    //  address, or by the kind's text; backward with `isReverse`, and as
    //  given for no column. Blocks that compare equal stay in the order given.
    static std::vector<DataBlockEntry>  GetSorted   (std::vector<DataBlockEntry> blocks, int column, bool isReverse);
    static const wchar_t              * GetKindText (DataBlockKind kind);

    //  The directive that marks a range of a block's kind, as DB4 or ASC.
    static std::string                  GetDirective (const DataBlockEntry & block);

    //  The Kind heading's filter.
    ListValueFilters                  & GetFilters  () { return m_filters; }

private:
    static constexpr size_t  kKindColumn = 2;

    void  SortBy     (int column);
    void  DeleteRows (const std::vector<int> & rows);
    void  Rename     (const DataBlockEntry & block);

    std::vector<DataBlockEntry>    m_blocks;
    ListValueFilters               m_filters;
    int                            m_sort      = -1;
    bool                           m_isReverse = false;
};
