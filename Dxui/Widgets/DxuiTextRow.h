#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"

struct DxuiIconImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSpan
//
//  A run of a cell's characters drawn in a color of their own, and bold,
//  italic or underlined. An inverse run is filled with its color and its
//  text drawn in the background color.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextSpan
{
    //  A character's style bits, from the spans over it.
    static constexpr uint8_t  kStyleBold      = 0x01;
    static constexpr uint8_t  kStyleItalic    = 0x02;
    static constexpr uint8_t  kStyleUnderline = 0x04;
    static constexpr uint8_t  kStyleInverse   = 0x08;

    int       cell       = 0;
    int       start      = 0;
    int       length     = 0;
    uint32_t  argb       = 0;
    uint32_t  background = 0;       // a fill behind the characters; with argb zero, their text color is the usual one
    bool      isLink     = false;   // with argb zero, drawn in the theme's link color
    bool      bold       = false;
    bool      italic     = false;
    bool      underline  = false;
    bool      inverse    = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPicture
//
//  A picture in 32-bit BGRA pixels, drawn a pixel a DIP, or smaller to
//  fit the text's width.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextPicture
{
    std::vector<uint32_t>  bgra;
    int                    width  = 0;
    int                    height = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRow
//
//  A row's fill spans the view's width on every line it takes; its icon
//  is drawn in the gutter, on its first line. A row of one cell is
//  aligned in the text's width on each line it wraps onto; Justify draws
//  as Left. A row with a picture is the picture alone, on as many lines
//  as it is tall.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextRow
{
    std::vector<std::wstring>               cells;
    bool                                    warning    = false;
    bool                                    heading    = false;   // drawn in the accent color
    uint32_t                                background = 0;
    std::shared_ptr<const DxuiIconImage>    icon;
    std::vector<DxuiTextSpan>               spans;

    //  The row's height as a fraction of a line, for a blank row that
    //  spaces the rows around it. A row this applies to takes one line.
    float                                   height     = 1.0f;

    //  A wrapped line of the last cell starts this many characters into
    //  the cell, so a help row's description wraps under itself rather
    //  than under its syntax.
    int                                     wrapIndent = 0;

    //  The fill of the gutter's band (SetGutterBand) on each line the
    //  row takes; zero for none.
    uint32_t                                gutterFill = 0;

    DxuiTextHAlign                          alignment  = DxuiTextHAlign::Left;
    std::shared_ptr<const DxuiTextPicture>  picture;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLine
//
//  One drawn line: its row, and the run of the row's last cell it shows.
//  The row's other cells are drawn on its first line only.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextLine
{
    int   row    = 0;
    int   start  = 0;
    int   length = 0;
    bool  first  = true;
};
