#include "Pch.h"

#include "FluxTestImages.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTestImages::BitsToFlux
//
//  Lays the cells end to end in time, carrying the fraction so a long track
//  does not drift, and records one transition in the middle of each 1 cell.
//  Time after the last transition is added to the first gap, the way the
//  format has to record it.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FluxTestImages::BitsToFlux (
    const vector<Byte>          &  packedBits,
    size_t                         bitCount,
    const vector<CellStretch>   &  stretches)
{
    constexpr Byte    kRunByte = 255;



    vector<uint64_t>  ticks;
    vector<uint64_t>  gaps;
    vector<Byte>      flux;
    double            time     = 0;
    double            cell     = kNominalCellTicks;
    size_t            stretch  = 0;
    size_t            i        = 0;
    uint64_t          prev     = 0;
    uint64_t          total    = 0;
    uint64_t          gap      = 0;



    for (i = 0; i < bitCount; i++)
    {
        while (stretch < stretches.size() && stretches[stretch].firstBit <= i)
        {
            cell = stretches[stretch].cellTicks;
            stretch++;
        }

        if ((packedBits[i >> 3] >> (7 - (i & 7))) & 1)
        {
            ticks.push_back (static_cast<uint64_t> (llround (time + cell / 2)));
        }

        time += cell;
    }

    total = static_cast<uint64_t> (llround (time));

    for (i = 0; i < ticks.size(); i++)
    {
        gaps.push_back (ticks[i] - prev);
        prev = ticks[i];
    }

    if (!gaps.empty() && prev < total)
    {
        gaps[0] += total - prev;
    }

    for (i = 0; i < gaps.size(); i++)
    {
        gap = gaps[i];

        while (gap >= kRunByte)
        {
            flux.push_back (kRunByte);
            gap -= kRunByte;
        }

        flux.push_back (static_cast<Byte> (gap));
    }

    return flux;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTestImages::BitsToNominalFlux
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> FluxTestImages::BitsToNominalFlux (const vector<Byte> & packedBits, size_t bitCount)
{
    return BitsToFlux (packedBits, bitCount, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTestImages::MakeAlternating
//
////////////////////////////////////////////////////////////////////////////////

vector<FluxTestImages::CellStretch> FluxTestImages::MakeAlternating (
    size_t   bitCount,
    size_t   stretchBits,
    double   firstCellTicks,
    double   secondCellTicks)
{
    vector<CellStretch>  stretches;
    size_t               at    = 0;
    bool                 first = true;



    for (at = 0; at < bitCount; at += stretchBits)
    {
        stretches.push_back ({ at, first ? firstCellTicks : secondCellTicks });
        first = !first;
    }

    return stretches;
}
