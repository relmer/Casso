#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip
//
//  The heat columns' strip over a run of bytes in the CPU's address space: a
//  space per byte, or for a longer run up to kMaxBlocks of them, each filled
//  with the heat map's color for the hottest of its bytes: the code color
//  for a byte run, the read or write color for one read or written as data
//  more than it is run, as the heat map colors a byte by the kind that
//  touched it most. A list cell, which has one fill, takes the hottest.
//
//  A BASIC line's bytes are its tokens, which the interpreter reads as the
//  line runs, so for tokens a read is drawn as the line running, in the code
//  color; only a write hotter than the reads is in the write color.
//
////////////////////////////////////////////////////////////////////////////////

class HeatStrip
{
public:
    struct Strip
    {
        std::wstring                                 text;
        std::vector<std::tuple<int, int, uint32_t>>  colors;      // each space's fill
        uint32_t                                     hottest = 0;
    };

    static constexpr size_t  kMaxBlocks = 4;

    static Strip  Get (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                       const HeatMapView::Palette & palette, bool isTokens);

    //  The strip as a list cell: its spaces, filled with the hottest color.
    static DxuiListView::Cell  GetCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                                        const HeatMapView::Palette & palette, bool isTokens);

    //  The bytes a disassembly line's code bytes text lists, as "A9 00".
    static size_t  CountBytes (const std::string & bytes);
};
