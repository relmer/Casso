#pragma once

#include "Pch.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  DramPowerOnPattern
//
//  What Apple II DRAM holds at power-on: a regular FF FF 00 00 pattern with a
//  few random bytes, not noise. The details are in the .cpp.
//
////////////////////////////////////////////////////////////////////////////////

class DramPowerOnPattern
{
public:
    static constexpr size_t  kHoleStride = 512;

    // Fill `size` bytes of RAM whose first byte sits at a multiple of
    // kHoleStride in the machine's address space.
    static void  Fill (Byte * dst, size_t size, Prng & prng);

    // The pattern byte at `offset`, before any hole is applied.
    static Byte  GetPatternByte (size_t offset);

    // Whether `offset` is one of the seeded bytes in each kHoleStride block.
    static bool  IsHole (size_t offset);
};
