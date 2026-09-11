#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FrameClock
//
//  The CPU thread's two readings of real time, behind one clock it is given.
//
//  Everything else the CPU thread does is measured in emulated cycles, which
//  a test controls exactly. Two things are not: how long the keyboard's
//  auto-repeat has been waiting, which must follow the wall clock or a held
//  key repeats at whatever speed the guest happens to run; and whether a
//  Maximum-speed run may publish another frame yet, which throttles the
//  render side to something a display can show. Both used to read
//  steady_clock::now() where they stood, so a test of either had to wait for
//  real time to pass and hope.
//
//  The clock is a function so a test hands in one it advances by hand. The
//  default is the real one, and the shell never notices the difference.
//
////////////////////////////////////////////////////////////////////////////////

class FrameClock
{
public:

    using TimePoint = std::chrono::steady_clock::time_point;
    using Now       = std::function<TimePoint ()>;

    //  The real clock.
    FrameClock ();

    //  A clock a test drives.
    explicit FrameClock (Now now);

    //  Microseconds the keyboard has waited since the last call, capped at
    //  `capUs`. The first call anchors the interval and reports nothing, so a
    //  machine that has just come up does not see the whole time since the
    //  epoch as one stall. A clock that has not moved reports nothing and
    //  keeps its anchor.
    uint32_t  TakeKeyRepeatElapsedUs (uint32_t capUs);

    //  Whether a frame may be published now. Below Maximum speed the answer
    //  is always yes -- the CPU thread is already paced to one frame per
    //  frame. At Maximum it is yes at most once per `minIntervalUs`, and a
    //  yes is recorded as the start of the next interval.
    bool  ShouldPublish (bool isMaximumSpeed, int64_t minIntervalUs);

    //  Forgets both anchors, for a machine that is starting over.
    void  Reset ();

private:

    Now        m_now;
    bool       m_keyRepeatAnchored = false;   // a flag, not a sentinel time: zero is a legal reading
    TimePoint  m_lastKeyRepeat     = {};
    TimePoint  m_lastPublish       = {};
};
