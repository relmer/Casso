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
//  A paused machine keeps sending snapshots, so a stop is recognized by its
//  values differing from the last paused ones, not by the snapshot arriving.
//  A step, a register write and a run that ends at a breakpoint all read as
//  one.
//
////////////////////////////////////////////////////////////////////////////////

class StopChanges
{
public:
    //  Values by key, as "R:A" or "W:3", from one snapshot.
    using Values = std::map<std::string, std::string>;

    void  Update    (bool isPaused, Values values);
    bool  IsChanged (const std::string & key) const;

private:
    Values  m_stop;
    Values  m_previous;
    bool    m_isPaused = false;
};
