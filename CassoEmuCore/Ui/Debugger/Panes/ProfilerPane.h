#pragma once

#include "Ui/Debugger/Panes/ToolPanes.h"

class FlameGraphView;
class ProfileProgress;
class ProfileTimeline;
class ToolbarLabelEntry;





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

    static constexpr int  kViewCount        = 5;
    static constexpr int  kTotalsHeightDip  = 22;
    static constexpr int  kProgressWidthDip = 280;

    //  The periods in the Show profile drop-down: the words PROFILE HISTORY
    //  takes, and the drop-down's.
    struct Period
    {
        const char     * words;
        const wchar_t  * label;
    };

    static const std::vector<Period> &  GetPeriods ();

    explicit ProfilerPane (IDebuggerPaneHost & host);
    ~ProfilerPane() override;

    View                                GetView    () const { return m_view; }
    bool                                IsOn       () const { return m_isOn; }
    const std::optional<ProfileData> &  GetProfile () const { return m_byOpcode; }
    void                                SetView    (View view);

    //  A choice in the Show profile drop-down, by its index in GetPeriods,
    //  which runs PROFILE HISTORY for it; and whether a profile is building,
    //  with the progress bar up.
    void                                ShowPeriod (size_t period);
    bool                                IsBuilding () const { return m_isBuilding; }

    //  The bar's commands, as last built.
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetCommands () const { return m_commands; }

    void                         CreateControls    () override;
    void                         Configure         () override;
    void                         AddToDock         (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>  GetControls       () const override;
    std::vector<DxuiToolbar *>   GetBars           () const override { return { m_bar.GetBar() }; }
    void                         Apply             (const DebuggerViewSnapshot & snapshot) override;
    void                         PlaceBar          () override { m_bar.Place(); BuildIfDue(); }
    bool                         RouteBarMouse     (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteContentMouse (const DxuiMouseEvent & ev) override;
    const wchar_t              * GetBarTip         (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                         TryGetTip         (POINT clientPx, PaneTip & tip) override;
    bool                         AddMenuItems      (DxuiListView * list, int row, PaneMenuItems & items) override;

    //  The drop-down's words for a view.
    static const wchar_t *  GetViewLabel (View view);

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<ProfileData> & profile, View view, bool isOn);

    //  The line above the list: the profile's totals and the time its cycles
    //  span; empty with no profile.
    static std::wstring  GetTotalsText (const std::optional<ProfileData> & profile);
    static std::wstring  FormatSeconds (double seconds);

    //  The address a row's activation moves the disassembly to: an address
    //  row's, or a routine's start other than Top level's.
    static std::optional<Word>  GetRowAddress (const std::optional<ProfileData> & profile, View view, int row);

private:
    void  Refresh        ();
    void  BuildIfDue     ();
    void  SetBarEntries  ();
    void  SetViewMenu    ();
    void  RunProfile     (const char * words);
    void  Save           ();

    PaneBarSlot                                  m_bar;
    ProfileTimeline                            * m_timeline    = nullptr;
    ProfileProgress                            * m_progress    = nullptr;
    DxuiLabel                                  * m_totals      = nullptr;
    std::unique_ptr<ToolbarLabelEntry>           m_showLabel;
    size_t                                       m_period      = 1;
    bool                                         m_isBuilding  = false;
    bool                                         m_isPaused    = false;
    std::optional<uint64_t>                      m_stopCycle;            // where the machine is stopped, with history
    std::optional<uint64_t>                      m_builtCycle;           // the stop the last profile was requested at
    FlameGraphView                             * m_flameGraph  = nullptr;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;
    View                                         m_view        = View::Opcodes;
    bool                                         m_isOn        = false;
    std::optional<ProfileData>                   m_byOpcode;
    std::optional<ProfileData>                   m_byAddress;
};
