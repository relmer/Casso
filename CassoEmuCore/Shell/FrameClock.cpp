#include "Pch.h"

#include "Shell/FrameClock.h"
#include "Shell/FramePacing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FrameClock
//
////////////////////////////////////////////////////////////////////////////////

FrameClock::FrameClock()
    : m_now ([] { return std::chrono::steady_clock::now(); })
{
}


FrameClock::FrameClock (Now now)
    : m_now (std::move (now))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeKeyRepeatElapsedUs
//
//  The anchor advances by exactly what is reported, never by what was
//  capped away, so a long stall is charged once at the cap and the interval
//  starts fresh from the moment the machine was looked at again.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FrameClock::TakeKeyRepeatElapsedUs (uint32_t capUs)
{
    TimePoint  now     = m_now();
    int64_t    elapsed = 0;



    if (!m_keyRepeatAnchored)
    {
        m_keyRepeatAnchored = true;
        m_lastKeyRepeat     = now;
        return 0;
    }

    elapsed = std::chrono::duration_cast<std::chrono::microseconds> (now - m_lastKeyRepeat).count();

    if (elapsed <= 0)
    {
        return 0;
    }

    m_lastKeyRepeat = now;

    if (elapsed > static_cast<int64_t> (capUs))
    {
        elapsed = capUs;
    }

    return static_cast<uint32_t> (elapsed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldPublish
//
//  FramePacing decides from the elapsed microseconds; this is where the
//  microseconds come from.
//
////////////////////////////////////////////////////////////////////////////////

bool FrameClock::ShouldPublish (bool isMaximumSpeed, int64_t minIntervalUs)
{
    TimePoint  now     = m_now();
    int64_t    sinceUs = std::chrono::duration_cast<std::chrono::microseconds> (now - m_lastPublish).count();
    bool       publish = FramePacing::ShouldPublish (isMaximumSpeed, sinceUs, minIntervalUs);



    if (publish)
    {
        m_lastPublish = now;
    }

    return publish;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void FrameClock::Reset()
{
    m_keyRepeatAnchored = false;
    m_lastKeyRepeat     = {};
    m_lastPublish       = {};
}
