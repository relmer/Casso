#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPosition / DxuiTextMatch
//
//  A character in a text view's row, its cells joined by tabs; and a match
//  of a search: where it starts and how many characters it takes, which a
//  regular expression can make differ from the text searched for.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextPosition
{
    int  row    = 0;
    int  offset = 0;

    bool operator== (const DxuiTextPosition & other) const { return row == other.row && offset == other.offset; }
    bool operator<  (const DxuiTextPosition & other) const { return row < other.row || (row == other.row && offset < other.offset); }
};

struct DxuiTextMatch
{
    DxuiTextPosition  start;
    int               length = 0;
};
