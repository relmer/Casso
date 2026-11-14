#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpLayout::Compute
//
//  The switch column takes its widest switch, capped at kMaxOptionPercent of
//  the table, and the description column takes the rest after the gap. Each
//  row is as tall as the taller of its two cells, no shorter than one line,
//  with the padding above and below.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineHelpTable CommandLineHelpLayout::Compute (const Input & input)
{
    constexpr int         kPercent   = 100;
    constexpr int         kPadEdges  = 2;
    CommandLineHelpTable  table;
    int                   widest     = 0;
    int                   cap        = input.widthPx * kMaxOptionPercent / kPercent;
    int                   top        = 0;
    int                   optionH    = 0;
    int                   describedH = 0;
    int                   cellH      = 0;
    size_t                i          = 0;



    for (int width : input.optionWidthsPx)
    {
        widest = std::max (widest, width);
    }

    table.optionWidthPx      = std::min (widest, cap);
    table.descriptionLeftPx  = table.optionWidthPx + input.columnGapPx;
    table.descriptionWidthPx = std::max (1, input.widthPx - table.descriptionLeftPx);

    for (i = 0; i < input.optionWidthsPx.size(); ++i)
    {
        optionH    = input.measureOption      ? input.measureOption      (i, table.optionWidthPx)      : 0;
        describedH = input.measureDescription ? input.measureDescription (i, table.descriptionWidthPx) : 0;
        cellH      = std::max ({ input.lineHeightPx, optionH, describedH });

        table.rows.push_back ({ top, cellH + kPadEdges * input.rowPadPx });

        top += cellH + kPadEdges * input.rowPadPx;
    }

    table.heightPx = top;

    return table;
}





