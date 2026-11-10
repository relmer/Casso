#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicOracleRows
//
//  Lines typed into Integer BASIC on an emulated Apple ][, each beside the
//  bytes the ROM stored for it: the line after its length and number, through
//  the $01 at the end. A null `stored` is a line that got
//  *** SYNTAX ERR or *** >32767 ERR.
//
////////////////////////////////////////////////////////////////////////////////

class IntegerBasicOracleRows
{
public:
    struct Row
    {
        const char  * typed;
        const char  * stored;
    };

    static const Row     s_kRows[];
    static const size_t  s_kRowCount;
};
