#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ITapeDeckPort
//
//  What the cassette port on the motherboard sees of the recorder plugged into
//  it: the level on the input line at a bus cycle, and each toggle of the
//  output line.
//
////////////////////////////////////////////////////////////////////////////////

class ITapeDeckPort
{
public:
    virtual ~ITapeDeckPort () = default;

    virtual bool  ReadInputLevel (uint64_t busCycle) = 0;
    virtual void  OnOutputToggle (uint64_t busCycle) = 0;
};
