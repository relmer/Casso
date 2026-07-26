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
    if (!hasRendered)
    {
        return -1;   // first render: everything dirty
    }

    return (std::min) (platenRow - 3 * PrinterGrid::kPinBandRows,
                       renderedPlaten - PrinterGrid::kPinBandRows);
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
    int   topRow = (std::max) (0, revealRow - spanFirstRow);   // span-relative
    int   botRow = revealRow - spanFirstRow + PrinterGrid::kPinBandRows - 1;

    for (int r = topRow; r <= botRow; r++)
    {
        for (int c = loCol; c <= hiCol; c++)
        {
            if (spanRaster.CellAt (c, r) != 0)
            {
                return true;
            }
        }
    }

    return false;
}
