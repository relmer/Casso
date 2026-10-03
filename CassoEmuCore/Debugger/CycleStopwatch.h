#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatch
//
//  The counters behind STOPWATCH: armed with a start and a stop address, it
//  starts timing when an instruction at the start address is about to run
//  and records a lap when one at the stop address is, the lap being the
//  cycles between the two. A start address reached while timing is ignored,
//  so a routine that calls itself is timed from its outermost entry. With
//  the two addresses the same, each lap is the time from one arrival to the
//  next.
//
//  A stopwatch that is not armed ignores every instruction it is handed.
//
////////////////////////////////////////////////////////////////////////////////

class CycleStopwatch
{
public:
    void      Arm           (Word start, Word stop);
    void      Disarm        ();
    void      Reset         ();
    void      OnInstruction (Word pc, uint64_t cycles);

    bool      IsArmed       () const { return m_isArmed; }
    bool      IsTiming      () const { return m_isTiming; }
    Word      GetStart      () const { return m_start; }
    Word      GetStop       () const { return m_stop; }
    uint64_t  GetLapCount   () const { return m_laps; }
    uint64_t  GetLastLap    () const { return m_last; }
    uint64_t  GetShortest   () const { return m_shortest; }
    uint64_t  GetLongest    () const { return m_longest; }
    uint64_t  GetTotal      () const { return m_total; }

private:
    Word      m_start      = 0;
    Word      m_stop       = 0;
    uint64_t  m_startCycle = 0;
    uint64_t  m_laps       = 0;
    uint64_t  m_last       = 0;
    uint64_t  m_shortest   = 0;
    uint64_t  m_longest    = 0;
    uint64_t  m_total      = 0;
    bool      m_isArmed    = false;
    bool      m_isTiming   = false;
};
