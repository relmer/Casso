#include "Pch.h"

#include "Devices/Printer/PrinterPreviewModel.h"

#include "Devices/Printer/PrinterTypes.h"
#include "Devices/Printer/PrintRaster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::RevealColumnSpan
//
////////////////////////////////////////////////////////////////////////////////

PrinterPreviewModel::RevealSpan PrinterPreviewModel::RevealColumnSpan (bool sweepLtr, int carriageCol)
{
    RevealSpan   span;



    span.loDots = sweepLtr ? 0 : carriageCol;
    span.hiDots = sweepLtr ? carriageCol : PrinterGrid::kDotsPerRow;

    return span;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::StripTornOff
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPreviewModel::StripTornOff (int rowsUsed, int viewportLiveRow)
{
    return rowsUsed - 1 < viewportLiveRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::LiveBandOutsideSpan
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPreviewModel::LiveBandOutsideSpan (int platenRow, int spanFirstRow, int spanLastRow)
{
    return (platenRow + PrinterGrid::kPinBandRows - 1 > spanLastRow)
        || (platenRow < spanFirstRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::DirtyFromRow
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPreviewModel::DirtyFromRow (bool hasRendered, int platenRow, int renderedPlaten)
{
    // -1 is at-or-above any span top, which RenderSpan reads as "whole span
    // dirty" -- the right answer for a panel that has never rendered.
    return hasRendered
               ? (std::min) (platenRow - 3 * PrinterGrid::kPinBandRows,
                             renderedPlaten - PrinterGrid::kPinBandRows)
               : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::SpanMoved
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPreviewModel::SpanMoved (int firstRow, int lastRow, int renderedFirstRow, int renderedLastRow)
{
    return firstRow != renderedFirstRow || lastRow != renderedLastRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::RevealMoved
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPreviewModel::RevealMoved (int revealRow, int revealCol, int renderedRevealRow, int renderedRevealCol)
{
    return revealRow != renderedRevealRow || revealCol != renderedRevealCol;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::AudioSampleWindow
//
//  Picks the column span to check for ink when deciding whether the head is
//  currently PRINTING or merely traversing -- which is what selects the print
//  sound rather than the quiet carriage sound.
//
//  The span is the ground the head just swept, extended slightly BEHIND it.
//  That bridge exists because ink is laid at the strike position while the
//  animation samples a moment later: without it, sparse text produces gaps
//  where the head has passed the last dot but not yet reached the next, and
//  the sound stutters mid-word. Which side is "behind" depends on the sweep
//  direction, hence the two arms.
//
//  A LINE WRAP is detected and handled separately: when the row changed or the
//  column jumped more than half a line, no contiguous swept span exists at
//  all, so the whole row is sampled and any ink counts as a printing pass.
//
//  A negative previous column -- the first sample of a pass -- is treated as a
//  wrap for the same reason: there is no prior position to sweep from.
//
//  The result is clamped to the row, so the bridge cannot index past either
//  margin.
//
////////////////////////////////////////////////////////////////////////////////

PrinterPreviewModel::InkSample PrinterPreviewModel::AudioSampleWindow (bool sweepLtr, int prevCol, int curCol,
                                                                       int revealRow, int renderedRevealRow)
{
    constexpr int   kInkBridgeDots = (PrinterGrid::kDotsPerInchH * 3) / 20;   // 0.15"

    InkSample   sample;
    int         colJump = (curCol > prevCol) ? (curCol - prevCol) : (prevCol - curCol);
    bool        wrapped = (revealRow != renderedRevealRow)
                          || (prevCol < 0)
                          || (colJump > PrinterGrid::kDotsPerRow / 2);

    if (wrapped)
    {
        // Line wrap: the column jumped margin to margin, so no contiguous swept
        // span exists -- sample the whole row (any ink == a printing pass).
        sample.loCol = 0;
        sample.hiCol = PrinterGrid::kDotsPerRow - 1;
    }
    else
    {
        int   lo = (std::min) (prevCol, curCol);
        int   hi = (std::max) (prevCol, curCol);

        sample.loCol = sweepLtr ? (lo - kInkBridgeDots) : lo;   // bridge behind the head
        sample.hiCol = sweepLtr ? hi : (hi + kInkBridgeDots);
    }

    sample.loCol = (std::max) (0, sample.loCol);
    sample.hiCol = (std::min) (PrinterGrid::kDotsPerRow - 1, sample.hiCol);

    return sample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModel::BandHasInk
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPreviewModel::BandHasInk (const PrintRaster & spanRaster, int spanFirstRow,
                                      int revealRow, int loCol, int hiCol)
{
    int   topRow  = (std::max) (0, revealRow - spanFirstRow);   // span-relative
    int   botRow  = revealRow - spanFirstRow + PrinterGrid::kPinBandRows - 1;
    int   r       = 0;
    int   c       = 0;
    bool  hasInk  = false;

    // Any inked cell in the band answers the question, so both loops carry
    // the found test -- this runs once per audio frame over a live band.
    for (r = topRow; !hasInk && r <= botRow; r++)
    {
        for (c = loCol; !hasInk && c <= hiCol; c++)
        {
            hasInk = (spanRaster.CellAt (c, r) != 0);
        }
    }

    return hasInk;
}
