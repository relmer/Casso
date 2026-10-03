#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTestImages
//
//  Builds flux tracks for tests from ordinary bit streams, with a chosen cell
//  length for each stretch of the track. That is what a real disk written on
//  a drive running fast or slow looks like, and it is the only way to get
//  flux test data without a preservation dump.
//
////////////////////////////////////////////////////////////////////////////////

class FluxTestImages
{
public:
    // A stretch starting at firstBit whose cells are cellTicks long, until
    // the next stretch begins.
    struct CellStretch
    {
        size_t  firstBit  = 0;
        double  cellTicks = 0;
    };

    static constexpr double  kNominalCellTicks = 1408.0 / 45.0;
    static constexpr double  kFastCellTicks    = 29.6;    // 3.7 us
    static constexpr double  kSlowCellTicks    = 32.8;    // 4.1 us

    // One transition per 1 bit, in the middle of its cell. The revolution is
    // the sum of every cell.
    static vector<Byte>  BitsToFlux       (const vector<Byte>          &  packedBits,
                                           size_t                         bitCount,
                                           const vector<CellStretch>   &  stretches);

    static vector<Byte>  BitsToNominalFlux (const vector<Byte> & packedBits, size_t bitCount);

    // Stretches alternating between two cell lengths every stretchBits bits.
    static vector<CellStretch>  MakeAlternating (size_t   bitCount,
                                                 size_t   stretchBits,
                                                 double   firstCellTicks,
                                                 double   secondCellTicks);
};
