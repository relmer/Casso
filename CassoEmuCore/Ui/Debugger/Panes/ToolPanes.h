#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

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

    std::wstring                 GetId          () const override { return m_id; }
    std::wstring                 GetTitle       () const override { return m_title; }
    void                         CreateControls () override;
    void                         AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl               * GetContent     () const override;
    std::vector<IDxuiControl *>  GetControls    () const override { return { m_list }; }
    std::vector<DxuiListView *>  GetLists       () const override { return { m_list }; }

protected:
    //  Copy, for the rows selected in the list.
    void                         AddCopyItem    (PaneMenuItems & items);

    //  The path the user picks to save to; empty when the user backs out or
    //  the window has no file pickers.
    std::wstring                 PickSaveFile   (const wchar_t * filterName, const wchar_t * filter, const wchar_t * extension, const std::wstring & fileName);

    //  One muted cell across the row.
    static std::vector<DxuiListView::Cell>  MakeNote (const std::wstring & text);

    std::wstring                         m_id;
    std::wstring                         m_title;
    DxuiListView                       * m_list = nullptr;
    std::unique_ptr<DebuggerPaneFrame>   m_frame;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane
//
//  The symbol tables, each a heading with its name, its count and a check
//  that turns it on or off, and its symbols under it, each with its address.
//  The pane's Find searches them. Activating a symbol moves the disassembly
//  to its address. The rows are built again only when the symbols change.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolsPane : public ToolListPane
{
public:
    //  What a row is: a table's heading, or one of its symbols.
    struct Row
    {
        std::string  table;
        std::string  name;
        Word         address   = 0;
        bool         isHeading = false;
    };

    explicit SymbolsPane (IDebuggerPaneHost & host);

    void         Configure    () override;
    void         Apply        (const DebuggerViewSnapshot & snapshot) override;
    bool         AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items) override;
    const Row *  GetRow       (int row) const;

    static std::vector<Row>                              GetRows  (const DebuggerViewSnapshot::SymbolsState & symbols);
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const DebuggerViewSnapshot::SymbolsState & symbols, const std::vector<Row> & rows);

private:
    void  SaveTable (const std::string & table);

    std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>    m_symbols;
    std::vector<Row>                                             m_rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane
//
//  The bookmarks, each its number, its address and the symbol there.
//  Activating one moves the disassembly to its address.
//
////////////////////////////////////////////////////////////////////////////////

class BookmarksPane : public ToolListPane
{
public:
    explicit BookmarksPane (IDebuggerPaneHost & host);

    void  Configure    () override;
    void  Apply        (const DebuggerViewSnapshot & snapshot) override;
    bool  AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items) override;

    //  The bookmark a row has, none for no such row.
    std::optional<DebuggerViewSnapshot::WatchLine>  GetBookmark (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks,
                                                                    const DebuggerViewSnapshot::SymbolsState      * symbols);

    //  The bookmark after `from`, or before it with `isBackward`, round
    //  from the last to the first; none when there are none.
    static std::optional<Word>  GetNext (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, Word from, bool isBackward);

private:
    std::vector<DebuggerViewSnapshot::WatchLine>      m_bookmarks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane
//
//  The ranges marked as data, each its name, its range and its kind.
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

    //  The block a row has, none for no such row.
    std::optional<DataBlockEntry>  GetBlock (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DataBlockEntry> & blocks);

private:
    std::vector<DataBlockEntry>    m_blocks;
};
