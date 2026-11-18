#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistorySpan
//
//  The machine's place in reverse execution's history: whether recording
//  is on, whether the machine is live (at the end of history) or behind
//  it, whether recording is paused at Maximum speed and
//  where it paused, and where history ended when the machine left live.
//  Pure state: each query takes the machine's position or cycle now.
//
////////////////////////////////////////////////////////////////////////////////

class HistorySpan
{
public:
    void  Start ();
    void  Stop  ();

    //  The user chose Maximum speed, or left it; false when nothing changed.
    bool  SetPaused (bool isPaused);

    //  The machine moves into history: history ends at position and cycle.
    //  False when it was already behind live.
    bool  Leave (uint64_t position, uint64_t cycle);

    //  The machine is at the end of history at position. While paused,
    //  recording stops there.
    void  BecomeLive (uint64_t position);

    //  While paused, recording stops at position again.
    void  NotePause (uint64_t position);

    //  A debugger edit or disk change while live, stored as a boundary before
    //  the next instruction; TakeEdit returns whether one was pending and
    //  clears it.
    void  MarkEdit () { m_isEditPending = true; }
    bool  TakeEdit ();

    //  The stretch the machine last ran an instruction in behind live.
    void  SetHere (uint64_t start, uint64_t end) { m_hereStart = start; m_hereEnd = end; }
    bool  IsHere  (uint64_t position) const      { return position >= m_hereStart && position < m_hereEnd; }

    bool      IsRecording        () const { return m_isRecording; }
    bool      IsLive             () const { return m_isLive; }
    bool      IsPaused           () const { return m_isPaused; }
    bool      IsInHistory        () const { return m_isRecording && !m_isLive; }
    uint64_t  GetPauseStart      () const { return m_pauseStart; }

    //  Live and paused, with the machine past where recording paused: the
    //  positions between are a gap.
    bool      IsPausedBefore     (uint64_t position) const { return m_isLive && m_isPaused && position > m_pauseStart; }

    //  The newest position history has: the machine's while live, where it
    //  left live otherwise. The cycle likewise.
    uint64_t  GetLiveEndPosition (uint64_t position) const { return m_isLive ? position : m_liveEndPosition; }
    uint64_t  GetLiveEndCycle    (uint64_t cycle) const    { return m_isLive ? cycle : m_liveEndCycle; }

    //  The newest position a replay can run to: the live end, except while
    //  live and paused, when it is where recording paused.
    uint64_t  GetRecordedEnd     (uint64_t position) const;

    //  Where history ended when the machine last left live.
    uint64_t  GetLeftAt          () const { return m_liveEndPosition; }
    uint64_t  GetLeftCycle       () const { return m_liveEndCycle; }

private:
    bool      m_isRecording     = false;
    bool      m_isLive          = true;
    bool      m_isPaused        = false;
    bool      m_isEditPending   = false;
    uint64_t  m_pauseStart      = 0;
    uint64_t  m_liveEndPosition = 0;
    uint64_t  m_liveEndCycle    = 0;
    uint64_t  m_hereStart       = 0;
    uint64_t  m_hereEnd         = 0;
};
