#pragma once

#include "Pch.h"
#include "IVideoTiming.h"

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

class VideoTiming : public IVideoTiming
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

    // Phase 4 split-reset (FR-034 / FR-035, data-model.md line 326).
    // Both paths clear the cycle counter; PowerCycle accepts a Prng for
    // signature symmetry but does not consume any randomness — there is
    // no DRAM-shaped state on the timing model.
    void          SoftReset       () { m_cycleCounter = 0; }
    void          PowerCycle      (Prng & prng);

private:
    uint32_t      m_cycleCounter = 0;
};
