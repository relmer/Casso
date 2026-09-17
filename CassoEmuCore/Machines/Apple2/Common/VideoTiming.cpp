#include "Pch.h"

#include "Machines/Apple2/Common/VideoTiming.h"





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
