#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuMetrics
//
//  The dimensions a popup menu lays itself out with, read from the same
//  Windows settings a real menu reads: `SPI_GETNONCLIENTMETRICS` for the menu
//  font and the item height, and `SM_CXMENUCHECK` for the check gutter. A user
//  who enlarges the menu font in Ease of Access, or a display at any scale,
//  moves our menus exactly as far as it moves Notepad's.
//
//  Everything is in physical pixels for one DPI, so a widget holds the struct
//  the way it holds its scaler and re-reads it when the DPI changes. Nothing
//  here is a DIP constant to be scaled again at paint time.
//
//  The column model, left to right: an edge pad, the check gutter, a gap, the
//  label column, the accelerator gap, the accelerator column, and a right pad
//  wide enough for a submenu arrow. `FromSystem` fixes every part of it except
//  the two column widths, which only the text in a given menu can settle.
//
//  `FromSystem` never fails. A system that refuses the metrics call gets the
//  Windows defaults scaled to the requested DPI, which is what the refused
//  call would have returned anyway.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiMenuMetrics
{
    float  fontPx            = 0.0f;   // em size of the system menu font
    int    lineHeightPx      = 0;      // one line of that font, for centering
    int    rowHeightPx       = 0;      // a command row
    int    separatorHeightPx = 0;
    int    separatorInsetPx  = 0;
    int    checkGutterPx     = 0;
    int    leftPadPx         = 0;
    int    gutterGapPx       = 0;      // check gutter to label column
    int    accelGapPx        = 0;      // label column to accelerator column
    int    rightPadPx        = 0;      // accelerator column to the right edge
    int    minWidthPx        = 0;

    static DxuiMenuMetrics  FromSystem (UINT dpi);
};
