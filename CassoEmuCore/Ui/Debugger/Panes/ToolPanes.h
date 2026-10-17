#pragma once

#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane
//
//  The symbol tables, each a heading with its name, its count and a check
//  that turns it on or off, and its symbols under it, each with its address
//  (FR-183). The pane's Find searches them. Activating a symbol moves the
//  disassembly to its address.
//
//  The window owns the list; the pane has a pointer to it. The rows are
//  built again only when the symbols change.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolsPane
{
public:
    using ShowFn   = std::function<void (Word address)>;
    using EnableFn = std::function<void (const std::string & table, bool enable)>;

    //  What a row is: a table's heading, or one of its symbols.
    struct Row
    {
        std::string  table;
        std::string  name;
        Word         address   = 0;
        bool         isHeading = false;
    };

    SymbolsPane (DxuiListView * list, ShowFn show, EnableFn enable);

    DxuiListView *  GetList () const { return m_list; }

    void         Configure ();
    void         Apply     (const std::shared_ptr<const DebuggerViewSnapshot::SymbolsState> & symbols);
    const Row *  GetRow    (int row) const;

    static std::vector<Row>                              GetRows  (const DebuggerViewSnapshot::SymbolsState & symbols);
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const DebuggerViewSnapshot::SymbolsState & symbols, const std::vector<Row> & rows);

private:
    DxuiListView                                               * m_list = nullptr;
    ShowFn                                                       m_show;
    EnableFn                                                     m_enable;
    std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>    m_symbols;
    std::vector<Row>                                             m_rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane
//
//  The bookmarks, each its number, its address and the symbol there
//  (FR-189). Activating one moves the disassembly to its address.
//
////////////////////////////////////////////////////////////////////////////////

class BookmarksPane
{
public:
    using ShowFn = std::function<void (Word address)>;

    BookmarksPane (DxuiListView * list, ShowFn show);

    DxuiListView *  GetList () const { return m_list; }

    void  Configure ();
    void  Apply     (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, const DebuggerViewSnapshot::SymbolsState * symbols);

    //  The bookmark a row has, none for no such row.
    std::optional<DebuggerViewSnapshot::WatchLine>  GetBookmark (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks,
                                                                    const DebuggerViewSnapshot::SymbolsState      * symbols);

    //  The bookmark after `from`, or before it with `isBackward`, round
    //  from the last to the first; none when there are none.
    static std::optional<Word>  GetNext (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, Word from, bool isBackward);

private:
    DxuiListView                                    * m_list = nullptr;
    ShowFn                                            m_show;
    std::vector<DebuggerViewSnapshot::WatchLine>      m_bookmarks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane
//
//  The profile as PROFILE lists it (FR-193): its totals, then each opcode's
//  count and cycles, or with by-address on, the addresses that took the
//  most cycles. It is read while the machine is paused, so a running
//  machine's snapshots change only whether profiling is on.
//
////////////////////////////////////////////////////////////////////////////////

class ProfilerPane
{
public:
    using ShowFn = std::function<void (Word address)>;

    ProfilerPane (DxuiListView * list, ShowFn show);

    DxuiListView *  GetList        () const { return m_list; }
    bool            IsByAddress    () const { return m_isByAddress; }
    bool            IsOn           () const { return m_isOn; }
    void            SetByAddress   (bool isByAddress);

    void  Configure ();
    void  Apply     (const std::optional<ProfileData> & byOpcode, const std::optional<ProfileData> & byAddress, bool isOn);

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<ProfileData> & profile, bool isByAddress, bool isOn);

private:
    void  Refresh ();

    DxuiListView                 * m_list        = nullptr;
    ShowFn                         m_show;
    bool                           m_isByAddress = false;
    bool                           m_isOn        = false;
    std::optional<ProfileData>     m_byOpcode;
    std::optional<ProfileData>     m_byAddress;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane
//
//  The ranges marked as data, each its name, its range and its kind
//  (FR-182). Activating one moves the disassembly to its first address.
//
////////////////////////////////////////////////////////////////////////////////

class DataBlocksPane
{
public:
    using ShowFn = std::function<void (Word address)>;

    DataBlocksPane (DxuiListView * list, ShowFn show);

    DxuiListView *  GetList () const { return m_list; }

    void  Configure ();
    void  Apply     (const std::vector<DataBlockEntry> & blocks);

    //  The block a row has, none for no such row.
    std::optional<DataBlockEntry>  GetBlock (int row) const;

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::vector<DataBlockEntry> & blocks);

private:
    DxuiListView                 * m_list = nullptr;
    ShowFn                         m_show;
    std::vector<DataBlockEntry>    m_blocks;
};
