#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextLayout
//
//  An Apple text preview's lines as the rows of a screen as wide as the
//  preview pane: a line longer than the columns continues on the next row,
//  and an empty line is one empty row.
//
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class AppleTextLayout
{
public:
    static std::vector<std::wstring>  Wrap (const std::vector<std::wstring> & lines, int columns);

    //  The rows of a screen `columns` wide with a border of one blank row and
    //  column around the text, as the Apple's own screen has black around
    //  its picture.
    static std::vector<std::wstring>  MakeScreenRows (const std::vector<std::wstring> & lines, int columns);
};
