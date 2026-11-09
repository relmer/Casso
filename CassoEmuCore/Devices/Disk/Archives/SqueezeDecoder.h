#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SqueezeDecoder
//
//  Expands Squeeze, the Huffman coding of run-length encoded data that early
//  ShrinkIt and the CP/M-era SQ programs wrote. A NuFX thread starts with the
//  tree's node count; a `.QQ` file has a magic number, a checksum and a name
//  ahead of it. Each node is two 16-bit children, an index for a node or the
//  negated value plus one for a byte; 256 is the end. Bits are read least
//  significant first. The run-length step follows: a byte, $90 and a count
//  repeat the byte to the count, and $90 with a count of zero is $90 itself.
//
////////////////////////////////////////////////////////////////////////////////

class SqueezeDecoder
{
public:
    static HRESULT  Expand (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError);

private:
    static constexpr Byte    kRunEscape = 0x90;
    static constexpr Byte    kMagic1    = 0x76;
    static constexpr Byte    kMagic2    = 0xFF;
    static constexpr int     kEndCode   = 256;
    static constexpr size_t  kMaxNodes  = 257;
};
