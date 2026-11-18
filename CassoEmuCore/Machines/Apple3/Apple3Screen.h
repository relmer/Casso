#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Screen
//
//  One frame of the Apple /// display as color numbers 0-15, 560 by 192. A
//  280-dot mode fills two pixels per dot. The palette and the monitor style
//  turn the numbers into colors.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Screen
{
public:
    static constexpr int   kWidth  = 560;
    static constexpr int   kHeight = 192;
    static constexpr Byte  kBlack  = 0;
    static constexpr Byte  kWhite  = 15;

    void  Set (int x, int y, Byte color)  { m_pixels[(size_t) y * kWidth + (size_t) x] = color; }
    Byte  Get (int x, int y) const        { return m_pixels[(size_t) y * kWidth + (size_t) x]; }

private:
    std::array<Byte, (size_t) kWidth * kHeight>  m_pixels = {};
};
