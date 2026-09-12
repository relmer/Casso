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
//  The pads are the one part with no system metric behind them. They are DIPs
//  scaled here and never again, chosen against a real menu captured on a
//  120 DPI display: at 96 DPI they put the label column 35 px in from the
//  left edge, which is where Windows puts it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuMetrics DxuiMenuMetrics::FromSystem (UINT dpi)
{
    constexpr UINT     kBaseDpi               = 96;
    constexpr int      kDefaultMenuHeightDip  = 19;   // NONCLIENTMETRICS default
    constexpr int      kDefaultFontEmDip      = 12;   // Segoe UI, lfHeight -12
    constexpr int      kDefaultCheckGutterDip = 15;   // SM_CXMENUCHECK default
    constexpr int      kLinePercentOfEm       = 133;   // Segoe UI cell over em
    constexpr int      kRowPadDip             = 3;   // above and below the text
    constexpr int      kSeparatorPercentOfRow = 45;
    constexpr int      kLeftPadDip            = 4;
    constexpr int      kGutterGapDip          = 16;
    constexpr int      kAccelGapDip           = 20;
    constexpr int      kMinWidthDip           = 140;
    DxuiMenuMetrics    m                      = {};
    NONCLIENTMETRICSW  ncm                    = { sizeof (ncm) };
    UINT               effective              = (dpi == 0) ? kBaseDpi : dpi;
    int                emPx                   = 0;
    int                menuH                  = 0;
    int                textRowH               = 0;



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

    m.fontPx       = (float) emPx;
    m.lineHeightPx = MulDiv (emPx, kLinePercentOfEm, 100);

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
