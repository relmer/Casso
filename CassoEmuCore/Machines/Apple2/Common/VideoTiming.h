#pragma once

#include "Pch.h"
#include "Core/IMachineState.h"
#include "Machines/Apple2/Common/IVideoTiming.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoTiming
//
//  NTSC //e video timing model. Tracks `m_cycleCounter` modulo
//  17,030 (= 65 cycles/scanline x 262 scanlines/frame). VBL window
//  spans scanlines 192..261 — i.e. cycle-in-frame >= 12,480. The
//  $C019 RDVBLBAR soft-switch read consumes IsInVblank() (inverted
//  at the read site: bit 7 = 1 during display, 0 during vblank).
//
//  Per FR-033 + audit §1.2 M (RDVBLBAR) + data-model.md §VideoTiming.
//
////////////////////////////////////////////////////////////////////////////////

class VideoTiming final : public IVideoTiming, public IMachineState
{
public:
    static constexpr uint32_t   kCyclesPerScanline   = 65;
    static constexpr uint32_t   kScanlinesPerFrame   = 262;
    static constexpr uint32_t   kCyclesPerFrame      = kCyclesPerScanline * kScanlinesPerFrame;
    static constexpr uint32_t   kVblankStartScanline = 192;
    static constexpr uint32_t   kVblankStartCycle    = kVblankStartScanline * kCyclesPerScanline;

                             VideoTiming () = default;
                             ~VideoTiming () override = default;

    void          Tick               (uint32_t cpuCycles) override;
    bool          IsInVblank         () const override { return m_cycleCounter >= kVblankStartCycle; }
    uint32_t      GetCycleInFrame    () const override { return m_cycleCounter; }
    uint32_t      GetCurrentScanline () const          { return m_cycleCounter / kCyclesPerScanline; }
    uint32_t      GetHorizontalPos   () const          { return m_cycleCounter % kCyclesPerScanline; }
    void          Reset              () override       { m_cycleCounter = 0; }

    // A /RESET reaches the 6502, not the video counters, so a soft reset
    // leaves the beam where it is and the counter in step with the CPU's
    // cycle count. Power-on zeroes both. PowerCycle accepts a Prng for
    // signature symmetry but consumes no randomness.
    void          SoftReset       () {}
    void          PowerCycle      (Prng & prng);

    // IMachineState: the cycle within the frame, from which the beam
    // position and VBL derive.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('V', 'T', 'M', ' ');
    static constexpr uint16_t  kStateVersion = 1;

private:
    uint32_t      m_cycleCounter = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoTiming::Tick
//
//  Advances the cycle-in-frame counter wrapped modulo 17,030. Every emulated
//  CPU cycle ticks the //e video circuit; EmuCpu::AddCycles fans the
//  per-instruction count into here so that $C019 readers see the correct
//  phase of the 262-line frame.
//
//  Defined here, and the class final, so EmuCpu::AddCycles -- which holds a
//  VideoTiming rather than the interface -- inlines it. It runs once per
//  instruction, and as a virtual call it cost about 2% of emulation speed.
//
////////////////////////////////////////////////////////////////////////////////

inline void VideoTiming::Tick (uint32_t cpuCycles)
{
    uint32_t    total = m_cycleCounter + cpuCycles;



    // The increment is tiny (<= one instruction's cycles) and m_cycleCounter
    // is always < kCyclesPerFrame, so the sum almost never crosses a frame
    // boundary. Take the cheap compare on the common path and only pay the
    // integer division on the ~once-per-frame wrap (the modulo still handles
    // any larger increment correctly).
    m_cycleCounter = (total < kCyclesPerFrame) ? total : (total % kCyclesPerFrame);
}
