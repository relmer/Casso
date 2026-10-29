#pragma once

#include "Ui/Debugger/Panes/ProfileZoom.h"
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
//  The wheel over the timeline zooms it around the pointer, and a drag on it
//  selects frames, whose profile the list and the flame graph are drawn from;
//  each zoom, selection and flame graph zoom is a state that the mouse's
//  back and forward buttons and Alt+Left and Alt+Right move through.
//
////////////////////////////////////////////////////////////////////////////////

class ProfilerPane : public ToolListPane
{
public:
    //  The views, most used first, as the view drop-down lists them.
    enum class View
    {
        Routines,
        Tree,
        Flame,
        Addresses,
        Opcodes,
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

    //  The zoom, and a move back or forward through its states.
    const ProfileZoom &                 GetZoom    () const { return m_zoom; }
    bool                                GoBack     ();
    bool                                GoForward  ();

    void                         CreateControls      () override;
    void                         Configure           () override;
    void                         AddToDock           (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>  GetControls         () const override;
    std::vector<DxuiToolbar *>   GetBars             () const override { return { m_bar.GetBar() }; }
    void                         Apply               (const DebuggerViewSnapshot & snapshot) override;
    bool                         Tick                (int64_t nowMs) override;
    void                         PlaceBar            () override { m_bar.Place(); BuildIfDue(); }
    bool                         RouteEarlyMouse     (const DxuiMouseEvent & ev) override;
    bool                         RouteBarMouse       (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteContentMouse   (const DxuiMouseEvent & ev) override;
    bool                         RouteEditKey        (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    const wchar_t              * GetBarTip           (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                         TryGetTip           (POINT clientPx, PaneTip & tip) override;
    bool                         AddMenuItems        (DxuiListView * list, int row, PaneMenuItems & items) override;
    void                         AddContentMenuItems (POINT clientPx, PaneMenuItems & items) override;

    //  The drop-down's words for a view.
    static const wchar_t *  GetViewLabel (View view);

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<ProfileData> & profile, View view, bool isOn);

    //  The line above the list: the profile's totals and the time its cycles
    //  span; empty with no profile.
    static std::wstring  GetTotalsText (const std::optional<ProfileData> & profile);
    static std::wstring  FormatSeconds (double seconds);

    //  The address a row's activation moves the disassembly to: an address
    //  row's, or a routine's start other than Top level's. The row is an
    //  index in the profile's entries, before any sort.
    static std::optional<Word>  GetRowAddress (const std::optional<ProfileData> & profile, View view, int row);

    //  How a view's list is sorted: a column and its direction.
    struct Sort
    {
        int   column     = 0;
        bool  descending = true;
    };

    //  The sort a view opens with: the opcodes and the addresses by cycles,
    //  the routines and the call tree by total, most first.
    static Sort                 GetDefaultSort (View view);

    //  The order of a view's rows as indexes in the profile's entries, by a
    //  column, ties in the profile's order. The call tree sorts each
    //  routine's callees among themselves, under their caller.
    static std::vector<size_t>  GetOrder       (const ProfileData & profile, View view, int column, bool descending);

private:
    void  Refresh        ();
    void  BuildIfDue     ();
    void  SetBarEntries  ();
    void  SetViewMenu    ();
    void  RunProfile     (const char * words);
    void  Save           ();
    void  SortBy         (int column);

    //  A profile and its address counts, as a snapshot has them.
    struct Profiles
    {
        std::optional<ProfileData>  byOpcode;
        std::optional<ProfileData>  byAddress;
    };

    using Range = ProfileZoom::Range;

    static constexpr int64_t  kWheelGestureMs = 600;    // a pause in the wheel that starts a new state
    static constexpr int64_t  kZoomSettleMs   = 400;    // the wait after the wheel stops before a build
    static constexpr int      kDragDip        = 3;      // the move that makes a press on the timeline a drag
    static constexpr double   kZoomPerNotch   = 0.8;

    void  Take             (const Profiles & profiles, ProfileZoom::Target target);
    void  SetTimeline      (const Profiles & profiles, const std::optional<Range> & range);
    void  SetLists         (const Profiles & profiles, const std::optional<Range> & range);
    void  Request          (const Range & range, bool isSelection);
    void  ApplyState       (bool isFlameZoomSet);
    void  PushState        (const ProfileZoom::State & state);
    bool  ZoomAt           (const DxuiMouseEvent & ev);
    bool  RouteDrag        (const DxuiMouseEvent & ev);
    bool  IsOverTimeline   (POINT positionDip) const;

    //  A row's value in a column, to sort by: a number, or text for a name.
    struct SortKey
    {
        uint64_t     number = 0;
        std::string  text;
    };

    static bool     IsTextColumn (View view, int column);
    static SortKey  GetSortKey   (const ProfileData & profile, View view, size_t entry, int column);

    PaneBarSlot                                  m_bar;
    ProfileTimeline                            * m_timeline        = nullptr;
    ProfileProgress                            * m_progress        = nullptr;
    DxuiLabel                                  * m_totals          = nullptr;
    std::unique_ptr<ToolbarLabelEntry>           m_showLabel;
    size_t                                       m_period          = 1;
    bool                                         m_isBuilding      = false;
    bool                                         m_isPaused        = false;
    std::optional<uint64_t>                      m_stopCycle;        // where the machine is stopped, with history
    std::optional<uint64_t>                      m_builtCycle;       // the stop the last profile was requested at
    FlameGraphView                             * m_flameGraph      = nullptr;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;
    View                                         m_view            = View::Routines;
    std::array<Sort, kViewCount>                 m_sorts           = { GetDefaultSort (View::Routines), GetDefaultSort (View::Tree), GetDefaultSort (View::Flame),
                                                                       GetDefaultSort (View::Addresses), GetDefaultSort (View::Opcodes) };
    std::vector<size_t>                          m_order;            // the list's rows as indexes in the profile's entries
    std::optional<size_t>                        m_selectedRoutine;  // the flame graph's selection, an index in the profile's routines
    bool                                         m_isOn            = false;
    std::optional<ProfileData>                   m_byOpcode;         // the lists' and the flame graph's
    std::optional<ProfileData>                   m_byAddress;
    Profiles                                     m_timelineData;
    ProfileZoom                                  m_zoom;
    std::map<Range, Profiles>                    m_built;            // the ranges' profiles since the stack was emptied
    std::optional<Range>                         m_wholeRange;       // the cycles of the profile with no zoom
    std::optional<Range>                         m_timelineRange;    // the cycles of the timeline's profile
    std::optional<Range>                         m_listsRange;       // and of the lists'
    bool                                         m_isSelectionDue  = false;
    std::optional<size_t>                        m_flameZoomDue;
    int64_t                                      m_lastWheelMs     = 0;
    std::optional<int64_t>                       m_zoomDueMs;
    std::optional<int>                           m_dragFromX;
    std::optional<Range>                         m_dragRange;
};
