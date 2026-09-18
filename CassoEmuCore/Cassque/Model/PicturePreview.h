#pragma once

#include "Pch.h"


class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview
//
//  A binary that is the size and address of a graphics buffer, drawn the way
//  the machine would show it.
//
//  THE RULE IS THE SPEC'S AND NOTHING SMARTER: hi-res at $2000 or $4000 with
//  8192 or $1FF8 bytes, double hi-res at $2000 with 16384 bytes, lo-res at
//  $400 or $800 with 1024 bytes. A file that misses by a byte is a hex dump,
//  which is the honest answer for something nobody can prove is a picture.
//
//  The video modes render from a caller-supplied span and touch their bus
//  only when the span is null, so the bus handed in is never read; a test
//  proves that with a bus that fails on any read.
//
////////////////////////////////////////////////////////////////////////////////

class PicturePreview
{
public:
    enum class Mode { None, HiRes, DoubleHiRes, LoRes };

    static Mode  Choose (Word address, size_t length);

    static HRESULT  Render (Mode                     mode,
                            std::span<const Byte>    bytes,
                            Word                     address,
                            MemoryBus              & bus,
                            std::vector<uint32_t>  & outBgra,
                            int                    & outWidth,
                            int                    & outHeight);

    static constexpr int  kWidth  = 560;
    static constexpr int  kHeight = 384;

    static constexpr Word    kHiResPage1        = 0x2000;
    static constexpr Word    kHiResPage2        = 0x4000;
    static constexpr Word    kLoResPage1        = 0x0400;
    static constexpr Word    kLoResPage2        = 0x0800;
    static constexpr size_t  kHiResLength       = 8192;
    static constexpr size_t  kHiResShort        = 0x1FF8;
    static constexpr size_t  kDoubleHiResLength = 16384;
    static constexpr size_t  kLoResLength       = 1024;

private:
    static constexpr size_t  kAddressSpace = 0x10000;
};
