#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FollowHead
//
//  "Follow head" (FR-067): once the head has stayed on a quarter track for
//  kSettleMs, that track is the one to select; a track the head only passes
//  over is never selected. Turning it on selects the head's track at once,
//  and a selection made by hand turns it off.
//
////////////////////////////////////////////////////////////////////////////////

class FollowHead
{
public:
    static constexpr int64_t  kSettleMs = 300;

    //  The quarter track to select on turning it on, or -1 on turning it off.
    int   SetOn (bool isOn, int headQuarterTrack, int64_t nowMs);
    bool  IsOn () const { return m_isOn; }

    void  OnHandSelection () { m_isOn = false; }

    //  Once a frame: the quarter track to select now, or -1.
    int   Tick (int headQuarterTrack, int64_t nowMs);

private:
    bool     m_isOn      = false;
    int      m_candidate = -1;
    int64_t  m_since     = 0;
    int      m_selected  = -1;
};
