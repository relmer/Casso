#pragma once

#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class BreakpointBarCommands;
class DebuggerPaneFrame;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane
//
//  The breakpoints: a toolbar over a list of every breakpoint, each row its
//  checkbox, the gutter's mark and a cell for each visible column, sorted
//  by the heading last clicked. A breakpoint is renamed in a box laid over its
//  Name cell. Each change is one step on the pane's undo list.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointsPane : public IDebuggerPane
{
public:
    using Shown = std::array<bool, BreakpointColumns::kCount>;

    explicit BreakpointsPane (IDebuggerPaneHost & host);
    ~BreakpointsPane() override;

    DxuiListView                   * GetList        () const { return m_list; }
    DxuiToolbar                    * GetBar         () const { return m_bar.GetBar(); }

    //  Which columns are visible, saved by the window with the open views.
    const Shown                    & GetShown       () const { return m_shown; }
    void                             SetShown       (const Shown & shown);

    //  The breakpoint a row of the list has, in the list's order; null for a
    //  row past the end.
    const DebuggerViewSnapshot::BreakpointLine *  GetBreakpointOfRow (int row) const;

    //  Turns a column on or off; and sorts by one, a second click on the same
    //  one turning the order around.
    void                             ToggleColumn   (BreakpointColumns::Column column);
    void                             Sort           (BreakpointColumns::Column column);

    //  Whether a bar entry can act, and its step.
    bool                             IsBarEntryEnabled (int id) const;
    void                             RunBarEntry    (int id);

    //  A box over a row's Name cell with its name, all of it selected so
    //  typing replaces it; and its end, Enter or a click away committing
    //  what was typed.
    void                             BeginNameEdit  (int row);
    void                             EndNameEdit    (bool commit);

    //  BPSAVE to a file the user picks, and the breakpoints of one added.
    void                             Export         ();
    void                             Import         ();

    std::wstring                     GetId          () const override;
    std::wstring                     GetTitle       () const override { return L"Breakpoints"; }
    void                             CreateControls () override;
    void                             CreateOverlays () override;
    std::vector<IDxuiControl *>      GetOverlays    () const override { return { m_nameEditor }; }
    void                             Configure      () override;
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    std::vector<DxuiListView *>      GetLists       () const override { return { m_list }; }
    std::vector<DxuiToolbar *>       GetBars        () const override { return { m_bar.GetBar() }; }
    std::optional<PaneColorKeySpec>  GetColorKey    () const override;
    void                             Apply          (const DebuggerViewSnapshot & snapshot) override;
    void                             PlaceBar       () override { m_bar.Place(); }
    bool                             RouteEditMouse (const DxuiMouseEvent & ev) override;
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                             RouteEditKey   (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    const wchar_t                  * GetBarTip      (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                             AddMenuItems   (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                             AddColumnMenuItems (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands) override;

private:
    struct NameEdit
    {
        int           row = -1;
        int           id  = -1;
        std::wstring  opened;
    };

    void                             SetColumns     ();
    void                             ApplyRows      ();
    void                             NewBreakpoint  ();
    void                             RunStep        (BreakpointStep step);
    std::vector<DebuggerViewSnapshot::BreakpointLine>  GetSelected () const;

    PaneBarSlot                              m_bar;
    DxuiListView                           * m_list       = nullptr;
    DxuiTextInput                          * m_nameEditor = nullptr;
    NameEdit                                 m_nameEdit;
    std::unique_ptr<DebuggerPaneFrame>       m_frame;
    std::unique_ptr<BreakpointBarCommands>   m_commands;
    Shown                                    m_shown      = BreakpointColumns::GetDefaultShown();
    std::vector<size_t>                      m_order;
    int                                      m_sort       = -1;
    bool                                     m_isReverse  = false;
};
