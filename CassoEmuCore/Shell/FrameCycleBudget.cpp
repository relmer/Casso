#include "Pch.h"

#include "Shell/FrameCycleBudget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FrameCycleBudget::GetTarget
//
//  The cycles from `totalCycles` to the next multiple of `nominalCycles`.
//
//  A pass runs whole instructions until it has spent its target, so the last
//  instruction usually carries it a few cycles past. Measuring the target from
//  the cycle count rather than handing every pass the nominal count takes that
//  overshoot off the next pass, which holds the long-run rate at exactly the
//  nominal cycles per pass. A fixed target dropped it: every frame ran long,
//  by about 64 cycles a second in total, and the beam at each frame boundary
//  crept forward against the host's pacing.
//
//  The same measurement puts a pass that ended short back on the frame grid.
//  A breakpoint, a pause or a debugger step stops the machine partway through
//  a frame, and the pass after it runs only the rest of that frame rather
//  than a whole one from wherever the machine stopped. So the pass boundary,
//  where a pause between passes takes effect, stays at the same point of the
//  frame however often the machine is stopped and started.
//
//  Double speed passes twice the frame, and the grid is its multiples.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FrameCycleBudget::GetTarget (
    uint32_t  nominalCycles,
    uint64_t  totalCycles)
{
    uint32_t  intoFrame = (nominalCycles != 0) ? (uint32_t) (totalCycles % nominalCycles) : 0;



    return (nominalCycles - intoFrame);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FrameCycleBudget::GetPauseTarget
//
//  The cycles from totalCycles to fraction of the way into its frame, or
//  zero when the machine is already past that point.
//
//  A pause lands in step with the request: the share of the host tick that had
//  gone by when it was made becomes the share of the frame the next pass runs
//  before it stops. The pass still stops on a whole instruction, so it lands
//  a few cycles past the point at most. The pass after the pause runs the rest
//  of the frame to the boundary, as GetTarget gives it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FrameCycleBudget::GetPauseTarget (
    uint32_t  nominalCycles,
    uint64_t  totalCycles,
    double    fraction)
{
    uint32_t  intoFrame = (nominalCycles != 0) ? (uint32_t) (totalCycles % nominalCycles) : 0;
    uint32_t  stopAt    = (uint32_t) (fraction * nominalCycles);



    return (stopAt > intoFrame) ? (stopAt - intoFrame) : 0;
}





