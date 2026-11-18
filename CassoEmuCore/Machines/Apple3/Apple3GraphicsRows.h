#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"

class Apple3Screen;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3GraphicsRows
//
//  The graphics modes' shared layout. A buffer is 8K at $2000, $4000,
//  $6000 or $8000 in bank 0, laid out as the Apple II's hi-res screen: the
//  scan lines of each group $400 apart, the groups of each third $80 apart
//  and the thirds $28 apart. A line is 40 bytes, 7 bits of each
//  displayed, least significant first.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3GraphicsRows
{
public:
    static constexpr int   kLines       = 192;
    static constexpr int   kLineBytes   = 40;
    static constexpr int   kByteDots    = 7;
    static constexpr Word  kBuffer1     = 0x2000;
    static constexpr Word  kBuffer2     = 0x4000;
    static constexpr Word  kBuffer3     = 0x6000;
    static constexpr Word  kBuffer4     = 0x8000;

    //  The address of a line's first byte, after the smooth scroll offset.
    static Word  GetLineAddress (int line, Word buffer, int scrollOffset);

    //  A byte of bank 0 at a graphics address.
    static Byte  Read           (const Apple3VideoInput & input, Word address);
};