#include "Pch.h"

#include "InputDebugPanelLayout.h"


namespace
{
    constexpr int  kMargin96       = 8;
    constexpr int  kRowHeight96    = 22;
    constexpr int  kRowGap96       = 4;
    constexpr int  kCheckWidth96   = 110;
    constexpr int  kRowLabelWidth96 = 56;
    constexpr int  kButtonWidth96  = 90;
    constexpr int  kButtonHeight96 = 26;


    int Scale (int dipValue, UINT dpi) noexcept
    {
        return MulDiv (dipValue, (int) dpi, 96);
    }


    RECT MakeRect (int x, int y, int w, int h) noexcept
    {
        RECT  r;

        r.left   = x;
        r.top    = y;
        r.right  = x + w;
        r.bottom = y + h;
        return r;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeInputDebugPanelLayout
//
////////////////////////////////////////////////////////////////////////////////

InputPanelLayoutSlots ComputeInputDebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    int   topOffsetPx,
    UINT  dpi) noexcept
{
    InputPanelLayoutSlots  slots       = {};
    int                    margin      = Scale (kMargin96,        dpi);
    int                    rowHeight   = Scale (kRowHeight96,     dpi);
    int                    rowGap      = Scale (kRowGap96,        dpi);
    int                    checkWidth  = Scale (kCheckWidth96,    dpi);
    int                    rowLblWidth = Scale (kRowLabelWidth96, dpi);
    int                    buttonWidth = Scale (kButtonWidth96,   dpi);
    int                    buttonHeight = Scale (kButtonHeight96, dpi);
    int                    x           = 0;
    int                    y           = topOffsetPx + margin;
    int                    lvWidth     = 0;
    int                    lvHeight    = 0;


    x = margin;
    slots.showLabel = MakeRect (x, y, rowLblWidth, rowHeight);
    x += rowLblWidth + rowGap;
    for (int i = 0; i < kInputCategoryCheckCount; i++)
    {
        slots.categoryChecks[i] = MakeRect (x, y, checkWidth, rowHeight);
        x += checkWidth;
    }
    y += rowHeight + rowGap;

    slots.pauseButton = MakeRect (margin,                        y, buttonWidth, buttonHeight);
    slots.clearButton = MakeRect (margin + buttonWidth + rowGap, y, buttonWidth, buttonHeight);
    y += buttonHeight + rowGap;

    lvWidth  = clientWidthPx  - 2 * margin;
    lvHeight = clientHeightPx - y - margin;
    if (lvWidth  < 1) { lvWidth  = 1; }
    if (lvHeight < 1) { lvHeight = 1; }
    slots.listView = MakeRect (margin, y, lvWidth, lvHeight);

    return slots;
}
