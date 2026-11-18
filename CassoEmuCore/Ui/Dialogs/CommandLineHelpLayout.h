#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpTable
//
//  Where the help dialog's option table goes, in pixels relative to the
//  table's top left: the switch column, the description column, and each
//  row's band.
//
////////////////////////////////////////////////////////////////////////////////

struct CommandLineHelpTable
{
    struct Row
    {
        int  topPx    = 0;
        int  heightPx = 0;
    };

    int               optionWidthPx      = 0;
    int               descriptionLeftPx  = 0;
    int               descriptionWidthPx = 0;
    int               heightPx           = 0;
    std::vector<Row>  rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpLayout
//
//  Two columns for the help dialog: the switch column as wide as its widest
//  switch, up to a share of the table, and the descriptions wrapped in the
//  rest. Measurement is passed in, so a test can supply fake measurements.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineHelpLayout
{
public:
    //  The widest share of the table the switch column takes, in percent.
    static constexpr int  kMaxOptionPercent = 45;

    //  The height of row `row`'s text when wrapped at `widthPx`.
    using MeasureHeight = std::function<int (size_t row, int widthPx)>;

    struct Input
    {
        int               widthPx       = 0;
        int               columnGapPx   = 0;
        int               rowPadPx      = 0;
        int               lineHeightPx  = 0;
        std::vector<int>  optionWidthsPx;
        MeasureHeight     measureOption;
        MeasureHeight     measureDescription;
    };

    static CommandLineHelpTable  Compute (const Input & input);
};
