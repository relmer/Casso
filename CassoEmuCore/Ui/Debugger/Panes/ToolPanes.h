#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DebuggerPaneFrame;
class FlameGraphView;
class ProfileTimeline;





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
    //  The path the user picks to save to; empty when the user backs out or
    //  the window has no file pickers.
    std::wstring                 PickSaveFile   (const wchar_t * filterName, const wchar_t * filter, const wchar_t * extension, const std::wstring & fileName);

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
//  ProfilerPane
//
//  The profile as PROFILE lists it: its totals, then each opcode's count and
//  cycles, the addresses that took the most cycles, the routines, or the call
//  tree, a routine two spaces in under its caller per level. Its bar turns
//  profiling on and off, resets and saves it and chooses the view; a timeline
//  of frames runs across the top, and in the flame graph view the flame graph
//  is drawn in place of the list. It is read while the machine is paused, so
//  a running machine's snapshots change only whether profiling is on.
//
////////////////////////////////////////////////////////////////////////////////

class ProfilerPane : public ToolListPane
{
public:
    enum class View
    {
        Opcodes,
        Addresses,
        Routines,
        Tree,
        Flame,
    };

    static constexpr int  kViewCount = 5;

    explicit ProfilerPane (IDebuggerPaneHost & host);
    ~ProfilerPane() override;

    View                                GetView    () const { return m_view; }
    bool                                IsOn       () const { return m_isOn; }
    const std::optional<ProfileData> &  GetProfile () const { return m_byOpcode; }
    void                                SetView    (View view);

    //  The bar's commands, as last built.
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetCommands () const { return m_commands; }

    void                         CreateControls    () override;
    void                         Configure         () override;
    void                         AddToDock         (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>  GetControls       () const override;
    std::vector<DxuiToolbar *>   GetBars           () const override { return { m_bar.GetBar() }; }
    void                         Apply             (const DebuggerViewSnapshot & snapshot) override;
    void                         PlaceBar          () override { m_bar.Place(); }
    bool                         RouteBarMouse     (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteContentMouse (const DxuiMouseEvent & ev) override;
    const wchar_t              * GetBarTip         (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                         TryGetContentTip  (POINT clientPx, std::wstring & text) const override;
    bool                         AddMenuItems      (DxuiListView * list, int row, PaneMenuItems & items) override;

    //  The drop-down's words for a view.
    static const wchar_t *  GetViewLabel (View view);

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<ProfileData> & profile, View view, bool isOn);

    //  The address a row's activation moves the disassembly to: an address
    //  row's, or a routine's start other than Top level's.
    static std::optional<Word>  GetRowAddress (const std::optional<ProfileData> & profile, View view, int row);

private:
    void  Refresh        ();
    void  SetBarEntries  ();
    void  SetViewMenu    ();
    void  RunProfile     (const char * words);
    void  Save           ();

    PaneBarSlot                                  m_bar;
    ProfileTimeline                            * m_timeline    = nullptr;
    FlameGraphView                             * m_flameGraph  = nullptr;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;
    View                                         m_view        = View::Opcodes;
    bool                                         m_isOn        = false;
    std::optional<ProfileData>                   m_byOpcode;
    std::optional<ProfileData>                   m_byAddress;
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
