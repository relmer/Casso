#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileTimelineNavigator.h"

#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::ProfileTimelineNavigator
//
////////////////////////////////////////////////////////////////////////////////

ProfileTimelineNavigator::ProfileTimelineNavigator (const DebuggerPaneParts & parts, IProfileNavigatorOwner & owner) :
    m_paneLayout (parts.layout),
    m_paneRouting (parts.routing),
    m_owner (owner)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::SetControls
//
//  The pane creates the timeline and the flame graph after this, so they
//  come in once they exist.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::SetControls (ProfileTimeline * timeline, FlameGraphView * flameGraph)
{
    m_timeline   = timeline;
    m_flameGraph = flameGraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::TakeSnapshotProfile
//
//  A snapshot's profile goes where the zoom routes it. Returns where it
//  went, or Target::None when the zoom didn't take it.
//
////////////////////////////////////////////////////////////////////////////////

ProfileZoom::Target ProfileTimelineNavigator::TakeSnapshotProfile (const std::optional<ProfileData> & byOpcode, const std::optional<ProfileData> & byAddress)
{
    ProfileZoom::Target  target = ProfileZoom::Target::None;



    if (byOpcode.has_value())
    {
        target = m_zoom.Route (byOpcode->historyRange, byOpcode->historyBuild);
        Take ({ byOpcode, byAddress }, target);
    }

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::Reset
//
//  A new period: the whole timeline, no selection and no zoom states.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::Reset()
{
    m_zoomDueMs.reset();
    m_dragFromX.reset();
    m_zoom.Reset();
    m_timeline->ResetView();
    m_timeline->SetSelection (std::nullopt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::OnFlameZoom
//
//  A zoom in the flame graph is a new state.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::OnFlameZoom (size_t node)
{
    ProfileZoom::State  state = m_zoom.GetState();



    if (state.flameZoom != node)
    {
        state.flameZoom = node;
        m_zoom.Push (state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::Tick
//
//  The profile of a zoomed view is built once the wheel has stopped.
//  Returns true while that build waits.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::Tick (int64_t nowMs)
{
    if (!m_zoomDueMs.has_value())
    {
        return false;
    }

    if (nowMs < *m_zoomDueMs)
    {
        return true;
    }

    m_zoomDueMs.reset();
    ApplyState (false);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::Take
//
//  A whole new profile is the timeline's and the lists' with no zoom; the
//  rest go to the parts the zoom routed them to. Each range's profile is
//  stored in m_built, so a move back or forward doesn't start a build.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::Take (const Profiles & profiles, ProfileZoom::Target target)
{
    using Target = ProfileZoom::Target;

    std::optional<Range>  range;



    if (target == Target::None || !profiles.byOpcode.has_value())
    {
        return;
    }

    if (profiles.byOpcode->historyRange.has_value())
    {
        range = Range { (int64_t) profiles.byOpcode->historyRange->first, (int64_t) profiles.byOpcode->historyRange->second };
    }

    if (target == Target::Whole)
    {
        m_built.clear();
        m_wholeRange     = range;
        m_isSelectionDue = false;
        m_flameZoomDue.reset();
        m_zoomDueMs.reset();
        m_dragFromX.reset();
        m_timeline->ResetView();
        m_timeline->SetSelection (std::nullopt);
    }

    if (range.has_value())
    {
        m_built[*range] = profiles;
    }

    if (target != Target::Lists)
    {
        SetTimeline (profiles, range);
    }

    if (target != Target::Timeline)
    {
        SetLists (profiles, range);
    }

    //  A selection's profile is requested once the timeline's profile of
    //  the view it is in arrives.
    if (target == Target::Timeline && m_isSelectionDue && m_zoom.GetState().selection.has_value())
    {
        m_isSelectionDue = false;
        Request (*m_zoom.GetState().selection, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::SetTimeline
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::SetTimeline (const Profiles & profiles, const std::optional<Range> & range)
{
    m_timelineData  = profiles;
    m_timelineRange = range;
    m_timeline->SetData (profiles.byOpcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::SetLists
//
//  The flame graph zooms to the same routine in the new profile, by its
//  path from Top level, unless a move back or forward sets the state's zoom.
//  Either way the state then has the zoom the flame graph is at.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::SetLists (const Profiles & profiles, const std::optional<Range> & range)
{
    ProfileZoom::State  state;



    m_lists.byOpcode  = profiles.byOpcode;
    m_lists.byAddress = profiles.byAddress;
    m_listsRange      = range;
    m_flameGraph->SetData (m_lists.byOpcode);

    if (m_flameZoomDue.has_value())
    {
        m_flameGraph->SetZoom (*m_flameZoomDue);
        m_flameZoomDue.reset();
    }

    state = m_zoom.GetState();

    if (state.flameZoom != m_flameGraph->GetZoom())
    {
        state.flameZoom = m_flameGraph->GetZoom();
        m_zoom.Replace (state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::Request
//
//  A range's profile, from those built, or built from history.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::Request (const Range & range, bool isSelection)
{
    ProfileZoom::Request  request { range, isSelection };
    auto                  built   = m_built.find (range);
    Profiles              profiles;



    m_zoom.SetRequest (request);

    if (built != m_built.end())
    {
        profiles = built->second;
        Take (profiles, m_zoom.GetTarget (request));
        return;
    }

    m_owner.RunProfile (std::format ("HISTORY CYCLES {} {}", range.first, range.second).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::ApplyState
//
//  Brings the timeline, the lists and the flame graph to the zoom's state:
//  the view at once, then each part's profile, from those built or built
//  from history. The timeline's comes first, and a selection's after it.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::ApplyState (bool isFlameZoomSet)
{
    ProfileZoom::State  state = m_zoom.GetState();
    Range               view;
    Range               lists;



    if (!m_wholeRange.has_value())
    {
        return;
    }

    view  = state.view.value_or (*m_wholeRange);
    lists = state.selection.value_or (view);

    if (state.view.has_value())
    {
        m_timeline->SetView (view.first, view.second);
    }
    else
    {
        m_timeline->ResetView();
    }

    m_timeline->SetSelection (state.selection);

    m_flameZoomDue   = isFlameZoomSet ? std::optional<size_t> (state.flameZoom) : std::nullopt;
    m_isSelectionDue = state.selection.has_value() && m_listsRange != lists;

    if (m_timelineRange != view)
    {
        Request (view, false);
    }
    else if (m_isSelectionDue)
    {
        m_isSelectionDue = false;
        Request (lists, true);
    }
    else if (m_listsRange != lists)
    {
        SetLists (m_timelineData, view);
    }
    else if (m_flameZoomDue.has_value())
    {
        m_flameGraph->SetZoom (*m_flameZoomDue);
        m_flameZoomDue.reset();
    }

    m_owner.Refresh();
    m_paneLayout.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::PushState
//
////////////////////////////////////////////////////////////////////////////////

void ProfileTimelineNavigator::PushState (const ProfileZoom::State & state)
{
    m_zoom.Push (state);
    ApplyState (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::GoBack()
{
    if (!m_zoom.Back())
    {
        return false;
    }

    ApplyState (true);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::GoForward()
{
    if (!m_zoom.Forward())
    {
        return false;
    }

    ApplyState (true);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::ZoomAt
//
//  A notch of the wheel zooms the timeline around the cycle at the pointer,
//  and a horizontal notch, or one with Shift, pans it. One turn of the wheel
//  is one state, and the view's profile is built once the wheel stops.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::ZoomAt (const DxuiMouseEvent & ev)
{
    int64_t             now          = (int64_t) GetTickCount64();
    bool                isNewGesture = now - m_lastWheelMs > kWheelGestureMs;
    ProfileZoom::State  state        = m_zoom.GetState();
    Range               view         = m_timeline->GetView();
    double              width        = (double) (view.second - view.first);



    m_lastWheelMs = now;

    if (!m_wholeRange.has_value() || ev.wheelDelta == 0.0f)
    {
        return true;
    }

    if (ev.wheelHorizontal)
    {
        state.view = ProfileZoom::Pan (view, (int64_t) (width * ev.wheelDelta / 10.0), *m_wholeRange);
    }
    else if (ev.shift)
    {
        state.view = ProfileZoom::Pan (view, (int64_t) (-width * ev.wheelDelta / 10.0), *m_wholeRange);
    }
    else
    {
        state.view = ProfileZoom::ZoomAround (view, m_timeline->GetCycleAt (ev.positionDip.x), std::pow (kZoomPerNotch, (double) ev.wheelDelta), *m_wholeRange);
    }

    if (state.view == m_zoom.GetState().view)
    {
        return true;
    }

    if (isNewGesture)
    {
        m_zoom.Push (state);
    }
    else
    {
        m_zoom.Replace (state);
    }

    if (state.view.has_value())
    {
        m_timeline->SetView (state.view->first, state.view->second);
    }
    else
    {
        m_timeline->ResetView();
    }

    m_zoomDueMs = now + kZoomSettleMs;
    m_paneLayout.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::ZoomContent
//
//  Ctrl+wheel over the rest of the pane zooms the timeline as it does over
//  the timeline, around the cycle above the pointer, since the lists and
//  the flame graph span the timeline's width. The keys zoom around the
//  middle of the view.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::ZoomContent (float notches, std::optional<POINT> clientPx)
{
    RECT            bounds = {};
    DxuiMouseEvent  wheel;



    if (m_timeline == nullptr || !m_paneRouting.IsRoutable (m_timeline) || !m_timeline->IsVisible())
    {
        return false;
    }

    bounds = m_timeline->GetBounds();

    wheel.kind        = DxuiMouseEventKind::Wheel;
    wheel.ctrl        = true;
    wheel.wheelDelta  = notches;
    wheel.positionDip = clientPx.value_or (POINT { (bounds.left + bounds.right) / 2, bounds.top });

    return ZoomAt (wheel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::ResetContentZoom
//
//  The whole profile in the timeline again, as a new state, so the back
//  button and Alt+Left move to the zoom before it.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::ResetContentZoom()
{
    ProfileZoom::State  state = m_zoom.GetState();



    if (m_timeline == nullptr || !m_paneRouting.IsRoutable (m_timeline) || !m_timeline->IsVisible())
    {
        return false;
    }

    if (state.view.has_value())
    {
        state.view.reset();

        m_zoomDueMs.reset();
        PushState (state);
        m_paneLayout.Repaint();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::IsOverTimeline
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::IsOverTimeline (POINT positionDip) const
{
    return m_paneRouting.IsRoutable (m_timeline) && m_timeline->IsVisible() && DxuiDockSite::Contains (m_timeline->GetBounds(), positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimelineNavigator::RouteDrag
//
//  A press on the timeline starts a drag that selects the frames it
//  crosses, and has the mouse until the button comes up. A click with no
//  drag clears the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileTimelineNavigator::RouteDrag (const DxuiMouseEvent & ev)
{
    ProfileZoom::State  state = m_zoom.GetState();
    bool                isEnd = ev.kind == DxuiMouseEventKind::Up || (ev.kind == DxuiMouseEventKind::Move && ev.button != DxuiMouseButton::Left);



    if (!m_dragFromX.has_value())
    {
        if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left || !m_wholeRange.has_value() || !IsOverTimeline (ev.positionDip))
        {
            return false;
        }

        m_dragFromX = ev.positionDip.x;
        m_dragRange.reset();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move && !isEnd)
    {
        if (m_dragRange.has_value() || std::abs (ev.positionDip.x - *m_dragFromX) >= kDragDip)
        {
            m_dragRange = ProfileZoom::GetFramesBetween (m_timeline->GetCycleAt (*m_dragFromX), m_timeline->GetCycleAt (ev.positionDip.x), *m_wholeRange);
            m_timeline->SetSelection (m_dragRange);
            m_paneLayout.Repaint();
        }

        return true;
    }

    if (!isEnd)
    {
        return true;
    }

    state.selection = m_dragRange;
    m_dragFromX.reset();
    m_dragRange.reset();

    if (state.selection != m_zoom.GetState().selection)
    {
        PushState (state);
    }

    return true;
}
