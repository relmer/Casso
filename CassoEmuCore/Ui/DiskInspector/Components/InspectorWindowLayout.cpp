#include "Pch.h"

#include "Core/IntRange.h"
#include "Ui/DiskInspector/Components/InspectorWindowLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowLayout::Plan
//
//  The platter is as large as its column allows, above a band for its
//  corner groups. B's strip is under A's while comparing (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

InspectorWindowLayout::Frames InspectorWindowLayout::Plan (const Inputs & in, const DxuiDpiScaler & scaler)
{
    const RECT  & b         = in.bounds;
    int           margin    = scaler.ToPx (kMarginDip);
    int           row       = scaler.ToPx (s_kRowDip);
    int           width     = b.right - b.left;
    int           splitX    = b.left + static_cast<int> (width * in.splitFraction);
    int           bar       = in.isComparing ? scaler.ToPx (s_kBarDip) : 0;
    int           notice    = in.isComparing ? in.noticePx : 0;
    int           top       = b.top + in.toolbarPx + bar + notice;
    int           strip     = scaler.ToPx (s_kStripDip);
    int           band      = 2 * margin + strip + (in.isComparing ? strip + margin / 2 : 0) + margin / 2 + row;
    int           bandTop   = std::max (top, static_cast<int> (b.bottom) - band);
    int           x         = b.left + margin;
    int           column    = splitX - b.left - 2 * margin;
    int           areaH     = std::max (0, bandTop - top - 2 * margin);
    int           side      = IntRange::Limit (column, areaH, 0);
    int           minColumn = in.minColumnPx;
    int           maxColumn = std::max (minColumn, width - 4 * margin - scaler.ToPx (s_kSplitterDip) - scaler.ToPx (s_kMinTrackColumnDip));
    int           y         = 0;
    Frames        f;



    if (column < minColumn || column > maxColumn)
    {
        column = std::clamp (column, minColumn, maxColumn);
        splitX = b.left + 2 * margin + column;
        side   = IntRange::Limit (column, areaH, 0);
    }

    f.toolbar       = { b.left, b.top, b.right, b.top + in.toolbarPx };
    f.findArea      = { b.left, top, b.right, bandTop };
    f.splitter      = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), bandTop };
    f.trackColumn   = { f.splitter.right + margin, top + margin, b.right - margin, bandTop - margin / 2 };
    f.compareBar    = { b.left, top - notice - bar, b.right, top - notice };
    f.notice        = { b.left + margin, top - notice + ((notice > 0) ? margin : 0), b.right - margin, top };
    f.platter       = { b.left + margin + (column - side) / 2, top + margin, b.left + margin + (column - side) / 2 + side, top + margin + side };
    f.platter.bottom = std::max<LONG> (f.platter.top + 1, std::min<LONG> (f.platter.bottom, top + margin + areaH - in.cornerBandPx));
    f.platterArea   = { b.left, top, splitX, bandTop };
    f.platterColumn = { b.left + margin, top + margin, splitX - margin, top + margin + areaH };
    f.trackArea     = { f.splitter.right, top, b.right, top };

    y        = bandTop + margin;
    f.strip  = { x, y, b.right - margin, y + strip };
    y       += strip + margin / 2;
    f.stripB = { x, y, b.right - margin, y + (in.isComparing ? strip : 0) };
    y       += in.isComparing ? strip + margin / 2 : 0;

    f.stripControls = { x, y, b.right - margin, y + row };
    f.stripArea     = { b.left, bandTop, b.right, b.bottom };

    return f;
}
