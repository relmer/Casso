#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LzcDecoder
//
//  Expands LZC, the LZW of Unix `compress`, which NuFX threads use at 12 and
//  16 bits. A stream may start with compress's three-byte header, its magic
//  and the maximum width; without one, the width is the one the thread's
//  format indicates, and block mode is on. Codes start at 9 bits, packed least
//  significant bit first, and the width grows by one when the next free entry
//  exceeds the largest code of the current width. Codes are written in groups
//  of eight, so after a width change or a clear the rest of the current group
//  is skipped.
//
////////////////////////////////////////////////////////////////////////////////

class LzcDecoder
{
public:
    static HRESULT  Expand (std::span<const Byte> in, int maxBits, size_t outLength, std::vector<Byte> & outBytes, std::string & outError);

private:
    static constexpr int   kInitialBits = 9;
    static constexpr int   kMinMaxBits  = 9;
    static constexpr int   kMaxMaxBits  = 16;
    static constexpr int   kClearCode   = 256;
    static constexpr int   kFirstCode   = 257;
    static constexpr Byte  kMagic1      = 0x1F;
    static constexpr Byte  kMagic2      = 0x9D;
    static constexpr Byte  kBlockFlag   = 0x80;
    static constexpr Byte  kBitsMask    = 0x1F;

    static uint64_t  RoundUpToGroup (uint64_t bit, int width);
};
