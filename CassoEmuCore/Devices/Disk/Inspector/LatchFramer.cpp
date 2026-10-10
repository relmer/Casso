#include "Pch.h"

#include "Devices/Disk/Inspector/LatchFramer.h"
#include "Devices/Disk/FluxTrack.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::Frame
//
//  Cells first (unpacked bits, or flux decoded at the drive's cell), then the
//  random-bit cells by the drive's own rule for that kind of track, then the
//  nibbles.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::Frame (const TrackCopy & copy, FramedTrack & outTrack)
{
    outTrack = FramedTrack();

    if (copy.kind == TrackKind::Flux)
    {
        DecodeFluxCells (copy, outTrack);
        MarkRandomFlux (outTrack);
    }
    else
    {
        UnpackBitCells (copy, outTrack);
        MarkRandomBits (outTrack);
    }

    FrameCells (outTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::UnpackBitCells
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::UnpackBitCells (const TrackCopy & copy, FramedTrack & outTrack)
{
    static constexpr int  kBitsPerByte = 8;



    size_t  count = std::min (copy.bitCount, copy.bits.size() * kBitsPerByte);
    size_t  i     = 0;



    outTrack.cellCount = static_cast<uint32_t> (count);
    outTrack.cells.assign (count, 0);

    for (i = 0; i < count; i++)
    {
        outTrack.cells[i] = static_cast<Byte> ((copy.bits[i / kBitsPerByte] >> ((kBitsPerByte - 1) - (i % kBitsPerByte))) & 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::DecodeFluxCells
//
//  Each interval holds round (interval / cell) cells, at least one, at the
//  drive's own cell; the last of them is the transition, a 1. Each cell's
//  recorded time is the interval divided by its cell count. This is the rule
//  FluxBitView uses for sector reads, so the two agree on every cell.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::DecodeFluxCells (const TrackCopy & copy, FramedTrack & outTrack)
{
    FluxTrack         track;
    vector<uint64_t>  ticks;
    uint64_t          prev  = 0;
    uint64_t          gap   = 0;
    uint64_t          cells = 0;
    uint64_t          c     = 0;
    double            each  = 0;



    outTrack.isFlux = true;

    track.Assign (copy.fluxBytes);
    track.GetTransitionTicks (ticks);

    for (uint64_t tick : ticks)
    {
        gap   = tick - prev;
        cells = (gap * FluxTrack::kCellDenominator + FluxTrack::kCellNumerator / 2) / FluxTrack::kCellNumerator;
        cells = std::max<uint64_t> (cells, 1);
        each  = static_cast<double> (gap) / static_cast<double> (cells);

        for (c = 1; c <= cells; c++)
        {
            outTrack.cells.push_back ((c == cells) ? 1 : 0);
            outTrack.cellTicks.push_back (each);
        }

        prev = tick;
    }

    outTrack.transitionTicks = ticks;
    outTrack.turnTicks       = static_cast<double> (prev);
    outTrack.cellCount       = static_cast<uint32_t> (outTrack.cells.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::MarkRandomBits
//
//  On a bit track the drive's read amplifier looks at a window of the last
//  kHeadWindowCells cells. When they are all zero it puts out a random bit in
//  place of the cell it would have delivered, which is the one before the
//  newest. So cell k is random when cells k-2 through k+1 are all zero.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::MarkRandomBits (FramedTrack & inOutTrack)
{
    static constexpr int  kWindow = Disk2NibbleEngine::kHeadWindowCells;
    static constexpr int  kBefore = kWindow - 2;



    uint32_t  n      = inOutTrack.cellCount;
    uint32_t  k      = 0;
    int       w      = 0;
    bool      isZero = false;



    inOutTrack.isRandomCell.assign (n, 0);

    for (k = 0; k < n && n >= static_cast<uint32_t> (kWindow); k++)
    {
        isZero = true;

        for (w = 0; w < kWindow && isZero; w++)
        {
            isZero = inOutTrack.cells[(k + n - kBefore + w) % n] == 0;
        }

        inOutTrack.isRandomCell[k] = isZero ? 1 : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::MarkRandomFlux
//
//  On a flux track the drive puts out random bits once more than
//  kHeadWindowCells cells' worth of time has passed since the last
//  transition, sampled in the middle of each cell. A cell holding a
//  transition is never random.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::MarkRandomFlux (FramedTrack & inOutTrack)
{
    static constexpr double  kCell   = static_cast<double> (FluxTrack::kCellNumerator) / static_cast<double> (FluxTrack::kCellDenominator);
    static constexpr double  kWindow = Disk2NibbleEngine::kHeadWindowCells * kCell;
    static constexpr double  kHalf   = 0.5;



    uint32_t  n       = inOutTrack.cellCount;
    uint32_t  k       = 0;
    double    elapsed = 0;



    inOutTrack.isRandomCell.assign (n, 0);

    for (k = 0; k < n; k++)
    {
        if (inOutTrack.cells[k] != 0)
        {
            elapsed = 0;
            continue;
        }

        inOutTrack.isRandomCell[k] = (elapsed + kHalf * inOutTrack.cellTicks[k] > kWindow) ? 1 : 0;
        elapsed += inOutTrack.cellTicks[k];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::FrameCells
//
//  Runs the latch over the track twice. The first turn only brings the latch
//  to the state it has at the index on a disk that has been spinning; the
//  nibbles completed in the second turn are kept, so a nibble that spans the
//  index is read whole and starts before cell 0 (its start cell wraps).
//
//  Zero cells while the latch is empty belong to the nibble before them as its
//  extra zeros. A random cell empties the latch, and framing resumes at the
//  first 1 after the region.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::FrameCells (FramedTrack & inOutTrack)
{
    static constexpr Byte  kHighBit = 0x80;
    static constexpr int   kTurns   = 2;



    uint32_t  n            = inOutTrack.cellCount;
    uint64_t  step         = 0;
    uint32_t  k            = 0;
    uint32_t  start        = 0;
    uint16_t  leadingZeros = 0;
    Byte      value        = 0;
    bool      isKeeping    = false;
    bool      hasNibble    = false;



    inOutTrack.nibbles.clear();

    for (step = 0; step < static_cast<uint64_t> (n) * kTurns; step++)
    {
        k         = static_cast<uint32_t> (step % n);
        isKeeping = step >= n;

        if (inOutTrack.isRandomCell[k] != 0)
        {
            value = 0;
            continue;
        }

        if (value == 0 && inOutTrack.cells[k] == 0)
        {
            if (isKeeping && hasNibble)
            {
                inOutTrack.nibbles.back().extraZeroCells++;
            }
            else if (isKeeping)
            {
                leadingZeros++;
            }

            continue;
        }

        if (value == 0)
        {
            start = k;
        }

        value = static_cast<Byte> ((value << 1) | inOutTrack.cells[k]);

        if ((value & kHighBit) != 0)
        {
            if (isKeeping)
            {
                inOutTrack.nibbles.push_back ({ value, start, 0, IsNoiseNibble (value) });
                hasNibble = true;
            }

            value = 0;
        }
    }

    if (hasNibble)
    {
        inOutTrack.nibbles.back().extraZeroCells = static_cast<uint16_t> (inOutTrack.nibbles.back().extraZeroCells + leadingZeros);
    }

    CollectRegions (inOutTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::IsNoiseNibble
//
//  Three zero cells in a row inside a nibble's eight: no valid disk byte has
//  them, so the latch framed something the drive could not have written as a
//  byte.
//
////////////////////////////////////////////////////////////////////////////////

bool LatchFramer::IsNoiseNibble (Byte value)
{
    static constexpr int  kRun       = 3;
    static constexpr int  kRunMask   = (1 << kRun) - 1;
    static constexpr int  kLastShift = kNibbleCells - 1 - kRun;



    bool  isNoise = false;
    int   shift   = 0;



    for (shift = 0; shift <= kLastShift && !isNoise; shift++)
    {
        isNoise = ((value >> shift) & kRunMask) == 0;
    }

    return isNoise;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramer::CollectRegions
//
//  Runs of random cells, one region each. A run that wraps the index is one
//  region starting before it.
//
////////////////////////////////////////////////////////////////////////////////

void LatchFramer::CollectRegions (FramedTrack & inOutTrack)
{
    uint32_t        n      = inOutTrack.cellCount;
    uint32_t        k      = 0;
    uint32_t        first  = 0;
    uint32_t        cell   = 0;
    bool            isNext = false;
    RandomRegion *  last   = nullptr;



    inOutTrack.randomRegions.clear();

    //  Start the walk on a cell that is not random, so a region that wraps the
    //  index is found whole. A track that is random everywhere is one region.
    while (first < n && inOutTrack.isRandomCell[first] != 0)
    {
        first++;
    }

    if (n > 0 && first == n)
    {
        inOutTrack.randomRegions.push_back ({ 0, n });
    }

    for (k = 0; k < n && first < n; k++)
    {
        cell = (first + k) % n;

        if (inOutTrack.isRandomCell[cell] == 0)
        {
            continue;
        }

        last   = inOutTrack.randomRegions.empty() ? nullptr : &inOutTrack.randomRegions.back();
        isNext = last != nullptr && (last->startCell + last->cellCount) % n == cell;

        if (isNext)
        {
            last->cellCount++;
        }
        else
        {
            inOutTrack.randomRegions.push_back ({ cell, 1 });
        }
    }
}