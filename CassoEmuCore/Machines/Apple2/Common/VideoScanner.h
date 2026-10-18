#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScanMode
//
//  The display switches that select which memory the video scanner reads:
//  TEXT/GRAPHICS, MIXED, PAGE2, HIRES and the //e's 80STORE.
//
////////////////////////////////////////////////////////////////////////////////

struct VideoScanMode
{
    bool  graphics = false;
    bool  mixed    = false;
    bool  page2    = false;
    bool  hires    = false;
    bool  store80  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScanner
//
//  The address the video scanner reads in a given cycle of the frame, from
//  Sather's description of the scanner (Understanding the Apple IIe, chapters
//  3 and 5). On a real machine, a read of an address with no device behind
//  it returns the byte the scanner fetched, which the data bus retains.
//
//  The horizontal counter has 65 states, H5-H0 reading 0, 0, 1 ... 63; the
//  vertical counter has 262, $FA-$1FF, with the 192 displayed lines at
//  $100-$1BF. Cycle 0 of the frame is the first cycle of displayed line 0,
//  as VideoTiming counts it.
//
//  Address bits, low to high:
//      A0-A2    H0-H2
//      A3-A6    (H5 H4 H3) + 1101 + (V4 V3 V4 V3), four bits
//      A7-A9    V0-V2
//      TEXT and LORES: page 1 at $0400, page 2 at $0800
//      HIRES:   A10-A12 VA-VC, page 1 at $2000, page 2 at $4000
//
//  PAGE2 selects page 2 only with 80STORE off. In MIXED, the lines with V4
//  and V2 both set are scanned as text.
//
////////////////////////////////////////////////////////////////////////////////

class VideoScanner
{
public:
    static constexpr uint32_t  kCyclesPerScanline = 65;
    static constexpr uint32_t  kScanlinesPerFrame = 262;

    static Word  GetScannedAddress (uint32_t cycleInFrame, const VideoScanMode & mode);
};
