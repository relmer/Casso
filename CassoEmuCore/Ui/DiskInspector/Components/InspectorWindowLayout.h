#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowLayout
//
//  Where the disk inspector's content goes under its menus: the toolbar
//  row across the top, the comparison bar under it while comparing and
//  any infobar of the comparison's under that, the platter column, the splitter, the track column, and the strips and
//  their controls along the bottom.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorWindowLayout
{
public:
    static constexpr int  kMarginDip = 8;

    struct Inputs
    {
        RECT    bounds         = {};
        double  splitFraction  = 0.5;
        int     toolbarPx      = 0;
        bool    isComparing    = false;
        int     minColumnPx    = 0;    // the platter's control row
        int     cornerBandPx   = 0;    // the band under the platter for its corner groups
        int     noticePx       = 0;    // the comparison's infobar under its bar
    };

    struct Frames
    {
        RECT  toolbar       = {};
        RECT  compareBar    = {};
        RECT  notice        = {};
        RECT  findArea      = {};
        RECT  splitter      = {};
        RECT  platter       = {};
        RECT  platterArea   = {};
        RECT  platterColumn = {};
        RECT  trackColumn   = {};
        RECT  trackArea     = {};    // the right of the track column; its bottom is set by the column
        RECT  strip         = {};
        RECT  stripB        = {};
        RECT  stripControls = {};
        RECT  stripArea     = {};
    };

    //  Wherever the splitter is, the platter column is at least as wide as
    //  its control row, and the track column is at least its least width.
    //  The platter is centered in a column wider than it is tall.
    static Frames  Plan (const Inputs & in, const DxuiDpiScaler & scaler);

private:
    static constexpr int  s_kRowDip              = 28;
    static constexpr int  s_kStripDip            = 64;
    static constexpr int  s_kSplitterDip         = 6;
    static constexpr int  s_kBarDip              = 36;
    static constexpr int  s_kMinTrackColumnDip   = 400;
};
