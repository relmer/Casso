#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowLayout
//
//  Where the disk inspector's content goes under its menus: the toolbar
//  row across the top, the comparison bar under it while comparing, the
//  platter column, the splitter, the track column, and the strips and
//  their controls along the bottom.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorWindowLayout
{
public:
    static constexpr double  kMinSplit  = 0.3;
    static constexpr double  kMaxSplit  = 0.7;
    static constexpr int     kMarginDip = 8;

    struct Inputs
    {
        RECT    bounds         = {};
        double  splitFraction  = 0.5;
        int     toolbarPx      = 0;
        bool    isComparing    = false;
        int     minColumnPx    = 0;    // the platter's control row
        int     sectorsPx      = 0;    // a sector's bytes at their full width
        int     cornerBandPx   = 0;    // the band under the platter for its corner groups
    };

    struct Frames
    {
        RECT  toolbar       = {};
        RECT  compareBar    = {};
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
    //  its control row, up to the splitter's largest share of the window.
    //  The column is no wider than the platter and its control row, and the
    //  track column has room for a sector's bytes at their full width, so
    //  their font follows the zoom.
    static Frames  Plan (const Inputs & in, const DxuiDpiScaler & scaler);

private:
    static constexpr int  s_kRowDip              = 28;
    static constexpr int  s_kStripDip            = 64;
    static constexpr int  s_kSplitterDip         = 6;
    static constexpr int  s_kBarDip              = 36;
    static constexpr int  s_kMinPlatterColumnDip = 420;
};
