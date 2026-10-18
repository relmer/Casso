#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FramedNibble / RandomRegion / FramedTrack
//
//  One track record as the drive's read latch frames it, one turn from the
//  index. A nibble starts at the cell of its leading 1 and takes eight cells;
//  the zero cells after it, before the next nibble starts, are its extra-zero
//  count (two after a 10-cell sync nibble). A random-bit region is a stretch
//  where the drive's read amplifier has no transition to hold on to and
//  produces random bits; it has no nibbles.
//
//  On a flux track every cell also has its recorded time: the interval the
//  cell lies in, divided by the number of cells the drive reads in it.
//
////////////////////////////////////////////////////////////////////////////////

struct FramedNibble
{
    Byte      value          = 0;
    uint32_t  startCell      = 0;
    uint16_t  extraZeroCells = 0;
    bool      isNoise        = false;
};


struct RandomRegion
{
    uint32_t  startCell = 0;
    uint32_t  cellCount = 0;
};


struct FramedTrack
{
    bool                  isFlux    = false;
    uint32_t              cellCount = 0;
    vector<Byte>          cells;
    vector<Byte>          isRandomCell;
    vector<FramedNibble>  nibbles;
    vector<RandomRegion>  randomRegions;
    vector<double>        cellTicks;
    vector<uint64_t>      transitionTicks;
    double                turnTicks = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer
//
//  Frames a track copy as Casso's Disk II reads it: the same cell for flux,
//  the same random-bit window, and the latch's rule that a nibble is complete
//  when its high bit sets. Pure; the only state is the copy.
//
////////////////////////////////////////////////////////////////////////////////

class LatchFramer
{
public:
    static constexpr int   kNibbleCells = 8;

    static void  Frame          (const TrackCopy & copy, FramedTrack & outTrack);
    static void  FrameCells     (FramedTrack & inOutTrack);
    static bool  IsNoiseNibble  (Byte value);

private:
    static void  UnpackBitCells  (const TrackCopy & copy, FramedTrack & outTrack);
    static void  DecodeFluxCells (const TrackCopy & copy, FramedTrack & outTrack);
    static void  MarkRandomBits  (FramedTrack & inOutTrack);
    static void  MarkRandomFlux  (FramedTrack & inOutTrack);
    static void  CollectRegions  (FramedTrack & inOutTrack);
};
