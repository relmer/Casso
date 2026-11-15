#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "Machines/Apple2/Common/TrackLayoutWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures
//
//  A disk's track layouts from Measure track on each track, in order from
//  track 0, for TrackLayoutWriter. A turn's bits are the drive's revolution
//  in cycles over 4, a bit's cycles on an emulated drive, so a nibble on the
//  built WOZ takes the share of a turn it takes on the disk.
//
//  Track 0's sector 0 begins at bit 0. Each later track's begins kStepBits
//  after the track before's, the bits stepto's time covers, plus the bits
//  of the nibbles counted from the step, at the track's bits per nibble.
//  The program reads past a nibble or two after each address field it
//  compares, so kTurnNibbles is added to the nibbles it counts in a turn.
//  Both are calibrated on an emulated //c so that a WOZ TrackLayoutWriter
//  builds from a disk's measures measures as the disk did. Against a raw
//  capture of Wizardry I's boot disk, the offsets are within 8 nibbles of
//  the capture's, around a constant 11. Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class TrackMeasures
{
public:
    static std::vector<TrackLayout>  ToLayouts (const std::vector<CassoLinkHost::TrackMeasure> & measures, int bitsPerTurn);

    //  A turn's bits on a drive with this revolution; kDefaultBits for none.
    static int  GetBitsPerTurn (double revolutionMs);

    static constexpr long long  kStepBits     = 12630;
    static constexpr int        kTurnNibbles  = 4;
    static constexpr double     kCyclesPerMs  = 1020.484;     // the Apple's clock
    static constexpr int        kCyclesPerBit = 4;
};
