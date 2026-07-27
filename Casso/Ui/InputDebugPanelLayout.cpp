#include "Pch.h"

#include "InputDebugPanelLayout.h"


static constexpr int  kMargin96         = 8;
static constexpr int  kRowHeight96      = 22;
static constexpr int  kRowGap96         = 4;
static constexpr int  kRowVGap96        = 14;
static constexpr int  kAllCheckWidth96  = 52;
static constexpr int  kCheckWidth96     = 92;
static constexpr int  kRowLabelWidth96  = 100;
static constexpr int  kPairLabelWidth96 = 150;
static constexpr int  kDropdownWidth96  = 132;
static constexpr int  kDropdownHeight96 = 24;
static constexpr int  kButtonWidth96    = 90;
static constexpr int  kButtonHeight96   = 26;


static int Scale (int dipValue, UINT dpi) noexcept
{
    return MulDiv (dipValue, (int) dpi, 96);
}


static RECT MakeRect (int x, int y, int w, int h) noexcept
{
    RECT  r;

    r.left   = x;
    r.top    = y;
    r.right  = x + w;
    r.bottom = y + h;
    return r;
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
    bool  showJoystickCheck,
    bool  showPaddleCheck,
    UINT  dpi) noexcept
{
    InputPanelLayoutSlots  slots        = {};
    int                    margin       = Scale (kMargin96,         dpi);
    int                    rowHeight    = Scale (kRowHeight96,      dpi);
    int                    rowGap       = Scale (kRowGap96,         dpi);
    int                    rowVGap      = Scale (kRowVGap96,        dpi);
    int                    allWidth     = Scale (kAllCheckWidth96,  dpi);
    int                    checkWidth   = Scale (kCheckWidth96,     dpi);
    int                    rowLblWidth  = Scale (kRowLabelWidth96,  dpi);
    int                    pairLblWidth = Scale (kPairLabelWidth96, dpi);
    int                    dropWidth    = Scale (kDropdownWidth96,  dpi);
    int                    dropHeight   = Scale (kDropdownHeight96, dpi);
    int                    buttonWidth  = Scale (kButtonWidth96,    dpi);
    int                    buttonHeight = Scale (kButtonHeight96,   dpi);
    int                    x            = 0;
    int                    y            = topOffsetPx + margin;
    int                    lvWidth      = 0;
    int                    lvHeight     = 0;


    x = margin;
    slots.emuLabel = MakeRect (x, y, rowLblWidth, rowHeight);
    x += rowLblWidth + rowGap;
    slots.allCheck = MakeRect (x, y, allWidth, rowHeight);
    x += allWidth + rowGap;
    slots.emuKeyboardCheck = MakeRect (x, y, checkWidth, rowHeight);
    x += checkWidth + rowGap;
    if (showJoystickCheck)
    {
        slots.joystickCheck = MakeRect (x, y, checkWidth, rowHeight);
        x += checkWidth + rowGap;
    }
    if (showPaddleCheck)
    {
        slots.paddleCheck = MakeRect (x, y, checkWidth, rowHeight);
        x += checkWidth + rowGap;
    }
    y += rowHeight + rowVGap;

    x = margin;
    slots.hostLabel = MakeRect (x, y, rowLblWidth, rowHeight);
    x += rowLblWidth + rowGap;
    slots.hostKeyboardCheck = MakeRect (x, y, checkWidth, rowHeight);
    y += rowHeight + rowVGap;

    for (int p = 0; p < 2; p++)
    {
        x = margin;
        slots.pairLabel[p] = MakeRect (x, y, pairLblWidth, dropHeight);
        x += pairLblWidth + rowGap;
        slots.pairDropdown[p] = MakeRect (x, y, dropWidth, dropHeight);
        y += dropHeight + rowVGap;
    }

    slots.pauseButton = MakeRect (margin,                             y, buttonWidth, buttonHeight);
    slots.clearButton = MakeRect (margin + (buttonWidth + rowGap),     y, buttonWidth, buttonHeight);
    slots.copyButton  = MakeRect (margin + 2 * (buttonWidth + rowGap), y, buttonWidth, buttonHeight);
    y += buttonHeight + rowVGap;

    lvWidth  = clientWidthPx  - 2 * margin;
    lvHeight = clientHeightPx - y - margin;
    if (lvWidth  < 1) { lvWidth  = 1; }
    if (lvHeight < 1) { lvHeight = 1; }
    slots.listView = MakeRect (margin, y, lvWidth, lvHeight);

    return slots;
}

