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
//  from a scale for the kind that touched its hottest byte most: the run
//  scale for a byte run, the read or write scale for one read or written as
//  data more than it is run. A byte nothing touched has the heat map's cold
//  color, which is no color. A list cell, which has one fill, takes the
//  hottest.
//
//  A BASIC line's bytes are its tokens, which the interpreter reads as the
//  line runs, so for tokens a read is drawn as the line running, in the code
//  color; only a write hotter than the reads is in the write color.
//
////////////////////////////////////////////////////////////////////////////////

class HeatStrip
{
public:
    static constexpr size_t  kMaxBlocks   = 4;
    static constexpr size_t  kMaxTipBytes = 8;

    //  The kind of access a scale is for.
    enum class Kind
    {
        Run,
        Read,
        Write,
    };

    struct Strip
    {
        std::wstring                                 text;
        std::vector<std::tuple<int, int, uint32_t>>  colors;      // each space's fill
        uint32_t                                     hottest      = 0;
        Kind                                         hottestKind  = Kind::Run;
        Byte                                         hottestLevel = 0;
    };

    //  A heat cell's tip: each byte's counts of runs, reads and writes, or for
    //  a run longer than kMaxTipBytes the sums over all of them, then what the
    //  cell's color means; and the cell's scale with the mark at its level.
    struct Tip
    {
        std::wstring           text;
        std::vector<uint32_t>  gradient;
        float                  mark = -1.0f;
    };

    //  A scale's colors at level 1, the middle and level 255. The run scale is
    //  a soft yellow through orange to a strong red, read blue through sky to
    //  pale cyan, and write purple through magenta to pink: three hues apart
    //  from end to end, each bright enough at its low end to be seen on a
    //  light or dark page.
    static std::array<uint32_t, 3>  GetScaleStops (Kind kind);

    //  A level's color on a kind's scale; level 0 is the palette's cold color.
    static uint32_t  GetScaleColor (Kind kind, Byte level, const HeatMapView::Palette & palette);

    static Strip  Get (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                       const HeatMapView::Palette & palette, bool isTokens);

    //  The strip as a list cell: its spaces, filled with the hottest color.
    static DxuiListView::Cell  GetCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                                        const HeatMapView::Palette & palette, bool isTokens);

    //  A BASIC line's cell: one fill in the run scale's color for how often
    //  BASIC began the line, from the statement count at its first token,
    //  and a tip with the count. A line that hasn't run has no color.
    static DxuiListView::Cell  GetLineCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word firstToken, const HeatMapView::Palette & palette);

    //  A compound line's cell: its hottest statement's, from the first token
    //  of each, with that statement's count in the tip.
    static DxuiListView::Cell  GetLineCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, const std::vector<Word> & firstTokens, const HeatMapView::Palette & palette);

    //  The bytes a disassembly line's code bytes text lists, as "A9 00".
    static size_t  CountBytes (const std::string & bytes);

    static Tip           GetTip        (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count, bool isTokens);

    //  A heat column heading's tip: what the column is, in lines short enough
    //  for a tip that doesn't wrap, then a line for each scale, with the
    //  scale's colors for that line and none for the others. A BASIC
    //  program's column has the run and write scales.
    struct HeadingTip
    {
        std::wstring                        text;
        std::vector<std::vector<uint32_t>>  lineScales;
    };

    static HeadingTip    GetHeadingTip (bool isTokens);

    //  A scale's meaning in one line, as "Run: yellow to red".
    static std::wstring  GetScaleName  (Kind kind);
};
