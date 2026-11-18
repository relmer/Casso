#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileZoom.h"

class FlameGraphView;
class IDebuggerPaneHost;
struct DebuggerPaneParts;
class ProfileTimeline;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  IProfileNavigatorOwner
//
//  The pane a ProfileTimelineNavigator calls: to build a profile, and to
//  fill its list again once the lists' profile changes.
//
////////////////////////////////////////////////////////////////////////////////

class IProfileNavigatorOwner
{
public:
    virtual ~IProfileNavigatorOwner() = default;

    virtual void  RunProfile (const char * words) = 0;
    virtual void  Refresh    () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator
//
//  The Profiler's timeline zoom and frame selection. The wheel over the
//  timeline zooms it around the pointer, and a drag on it selects frames,
//  whose profile the lists and the flame graph are drawn from. Each zoom,
//  selection and flame graph zoom is a state that back and forward move
//  through. Each range's profile is stored, so a move back or forward
//  doesn't start a build.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileTimelineNavigator
{
public:
    using Range = ProfileZoom::Range;

    //  A profile and its address counts, as a snapshot has them.
    struct Profiles
    {
        std::optional<ProfileData>  byOpcode;
        std::optional<ProfileData>  byAddress;
    };

    ProfileTimelineNavigator (const DebuggerPaneParts & parts, IProfileNavigatorOwner & owner);

    void  SetControls (ProfileTimeline * timeline, FlameGraphView * flameGraph);

    //  The profile the lists and the flame graph are drawn from, and the
    //  zoom's states.
    const Profiles     & GetLists () const { return m_lists; }
    const ProfileZoom  & GetZoom  () const { return m_zoom; }

    ProfileZoom::Target  TakeSnapshotProfile (const std::optional<ProfileData> & byOpcode, const std::optional<ProfileData> & byAddress);
    void                 Reset               ();
    void                 OnFlameZoom         (size_t node);
    bool                 Tick                (int64_t nowMs);
    bool                 GoBack              ();
    bool                 GoForward           ();
    bool                 ZoomAt              (const DxuiMouseEvent & ev);
    bool                 ZoomContent         (float notches, std::optional<POINT> clientPx);
    bool                 ResetContentZoom    ();
    bool                 RouteDrag           (const DxuiMouseEvent & ev);
    bool                 IsOverTimeline      (POINT positionDip) const;

private:
    static constexpr int64_t  kWheelGestureMs = 600;    // a pause in the wheel that starts a new state
    static constexpr int64_t  kZoomSettleMs   = 400;    // the wait after the wheel stops before a build
    static constexpr int      kDragDip        = 3;      // the move that makes a press on the timeline a drag
    static constexpr double   kZoomPerNotch   = 0.8;

    void  Take        (const Profiles & profiles, ProfileZoom::Target target);
    void  SetTimeline (const Profiles & profiles, const std::optional<Range> & range);
    void  SetLists    (const Profiles & profiles, const std::optional<Range> & range);
    void  Request     (const Range & range, bool isSelection);
    void  ApplyState  (bool isFlameZoomSet);
    void  PushState   (const ProfileZoom::State & state);

    IDebuggerPaneHost         & m_host;
    IProfileNavigatorOwner    & m_owner;
    ProfileTimeline           * m_timeline       = nullptr;
    FlameGraphView            * m_flameGraph     = nullptr;
    Profiles                    m_lists;            // the lists' and the flame graph's
    Profiles                    m_timelineData;
    ProfileZoom                 m_zoom;
    std::map<Range, Profiles>   m_built;            // the ranges' profiles since the stack was emptied
    std::optional<Range>        m_wholeRange;       // the cycles of the profile with no zoom
    std::optional<Range>        m_timelineRange;    // the cycles of the timeline's profile
    std::optional<Range>        m_listsRange;       // and of the lists'
    bool                        m_isSelectionDue = false;
    std::optional<size_t>       m_flameZoomDue;
    int64_t                     m_lastWheelMs    = 0;
    std::optional<int64_t>      m_zoomDueMs;
    std::optional<int>          m_dragFromX;
    std::optional<Range>        m_dragRange;
};
