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

    m_cycleCounter = total % kCyclesPerFrame;
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

    SoftReset ();
}
