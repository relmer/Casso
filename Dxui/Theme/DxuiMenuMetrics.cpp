#include "Pch.h"

#include "DxuiMenuMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuMetrics::FromSystem
//
//  Reads the menu font and item height Windows hands its own menus, then
//  derives the rest of the column model from them.
//
//  The row is the taller of the system item height and a line of the menu
//  font plus a pad above and below, which is how a real menu keeps a row from
//  clipping its own text once the font is enlarged. `lfHeight` is negative
//  because it is an em size rather than a cell size, so the line height is
//  taken as a fixed fraction above the em; measuring it would need a device
//  context this struct deliberately does not have.
//
//  The pads are the one part with no system metric behind them, and they are
//  DELIBERATELY LOOSER THAN WINDOWS. A classic menu is built for applications
//  with dozens of items per list; Casso's longest menu is nine rows, so it
//  can afford the airier spacing a modern menu uses, and beside a slide-open
//  animation the tight classic rows read as dated. At 96 DPI this puts a row
//  at 32 px against Windows' 22, which is WinUI's item height.
//
//  What does NOT change is where the sizes COME FROM. A row is still the
//  taller of the system item height and a line of the system menu font plus
//  padding, so an enlarged menu font still moves every row, and every pad is
//  still a DIP scaled once for this display. The density is ours; the
//  tracking is the system's.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuMetrics DxuiMenuMetrics::FromSystem (UINT dpi)
{
    constexpr UINT     kBaseDpi               = 96;
    constexpr int      kDefaultMenuHeightDip  = 19;   // NONCLIENTMETRICS default
    constexpr int      kDefaultFontEmDip      = 12;   // Segoe UI, lfHeight -12
    constexpr int      kDefaultCheckGutterDip = 15;   // SM_CXMENUCHECK default
    constexpr int      kLinePercentOfEm       = 133;   // Segoe UI cell over em
    constexpr int      kMinFontDip            = 14;   // WinUI ControlContentThemeFontSize
    constexpr int      kRowPadDip             = 7;   // above and below the text
    constexpr int      kSeparatorPercentOfRow = 30;
    constexpr int      kLeftPadDip            = 6;
    constexpr int      kGutterGapDip          = 16;
    constexpr int      kAccelGapDip           = 20;
    constexpr int      kMinWidthDip           = 140;
    DxuiMenuMetrics    m                      = {};
    NONCLIENTMETRICSW  ncm                    = { sizeof (ncm) };
    UINT               effective              = (dpi == 0) ? kBaseDpi : dpi;
    int                emPx                   = 0;
    int                menuH                  = 0;
    int                textRowH               = 0;
    int                fontFloorPx            = 0;



    if (SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, sizeof (ncm), &ncm, 0, effective))
    {
        emPx  = (int) std::abs (ncm.lfMenuFont.lfHeight);
        menuH = ncm.iMenuHeight;
    }

    if (emPx <= 0)
    {
        emPx = MulDiv (kDefaultFontEmDip, (int) effective, (int) kBaseDpi);
    }

    if (menuH <= 0)
    {
        menuH = MulDiv (kDefaultMenuHeightDip, (int) effective, (int) kBaseDpi);
    }

    m.checkGutterPx = GetSystemMetricsForDpi (SM_CXMENUCHECK, effective);

    if (m.checkGutterPx <= 0)
    {
        m.checkGutterPx = MulDiv (kDefaultCheckGutterDip, (int) effective, (int) kBaseDpi);
    }

    // WinUI's body size as a FLOOR, not a multiple of the system em. WinUI
    // does not derive its type size from `lfMenuFont` at all -- it fixes
    // ControlContentThemeFontSize at 14 -- so there is no ratio here to
    // copy, and inventing one that happens to turn 12 into 14 would be a
    // coincidence pretending to be a rule.
    //
    // A floor takes WinUI's size at the Windows default and still yields to
    // a user who has enlarged the menu font past it, which is the direction
    // that matters: somebody who asked for bigger text gets bigger text.
    // The notification duration in DxuiTimedInfoBanner is a floor in the
    // same way.
    fontFloorPx    = MulDiv (kMinFontDip, (int) effective, (int) kBaseDpi);
    m.fontPx       = (float) ((emPx > fontFloorPx) ? emPx : fontFloorPx);
    m.lineHeightPx = MulDiv ((int) m.fontPx, kLinePercentOfEm, 100);

    textRowH = m.lineHeightPx + 2 * MulDiv (kRowPadDip, (int) effective, (int) kBaseDpi);

    m.rowHeightPx       = (textRowH > menuH) ? textRowH : menuH;
    m.separatorHeightPx = MulDiv (m.rowHeightPx, kSeparatorPercentOfRow, 100);
    m.leftPadPx         = MulDiv (kLeftPadDip,   (int) effective, (int) kBaseDpi);
    m.gutterGapPx       = MulDiv (kGutterGapDip, (int) effective, (int) kBaseDpi);
    m.accelGapPx        = MulDiv (kAccelGapDip,  (int) effective, (int) kBaseDpi);
    m.minWidthPx        = MulDiv (kMinWidthDip,  (int) effective, (int) kBaseDpi);
    m.separatorInsetPx  = m.leftPadPx;
    m.rightPadPx        = m.leftPadPx + m.checkGutterPx;

    return m;
}
