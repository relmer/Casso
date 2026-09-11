#include "Pch.h"

#include "PrinterStatusLed.h"




static constexpr int  s_kBaseDpi = 96;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusLed::GetStatusCoreColor
//
//  Same mapping the standalone indicator used, so the light keeps its
//  meaning across the move into the toolbar. Event-only: no LED at all while
//  idle, and the lit states run bright, since dim colors disappear against
//  the themed strip. 0 == unlit, which is why Idle keeps the initializer.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PrinterStatusLed::GetStatusCoreColor (PrinterStatus status)
{
    uint32_t  core = 0;



    switch (status)
    {
    case PrinterStatus::Receiving: core = 0xFF4CE96A; break;   // bright green: printing now
    case PrinterStatus::Pending:   core = 0xFFFFB938; break;   // bright amber: page waiting
    case PrinterStatus::Error:     core = 0xFFFF5257; break;   // bright red:   failed
    case PrinterStatus::Idle:
    default:                                          break;   // off: powered + idle
    }

    return core;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusLed::Paint
//
////////////////////////////////////////////////////////////////////////////////

void PrinterStatusLed::Paint (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core)
{
    float     r    = 2.0f * (float) dpi / (float) s_kBaseDpi;
    uint32_t  halo = (core & 0x00FFFFFFu) | 0x80000000u;



    // core == 0 is the idle state: paint nothing at all, so an idle printer
    // shows no light rather than a dark dot.
    if (core != 0)
    {
        painter.FillCircleApprox (cx, cy, r * 1.8f, halo);
        painter.FillCircleApprox (cx, cy, r,        core);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusLed::MakeDecoration
//
//  The light rides the glyph's top-right corner. The icon box is in the
//  fractional pixels the glyph was drawn at, and its size is 15 dp scaled,
//  so the DPI comes back out of it exactly at every common scale.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::DecorationFn PrinterStatusLed::MakeDecoration()
{
    return [this] (IDxuiPainter & painter, const IDxuiTheme & theme, const DxuiToolbarIconBox & icon, bool collapsed)
    {
        UINT  dpi = (UINT) (icon.size * (float) s_kBaseDpi / 15.0f + 0.5f);

        UNREFERENCED_PARAMETER (theme);
        UNREFERENCED_PARAMETER (collapsed);

        Paint (painter, icon.x + icon.size + 1.0f,
               icon.top + icon.rowH * 0.5f - icon.size * 0.48f, dpi,
               GetStatusCoreColor (m_status));
    };
}
