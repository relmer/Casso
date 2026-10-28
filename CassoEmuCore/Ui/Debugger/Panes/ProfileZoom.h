#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileTimeline.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoom
//
//  The Profiler pane's zoom of a profile built from history: the range of
//  cycles the timeline spans, zoomed in or not; a range of frames selected
//  on it, whose profile the list and the flame graph are drawn from; and
//  the flame graph's zoom. Each change is a state on a stack that back and
//  forward move through, as in a browser's history.
//
//  The pane requests a profile of the zoomed range or of the selection from
//  history, and every paused snapshot has the profile the debugger built
//  last, so a profile is taken once, by its build, and goes where the
//  request for its cycles was for. A later build the pane didn't request,
//  such as a period from the Show drop-down or one typed in the console, is
//  a whole new profile, and empties the stack.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileZoom
{
public:
    using Range = ProfileTimeline::Range;

    struct State
    {
        std::optional<Range>  view;            // none for the whole profile
        std::optional<Range>  selection;       // none for all of the view
        size_t                flameZoom = 0;   // the flame graph's zoomed node
    };

    //  Which of the pane's parts a profile is for; Whole for a new profile,
    //  for every part, with no zoom.
    enum class Target
    {
        None,
        Timeline,
        Lists,
        Both,
        Whole,
    };

    //  A request for a range's profile.
    struct Request
    {
        Range  range;
        bool   isSelection = false;
    };

    static constexpr int64_t  kMinFrames = 2;

    //  A new profile from the Show drop-down: no zoom, no selection, and an
    //  empty stack.
    void  Reset ();

    const State &  GetState () const { return m_states[m_at]; }

    //  A new state after the current one, dropping any the current one was
    //  gone back from; Replace changes the current one in place, as each
    //  notch of one turn of the wheel does.
    void  Push    (const State & state);
    void  Replace (const State & state);
    bool  Back    ();
    bool  Forward ();
    bool  CanGoBack    () const { return m_at > 0; }
    bool  CanGoForward () const { return m_at + 1 < m_states.size(); }

    //  The request a later profile is matched against, and where a profile
    //  goes: Route by its range and its build, GetTarget by a request. A
    //  profile counted as the machine ran is Whole.
    void    SetRequest (const Request & request);
    Target  Route      (const std::optional<CycleRange> & historyRange, uint64_t build);
    Target  GetTarget  (const Request & request) const;

    //  The view zoomed by factor around anchor, less than 1 zooming in, and
    //  moved along by some cycles, each within limit and no narrower than
    //  kMinFrames frames; none once it covers all of limit.
    static std::optional<Range>  ZoomAround (const Range & view, int64_t anchor, double factor, const Range & limit);
    static std::optional<Range>  Pan        (const Range & view, int64_t by, const Range & limit);

    //  The whole frames from one cycle to another, either way round, at
    //  least one, within limit.
    static Range                 GetFramesBetween (int64_t fromCycle, int64_t toCycle, const Range & limit);

private:
    std::vector<State>      m_states    = { State() };
    size_t                  m_at        = 0;
    std::optional<Request>  m_expected;
    std::vector<Range>      m_requested;             // the ranges requested since the stack was emptied
    uint64_t                m_seenBuild = 0;         // the latest build taken
};
