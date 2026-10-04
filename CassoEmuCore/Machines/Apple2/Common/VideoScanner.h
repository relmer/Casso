#pragma once

#include "Pch.h"
#include "Core/IFloatingBusSource.h"

class AppleSoftSwitchBank;
class Apple2eSoftSwitchBank;
class VideoTiming;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScanMode
//
//  The display switches the scanner's address depends on. `page2` is the
//  displayed page: with 80STORE on, PAGE2 selects aux memory for the CPU and
//  the display stays on page 1, so the caller passes false then.
//
////////////////////////////////////////////////////////////////////////////////

struct VideoScanMode
{
    bool  graphics = false;
    bool  mixed    = false;
    bool  page2    = false;
    bool  hires    = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScanner
//
//  The address the video scanner fetches on each cycle of the frame, and the
//  floating bus that follows from it.
//
//  Written from the documented counter and address sequence, not from any
//  other emulator: Jim Sather, "Understanding the Apple IIe" (1985), chapter
//  5, "The Video Display", and "Understanding the Apple II" (1983), chapter
//  3, for the ][ and ][+, with the screen memory maps of the Apple IIe
//  Technical Reference Manual (1985), chapter 2.
//
//  The horizontal counter takes 65 states per line: $00, then $40 to $7F.
//  The 25 states $00 and $40-$57 are horizontal blanking and $58-$7F are the
//  40 displayed columns. A line begins at $00, so the vertical counter
//  advances, and vertical blanking begins, at the line boundary that
//  VideoTiming already counts from. The vertical counter takes 262 states:
//  $100 to $1FF, then $0FA to $0FF. Lines $100-$1BF are displayed; the low
//  eight bits are the line within the frame. Named as Sather does, those
//  bits are VA VB VC (the line within a character row) and V0-V4.
//
//  The address is formed as Sather's figure 5.8 gives it:
//
//    A0-A2    H0-H2
//    A3-A6    the low four bits of  H5H4H3 + V4V3V4V3 + 1101
//    A7-A9    V0-V2
//    text and lo-res:  A10-A11 the page ($0400 or $0800)
//    hi-res:           A10-A12 VA-VC, A13-A14 the page ($2000 or $4000)
//
//  On the displayed columns the sum reproduces the manual's memory map
//  (row r at base + 128 * (r mod 8) + 40 * (r div 8)); in blanking it
//  carries on through the same adder, which is why horizontal blanking
//  fetches the screen holes at offsets $68-$7F of the row's 128 bytes.
//
//  Mixed mode shows text where V2 and V4 are both set, lines 160-191 of the
//  display (and 224-255, inside vertical blanking). On the ][ and ][+,
//  Sather's "Understanding the Apple II" gives A12 as set during horizontal
//  blanking in text and lo-res, which the //e's IOU no longer does.
//
//  80-column text and double hi-res fetch the same address from main and
//  aux memory together; the byte left on the data bus is main memory's,
//  which is what this returns.
//
//  The byte a read sees is the one fetched in the same cycle. A 6502 makes
//  its access in phase 2 and the scanner fetched in phase 1 just before it,
//  so a soft switch the read itself flips affects the next cycle, not this
//  one. The cycle comes from VideoTiming, which counts at instruction
//  boundaries, plus the CPU's in-instruction bus-cycle estimate.
//
////////////////////////////////////////////////////////////////////////////////

class VideoScanner : public IFloatingBusSource
{
public:
    static constexpr uint32_t  kCyclesPerLine       = 65;
    static constexpr uint32_t  kLinesPerFrame       = 262;
    static constexpr uint32_t  kFirstDisplayedState = 0x58;

    static Word  GetScanAddress (uint32_t cycleInFrame, const VideoScanMode & mode, bool isApple2OrPlus);

    void  SetTiming        (const VideoTiming * timing)                              { m_timing = timing; }
    void  SetCycleCounters (const uint64_t * totalCycles, const uint64_t * busCycle) { m_totalCycles = totalCycles; m_busCycle = busCycle; }
    void  SetSwitches      (const AppleSoftSwitchBank * switches, const Apple2eSoftSwitchBank * iieSwitches);
    void  SetMainRam       (const Byte * mainRam)                                    { m_mainRam = mainRam; }
    void  SetApple2OrPlus  (bool isApple2OrPlus)                                     { m_isApple2OrPlus = isApple2OrPlus; }

    uint32_t       GetAccessCycleInFrame () const;
    VideoScanMode  GetMode               () const;
    Byte           GetFloatingBusByte    () override;

private:
    static uint32_t  GetHorizontalState (uint32_t cycleInLine);
    static uint32_t  GetVerticalBits    (uint32_t line);

    const VideoTiming            * m_timing         = nullptr;
    const uint64_t               * m_totalCycles    = nullptr;
    const uint64_t               * m_busCycle       = nullptr;
    const AppleSoftSwitchBank    * m_switches       = nullptr;
    const Apple2eSoftSwitchBank  * m_iieSwitches    = nullptr;
    const Byte                   * m_mainRam        = nullptr;
    bool                           m_isApple2OrPlus = false;
};
