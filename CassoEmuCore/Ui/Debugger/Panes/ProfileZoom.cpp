#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileZoom.h"

#include "Debugger/RoutineProfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Reset
//
////////////////////////////////////////////////////////////////////////////////

void ProfileZoom::Reset()
{
    m_states = { State() };
    m_at     = 0;
    m_expected.reset();
    m_requested.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::SetRequest
//
////////////////////////////////////////////////////////////////////////////////

void ProfileZoom::SetRequest (const Request & request)
{
    m_expected = request;

    if (std::find (m_requested.begin(), m_requested.end(), request.range) == m_requested.end())
    {
        m_requested.push_back (request.range);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Push
//
////////////////////////////////////////////////////////////////////////////////

void ProfileZoom::Push (const State & state)
{
    m_states.resize (m_at + 1);
    m_states.push_back (state);
    m_at = m_states.size() - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Replace
//
////////////////////////////////////////////////////////////////////////////////

void ProfileZoom::Replace (const State & state)
{
    m_states[m_at] = state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Back
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileZoom::Back()
{
    if (!CanGoBack())
    {
        return false;
    }

    m_at--;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Forward
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileZoom::Forward()
{
    if (!CanGoForward())
    {
        return false;
    }

    m_at++;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Route
//
//  A build already taken goes nowhere. One of the range requested last goes
//  where the request was for: a selection's to the lists while that
//  selection is current, and a view's to the timeline, and to the lists too
//  when nothing is selected. One of a range requested before that goes
//  nowhere, and any other is a whole new profile, the first range of a new
//  stack.
//
////////////////////////////////////////////////////////////////////////////////

ProfileZoom::Target ProfileZoom::Route (const std::optional<CycleRange> & historyRange, uint64_t build)
{
    Range  range;



    if (!historyRange.has_value())
    {
        Reset();
        return Target::Whole;
    }

    if (build <= m_seenBuild)
    {
        return Target::None;
    }

    m_seenBuild = build;
    range       = Range { (int64_t) historyRange->first, (int64_t) historyRange->second };

    if (std::find (m_requested.begin(), m_requested.end(), range) == m_requested.end())
    {
        Reset();
        m_requested.push_back (range);
        return Target::Whole;
    }

    if (!m_expected.has_value() || m_expected->range != range)
    {
        return Target::None;
    }

    return GetTarget (*m_expected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::GetTarget
//
////////////////////////////////////////////////////////////////////////////////

ProfileZoom::Target ProfileZoom::GetTarget (const Request & request) const
{
    if (request.isSelection)
    {
        return (GetState().selection == request.range) ? Target::Lists : Target::None;
    }

    return GetState().selection.has_value() ? Target::Timeline : Target::Both;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::ZoomAround
//
//  The anchor stays where it is across the view, so the cycle under the
//  pointer stays under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ProfileZoom::Range> ProfileZoom::ZoomAround (
    const Range  & view,
    int64_t        anchor,
    double         factor,
    const Range  & limit)
{
    int64_t  minWidth = kMinFrames * (int64_t) RoutineProfile::kCyclesPerFrame;
    int64_t  width    = std::max (view.second - view.first, (int64_t) 1);
    int64_t  next     = std::clamp ((int64_t) std::llround ((double) width * factor), minWidth, limit.second - limit.first);
    double   share    = (double) (anchor - view.first) / (double) width;
    int64_t  from     = anchor - (int64_t) std::llround (share * (double) next);



    if (next >= limit.second - limit.first)
    {
        return std::nullopt;
    }

    from = std::clamp (from, limit.first, limit.second - next);
    return Range { from, from + next };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::Pan
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ProfileZoom::Range> ProfileZoom::Pan (
    const Range  & view,
    int64_t        by,
    const Range  & limit)
{
    int64_t  width = view.second - view.first;
    int64_t  from  = 0;



    if (width >= limit.second - limit.first)
    {
        return std::nullopt;
    }

    from = std::clamp (view.first + by, limit.first, limit.second - width);
    return Range { from, from + width };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom::GetFramesBetween
//
////////////////////////////////////////////////////////////////////////////////

ProfileZoom::Range ProfileZoom::GetFramesBetween (int64_t fromCycle, int64_t toCycle, const Range & limit)
{
    constexpr int64_t  kFrame = (int64_t) RoutineProfile::kCyclesPerFrame;
    int64_t            first  = std::min (fromCycle, toCycle);
    int64_t            last   = std::max (fromCycle, toCycle);



    first = std::max (first - first % kFrame, limit.first);
    last  = std::min (std::max ((last + kFrame - 1) / kFrame * kFrame, first + kFrame), limit.second);

    return Range { first, std::max (last, first + 1) };
}
