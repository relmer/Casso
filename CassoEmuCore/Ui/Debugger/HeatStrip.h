#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip
//
//  The heat columns' strip over a run of bytes in the CPU's address space: a
//  block per byte, or for a longer run up to kMaxBlocks blocks, each the
//  hottest of its bytes, in the heat map's colors. A byte read or written as
//  data more than it is run is r or w, in the read or write color, as the
//  heat map colors a byte by the kind that touched it most.
//
//  A BASIC line's bytes are its tokens, which the interpreter reads as the
//  line runs, so for tokens a read is drawn as the line running, a block in
//  the code color; only a write hotter than the reads is a letter.
//
////////////////////////////////////////////////////////////////////////////////

class HeatStrip
{
public:
    struct Strip
    {
        std::wstring                                 text;
        std::vector<std::tuple<int, int, uint32_t>>  colors;
    };

    static constexpr size_t  kMaxBlocks = 4;

    static Strip  Get (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                       const HeatMapView::Palette & palette, bool isTokens);

    //  The strip as a list cell.
    static DxuiListView::Cell  GetCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                                        const HeatMapView::Palette & palette, bool isTokens);

    //  The bytes a disassembly line's code bytes text lists, as "A9 00".
    static size_t  CountBytes (const std::string & bytes);
};
