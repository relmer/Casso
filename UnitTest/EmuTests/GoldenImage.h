#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GoldenImage
//
//  A rendered picture compared against the one checked in, pixel for pixel.
//
//  There is no tolerance, by design. A tolerance is a number someone has to
//  pick, and every value ever picked has let a real regression through while
//  still failing on the noise it was meant to absorb. The chain under test is
//  deterministic -- the CRT tests pin that first -- so the honest assertion is
//  equality, and a change that moves one pixel is a change someone looks at.
//
//  Goldens live in UnitTest/Fixtures/golden/<name>.png. When the picture
//  differs, or no golden exists yet, the picture that WAS produced is written
//  under the temp directory as <name>.actual.png and the failure says where.
//  Blessing a golden is copying that file into the fixtures directory, which
//  is a decision a person makes by looking at it, not something a test does.
//
////////////////////////////////////////////////////////////////////////////////

class GoldenImage
{
public:

    //  `bgra` is width * height words in the framebuffer's 0xAARRGGBB order,
    //  which is what a WARP read-back and the emulated framebuffer both give.
    static void  AssertMatches (const std::vector<uint32_t> &  bgra,
                                int                            width,
                                int                            height,
                                const wchar_t *                goldenName);

private:

    static std::filesystem::path  GoldenPath  (const wchar_t * goldenName);
    static std::filesystem::path  ActualPath  (const wchar_t * goldenName);
    static HRESULT                WritePng    (const std::filesystem::path &  path,
                                               const std::vector<uint32_t> &  bgra,
                                               int                            width,
                                               int                            height);
};
