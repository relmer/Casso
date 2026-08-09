#include "Pch.h"

#include "VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advances the cycle-in-frame counter wrapped modulo 17,030. Per FR-033
//  / audit §1.2, every emulated CPU cycle ticks the //e video circuit;
//  EmuCpu::AddCycles fans the per-instruction count into here so that
//  $C019 readers see the correct phase of the 262-line frame.
//
////////////////////////////////////////////////////////////////////////////////

void VideoTiming::Tick (uint32_t cpuCycles)
{
    uint32_t    total = m_cycleCounter + cpuCycles;



    // Called once per instruction, so the increment is tiny (<= one
    // instruction's cycles) and m_cycleCounter is always < kCyclesPerFrame --
    // the sum almost never crosses a frame boundary. Take the cheap compare on
    // the common path and only pay the integer division on the ~once-per-frame
    // wrap (the modulo still handles any larger increment correctly).
    m_cycleCounter = (total < kCyclesPerFrame) ? total : (total % kCyclesPerFrame);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Same effect as SoftReset — the timing model has no DRAM-shaped state.
//
////////////////////////////////////////////////////////////////////////////////

void VideoTiming::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    SoftReset();
}
