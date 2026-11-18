#include "Pch.h"

#include "Debugger/BeamBreakMonitor.h"
#include "Debugger/IDebugTarget.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::SetVideoBreak
//
////////////////////////////////////////////////////////////////////////////////

void BeamBreakMonitor::SetVideoBreak (uint32_t first, uint32_t last)
{
    uint32_t  scanline = m_target.GetVideoPosition().scanline;



    //  A beam already on the range's first line has not entered it, so the
    //  break waits for the beam to come round again.
    m_videoBreak = VideoBreak { first, last, scanline };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::GetVideoBreak
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<uint32_t, uint32_t>> BeamBreakMonitor::GetVideoBreak() const
{
    std::optional<std::pair<uint32_t, uint32_t>>  range;



    if (m_videoBreak.has_value())
    {
        range = std::pair<uint32_t, uint32_t> (m_videoBreak->first, m_videoBreak->last);
    }

    return range;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::SetBeamBreak
//
//  A beam already at the target has not reached it: the break waits for the
//  beam to come round again, which is what makes FRAME run a whole frame.
//  FRAME's break, for one run, is in m_frameBreak, separate from a BPBEAM's
//  in m_beamBreak, so a BPBEAM set before FRAME stops the frame run too.
//
////////////////////////////////////////////////////////////////////////////////

void BeamBreakMonitor::SetBeamBreak (uint32_t cycleInFrame, uint32_t passes, bool isForOneRun)
{
    (isForOneRun ? m_frameBreak : m_beamBreak)       = BeamBreak { cycleInFrame, std::max (passes, 1u), GetBeamCycle(), isForOneRun };
    (isForOneRun ? m_frameBreakHit : m_beamBreakHit) = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::GetBeamBreak
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint32_t> BeamBreakMonitor::GetBeamBreak() const
{
    return (m_beamBreak.has_value() && !m_beamBreak->isOneRun) ? std::optional<uint32_t> (m_beamBreak->target) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::ClearBeamBreak
//
////////////////////////////////////////////////////////////////////////////////

void BeamBreakMonitor::ClearBeamBreak (bool isForOneRun)
{
    (isForOneRun ? m_frameBreak : m_beamBreak).reset();
    (isForOneRun ? m_frameBreakHit : m_beamBreakHit) = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::Poll
//
//  Both beam breaks are tested, so each follows the beam on every
//  instruction.
//
////////////////////////////////////////////////////////////////////////////////

bool BeamBreakMonitor::Poll()
{
    if (HasEnteredVideoBreak())
    {
        m_videoBreakHit = true;
        return true;
    }

    m_beamBreakHit  = HasReachedBeamBreak (m_beamBreak);
    m_frameBreakHit = HasReachedBeamBreak (m_frameBreak);

    return m_beamBreakHit || m_frameBreakHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::OnStopped
//
////////////////////////////////////////////////////////////////////////////////

void BeamBreakMonitor::OnStopped()
{
    if (m_videoBreakHit)
    {
        m_videoBreak.reset();
        m_videoBreakHit = false;
    }

    if (m_beamBreakHit)
    {
        m_beamBreak.reset();
        m_beamBreakHit = false;
    }

    m_frameBreak.reset();
    m_frameBreakHit = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::ClearAll
//
////////////////////////////////////////////////////////////////////////////////

void BeamBreakMonitor::ClearAll()
{
    m_videoBreak.reset();
    m_videoBreakHit = false;
    m_beamBreak.reset();
    m_beamBreakHit  = false;
    m_frameBreak.reset();
    m_frameBreakHit = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::HasEnteredVideoBreak
//
//  True only on the instruction at which the beam moves onto the range's
//  first scanline, not on every instruction while it stays inside. The beam
//  runs one way and an instruction is far shorter than a scanline, so every
//  entry passes that line, even for a range that covers the whole frame.
//
////////////////////////////////////////////////////////////////////////////////

bool BeamBreakMonitor::HasEnteredVideoBreak()
{
    uint32_t  scanline = 0;
    uint32_t  previous = 0;



    if (!m_videoBreak.has_value())
    {
        return false;
    }

    scanline                   = m_target.GetVideoPosition().scanline;
    previous                   = m_videoBreak->lastScanline;
    m_videoBreak->lastScanline = scanline;

    return scanline == m_videoBreak->first && previous != m_videoBreak->first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::GetBeamCycle
//
//  The beam's place as the cycle within the frame.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BeamBreakMonitor::GetBeamCycle() const
{
    VideoPosition  position = m_target.GetVideoPosition();



    return position.scanline * VideoTiming::kCyclesPerScanline + position.cycleInLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor::HasReachedBeamBreak
//
//  True on the instruction at which the beam has reached or passed the
//  target since the instruction before. An instruction moves the beam several
//  cycles, so it can step over the exact cycle; passing it counts. The end of
//  a frame wraps the beam to zero, and a wrap passes every cycle after where
//  the beam was and every one up to where it is now.
//
////////////////////////////////////////////////////////////////////////////////

bool BeamBreakMonitor::HasReachedBeamBreak (std::optional<BeamBreak> & beamBreak)
{
    uint32_t  now      = 0;
    uint32_t  previous = 0;
    uint32_t  target   = 0;
    bool      isWrap   = false;
    bool      reached  = false;



    if (!beamBreak.has_value())
    {
        return false;
    }

    now                  = GetBeamCycle();
    previous             = beamBreak->lastCycle;
    target               = beamBreak->target;
    isWrap               = now < previous;
    beamBreak->lastCycle = now;

    if (now == previous)
    {
        return false;
    }

    reached = isWrap ? (target > previous || target <= now)
                     : (target > previous && target <= now);

    if (!reached)
    {
        return false;
    }

    beamBreak->passes--;
    return beamBreak->passes == 0;
}
