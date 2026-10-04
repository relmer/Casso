#include "Pch.h"

#include "Machines/Apple2/Common/VideoScanner.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetHorizontalState
//
//  The horizontal counter's state on a cycle of the line: $00 on the first,
//  then $40 through $7F (Sather, Understanding the Apple IIe, chapter 5).
//
////////////////////////////////////////////////////////////////////////////////

uint32_t VideoScanner::GetHorizontalState (uint32_t cycleInLine)
{
    constexpr uint32_t  kFirstCountingState = 0x40;



    return (cycleInLine == 0) ? 0 : kFirstCountingState + cycleInLine - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVerticalBits
//
//  The low eight bits of the vertical counter on a line of the frame. The
//  counter runs $100-$1FF, then $0FA-$0FF, so the frame's last six lines
//  repeat the low bits $FA-$FF.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t VideoScanner::GetVerticalBits (uint32_t line)
{
    constexpr uint32_t  kUpperStates = 0x100;
    constexpr uint32_t  kLowerStart  = 0xFA;



    return (line < kUpperStates) ? line : kLowerStart + (line - kUpperStates);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScanAddress
//
//  The address the scanner fetches on a cycle of the frame, formed as the
//  class comment lays out from Sather's figure 5.8.
//
////////////////////////////////////////////////////////////////////////////////

Word VideoScanner::GetScanAddress (uint32_t cycleInFrame, const VideoScanMode & mode, bool isApple2OrPlus)
{
    constexpr uint32_t  kSumConstant    = 0xD;   // the 1101 Sather's adder adds
    constexpr uint32_t  kNibbleMask     = 0xF;
    constexpr uint32_t  kThreeBitMask   = 0x7;
    constexpr uint32_t  kRowGroupWeight = 5;   // V4V3V4V3 is five times V4V3
    constexpr uint32_t  kMixedTextBits  = 0xA0;   // V2 and V4
    constexpr Word      kTextPage1      = 0x0400;
    constexpr Word      kTextPage2      = 0x0800;
    constexpr Word      kHiresPage1     = 0x2000;
    constexpr Word      kHiresPage2     = 0x4000;
    constexpr Word      kApple2HblBit   = 0x1000;   // A12 in blanking, ][ and ][+ text and lo-res
    constexpr uint32_t  kRowGroupMask   = 0x3;
    constexpr uint32_t  kRowGroupShift  = 6;   // V3 is bit 6 of the line
    constexpr uint32_t  kFieldShift     = 3;   // V0 and H3 are bit 3 of their counters, A3 of the address
    constexpr uint32_t  kRowShift       = 7;   // V0-V2 are A7-A9
    constexpr uint32_t  kLineShift      = 10;   // VA-VC are A10-A12 in hi-res
    uint32_t            line            = (cycleInFrame / kCyclesPerLine) % kLinesPerFrame;
    uint32_t            h               = GetHorizontalState (cycleInFrame % kCyclesPerLine);
    uint32_t            v               = GetVerticalBits (line);
    uint32_t            rowGroup        = (v >> kRowGroupShift) & kRowGroupMask;   // V4V3
    uint32_t            rowInGroup      = (v >> kFieldShift) & kThreeBitMask;   // V0-V2
    uint32_t            lineInRow       = v & kThreeBitMask;   // VA-VC
    uint32_t            sum             = (kSumConstant + ((h >> kFieldShift) & kThreeBitMask) + kRowGroupWeight * rowGroup) & kNibbleMask;
    Word                low             = static_cast<Word> ((rowInGroup << kRowShift) | (sum << kFieldShift) | (h & kThreeBitMask));
    bool                isText          = !mode.graphics || (mode.mixed && (v & kMixedTextBits) == kMixedTextBits);
    Word                address         = 0;



    if (!isText && mode.hires)
    {
        address = static_cast<Word> ((mode.page2 ? kHiresPage2 : kHiresPage1) | (lineInRow << kLineShift) | low);
    }
    else
    {
        address = static_cast<Word> ((mode.page2 ? kTextPage2 : kTextPage1) | low);

        if (isApple2OrPlus && h < kFirstDisplayedState)
        {
            address |= kApple2HblBit;
        }
    }

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSwitches
//
////////////////////////////////////////////////////////////////////////////////

void VideoScanner::SetSwitches (const AppleSoftSwitchBank * switches, const Apple2eSoftSwitchBank * iieSwitches)
{
    m_switches    = switches;
    m_iieSwitches = iieSwitches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAccessCycleInFrame
//
//  The cycle of the frame the current bus access falls on: VideoTiming's
//  count, which stands at the start of the instruction, plus how far into
//  the instruction the CPU's bus-cycle estimate puts the access. A read
//  from outside an instruction finds the estimate behind the total and
//  takes no offset.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t VideoScanner::GetAccessCycleInFrame() const
{
    uint64_t  offset = 0;
    uint32_t  start  = 0;



    if (m_timing != nullptr)
    {
        start = m_timing->GetCycleInFrame();
    }

    if (m_totalCycles != nullptr && m_busCycle != nullptr && *m_busCycle > *m_totalCycles)
    {
        offset = *m_busCycle - *m_totalCycles;
    }

    return static_cast<uint32_t> ((start + offset) % VideoTiming::kCyclesPerFrame);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMode
//
//  The display switches as the scanner sees them. With 80STORE on, PAGE2
//  selects aux memory rather than the displayed page.
//
////////////////////////////////////////////////////////////////////////////////

VideoScanMode VideoScanner::GetMode() const
{
    VideoScanMode  mode;



    if (m_switches != nullptr)
    {
        mode.graphics = m_switches->IsGraphicsMode();
        mode.mixed    = m_switches->IsMixedMode();
        mode.page2    = m_switches->IsPage2();
        mode.hires    = m_switches->IsHiresMode();
    }

    if (m_iieSwitches != nullptr && m_iieSwitches->Is80Store())
    {
        mode.page2 = false;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFloatingBusByte
//
//  The main-memory byte the scanner fetches on this access's cycle.
//
////////////////////////////////////////////////////////////////////////////////

Byte VideoScanner::GetFloatingBusByte()
{
    Word  address = 0;
    Byte  value   = 0;



    if (m_mainRam != nullptr)
    {
        address = GetScanAddress (GetAccessCycleInFrame(), GetMode(), m_isApple2OrPlus);
        value   = m_mainRam[address];
    }

    return value;
}
