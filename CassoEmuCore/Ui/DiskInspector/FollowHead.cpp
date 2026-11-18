#include "Pch.h"

#include "Ui/DiskInspector/FollowHead.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FollowHead::SetOn
//
////////////////////////////////////////////////////////////////////////////////

int FollowHead::SetOn (bool isOn, int headQuarterTrack, int64_t nowMs)
{
    m_isOn      = isOn;
    m_candidate = headQuarterTrack;
    m_since     = nowMs;
    m_selected  = isOn ? headQuarterTrack : -1;

    return m_selected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FollowHead::Tick
//
//  A new track under the head starts the wait over; one that stays for the
//  whole wait is selected once.
//
////////////////////////////////////////////////////////////////////////////////

int FollowHead::Tick (int headQuarterTrack, int64_t nowMs)
{
    int  select = -1;



    if (m_isOn && headQuarterTrack != m_candidate)
    {
        m_candidate = headQuarterTrack;
        m_since     = nowMs;
    }

    if (m_isOn && m_candidate != m_selected && nowMs - m_since >= kSettleMs)
    {
        m_selected = m_candidate;
        select     = m_selected;
    }

    return select;
}
