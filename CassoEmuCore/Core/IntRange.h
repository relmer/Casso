#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntRange
//
//  Integer limits taken and returned by value. std::min, std::max and
//  std::clamp take and return references, and MSVC 14.52's whole-program
//  optimizer miscompiled std::max (1, std::min (a, b)) inlined into a
//  caller: it read the value 1 as an address.
//
////////////////////////////////////////////////////////////////////////////////

class IntRange
{
public:
    //  The value capped at the ceiling, and never below the floor. When the
    //  ceiling is below the floor, the floor is returned, as
    //  std::max (floor, std::min (value, ceiling)) returns it.
    static constexpr int  Limit (int value, int ceiling, int floor)
    {
        int  capped = value < ceiling ? value : ceiling;



        return capped > floor ? capped : floor;
    }
};
