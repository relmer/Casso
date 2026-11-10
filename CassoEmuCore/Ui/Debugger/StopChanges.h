#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  StopChanges
//
//  Which shown values changed since the machine last stopped, as Visual
//  Studio marks them: a stop compares with the stop before it, and the marks
//  hold until the next one. While the machine runs nothing is marked, since
//  everything would be.
//
//  Snapshots arrive while the machine is paused too, so a stop is recognized
//  by the CPU's cycles differing from the last paused snapshot's, not by the
//  snapshot arriving. A step, a run that ends at a breakpoint and a seek
//  through history each move the cycles. An edit at a stop, to a register or
//  to memory, doesn't: its values replace the stop's, and are compared with
//  the stop before it.
//
////////////////////////////////////////////////////////////////////////////////

class StopChanges
{
public:
    //  Values by key, as "R:A" or "W:3", from one snapshot.
    using Values = std::map<std::string, std::string>;

    void         Update    (bool isPaused, uint64_t cycles, Values values);
    bool         IsChanged (const std::string & key) const;

    //  How far a whole-number value moved since the previous stop, as "+6"
    //  or "-6"; empty while running, with no previous stop, or no move.
    std::string  GetDelta  (const std::string & key) const;

    //  The bits of a hexadecimal value that differ from the previous stop;
    //  0 while running, with no previous stop, or no change.
    uint32_t     GetChangedBits (const std::string & key) const;

private:
    Values    m_stop;
    Values    m_previous;
    bool      m_isPaused   = false;
    bool      m_hasStop    = false;
    uint64_t  m_stopCycles = 0;
};
