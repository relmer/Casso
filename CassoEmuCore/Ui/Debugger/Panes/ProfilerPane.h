#pragma once

#include "Ui/Debugger/Panes/ProfileRows.h"
#include "Ui/Debugger/Panes/ProfileTimelineNavigator.h"
#include "Ui/Debugger/Panes/ToolPanes.h"

class FlameGraphView;
class ProfileProgress;
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
//  The wheel over the timeline zooms it around the pointer, as Ctrl+wheel
//  anywhere over the pane does, and a drag on it selects frames, whose
//  profile the list and the flame graph are drawn from;
//  each zoom, selection and flame graph zoom is a state that the mouse's
//  back and forward buttons and Alt+Left and Alt+Right move through.
//
////////////////////////////////////////////////////////////////////////////////

class ProfilerPane : public ToolListPane, private IProfileNavigatorOwner
{
public:
    static constexpr int       kTotalsHeightDip  = 22;
    static constexpr int       kProgressWidthDip = 280;
    static constexpr uint64_t  kRebuildCycles    = 17030;   // a frame: a stop nearer than this to the last build doesn't rebuild the profile

    //  The periods in the Show profile drop-down: the words PROFILE HISTORY
    //  takes, and the drop-down's.
    struct Period
    {
        const char     * words;
        const wchar_t  * label;
    };

    static const std::vector<Period> &  GetPeriods ();

    //  The period whose HISTORY argument is `argument`; the first, the last
    //  5 seconds, for an empty or unknown one.
    static size_t                       GetPeriodIndex (const std::string & argument);

    explicit ProfilerPane (const DebuggerPaneParts & parts);
    ~ProfilerPane() override;

    ProfileRows::View  GetView () const { return m_view; }
    bool               IsOn    () const { return m_isOn; }
    void               SetView (ProfileRows::View view);

    //  A choice in the Show profile drop-down, by its index in GetPeriods,
    //  which runs PROFILE HISTORY for it; and whether a profile is building,
    //  with the progress bar up.
    void                                ShowPeriod (size_t period);

    //  Builds the profile of the period the user chose, and saves the choice for the next session.
    void  ChoosePeriod (size_t period);
    bool  IsBuilding   () const { return m_isBuilding; }

    //  The timeline's zoom and selection, and the profile the list is drawn
    //  from.
    ProfileTimelineNavigator  & GetNavigator () { return m_navigator; }

    //  The bar's commands, as last built.
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetCommands () const { return m_commands; }

    void                         CreateControls        () override;
    void                         Configure             () override;
    void                         AddToDock             (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>  GetControls           () const override;
    std::vector<DxuiToolbar *>   GetBars               () const override { return { m_bar.GetBar() }; }
    void                         Apply                 (const DebuggerViewSnapshot & snapshot) override;
    bool                         Tick                  (int64_t nowMs) override;
    void                         PlaceBar              () override { m_bar.Place(); BuildIfDue(); }
    bool                         RouteEarlyMouse       (const DxuiMouseEvent & ev) override;
    bool                         RouteBarMouse         (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteContentMouse     (const DxuiMouseEvent & ev) override;
    bool                         ZoomContent           (float notches, std::optional<POINT> clientPx) override { return m_navigator.ZoomContent (notches, clientPx); }
    bool                         ResetContentZoom      () override { return m_navigator.ResetContentZoom(); }
    bool                         RouteEditKey          (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                         TryGetTipAt           (POINT clientPx, DxuiTip & tip);
    std::string                  GetTipHeadWithComment (const ProfileRoutineEntry & routine) const;
    bool                         AddMenuItems          (DxuiListView * list, int row, PaneMenuItems & items) override;
    void                         AddContentMenuItems   (POINT clientPx, PaneMenuItems & items) override;

private:
    void  RunProfile     (const char * words) override;
    void  Refresh        () override;
    void  BuildIfDue     ();
    void  SetBarEntries  ();
    void  SetViewMenu    ();
    void  Save           ();
    void  SortBy         (int column);

    const LatestSnapshot                       & m_latest;
    PaneBarSlot                                  m_bar;
    ProfileTimeline                            * m_timeline        = nullptr;
    ProfileProgress                            * m_progress        = nullptr;
    DxuiLabel                                  * m_totals          = nullptr;
    std::unique_ptr<ToolbarLabelEntry>           m_showLabel;
    size_t                                       m_period          = 0;
    bool                                         m_isPeriodRead    = false;
    bool                                         m_isBuilding      = false;
    bool                                         m_isPaused        = false;
    std::optional<uint64_t>                      m_stopCycle;   // where the machine is stopped, with history
    std::optional<uint64_t>                      m_builtCycle;   // the stop the last profile was requested at
    FlameGraphView                             * m_flameGraph      = nullptr;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;
    ProfileRows::View                            m_view            = ProfileRows::View::Routines;
    ProfileRows::Sorts                           m_sorts           = ProfileRows::GetDefaultSorts();
    std::vector<size_t>                          m_order;   // the list's rows as indexes in the profile's entries
    std::optional<size_t>                        m_selectedRoutine;   // the flame graph's selection, an index in the profile's routines
    bool                                         m_isOn            = false;
    ProfileTimelineNavigator                     m_navigator;
};
