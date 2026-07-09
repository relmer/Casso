#include "Pch.h"

#include "PrinterIndicator.h"




namespace
{
    // Intrinsic size (DIP). Sits comfortably inside the command-bar band.
    constexpr int  kBodyWDp = 30;
    constexpr int  kBodyHDp = 20;

    // Fixed neutral palette for this first cut (not yet theme-driven -- the
    // review pass can pull these from CassoTheme once the look is agreed).
    constexpr uint32_t  kChassisTop = 0xFF4A4A52;
    constexpr uint32_t  kChassisBot = 0xFF2C2C31;
    constexpr uint32_t  kOutline    = 0xFF17171A;
    constexpr uint32_t  kPaper      = 0xFFF4F3EF;
    constexpr uint32_t  kSlot       = 0xFF121214;

    uint32_t  StatusCore (PrinterStatus s)
    {
        switch (s)
        {
        case PrinterStatus::Receiving: return 0xFF3FB950;   // green: printing now
        case PrinterStatus::Pending:   return 0xFFF5A623;   // amber: page waiting
        case PrinterStatus::Error:     return 0xFFE5484D;   // red:   failed
        case PrinterStatus::Idle:
        default:                       return 0xFF444B54;   // dim slate
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterIndicator::Layout
//
//  Intrinsic-size widget: boundsDip.left/top is the anchor; the body rect is
//  computed from per-DPI metrics and stored as the widget bounds. Clears the
//  hidden latch (the shell calls Hide() when there is no printer card).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterIndicator::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  w = scaler.Px (kBodyWDp);
    int  h = scaler.Px (kBodyHDp);

    m_hidden   = false;
    m_dpi      = scaler.Dpi ();
    m_bodyRect = { boundsDip.left, boundsDip.top, boundsDip.left + w, boundsDip.top + h };

    SetBounds (m_bodyRect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterIndicator::Paint
//
//  A compact printer: chassis, the paper in the feeder, the front output slot,
//  a sheet emerging from the slot, and a status LED (halo + core) whose colour
//  tracks PrinterStatus. Draws nothing while hidden or unsized.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterIndicator::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    DxuiDpiScaler  scaler;
    float          x = (float) m_bodyRect.left;
    float          y = (float) m_bodyRect.top;
    float          w = (float) (m_bodyRect.right  - m_bodyRect.left);
    float          h = (float) (m_bodyRect.bottom - m_bodyRect.top);

    if (m_hidden || w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    scaler.SetDpi (m_dpi);

    // Paper standing in the rear feeder (drawn first so the chassis overlaps it).
    {
        float  pw = w * 0.6f;
        float  px = x + (w - pw) * 0.5f;

        painter.FillRect (px, y - scaler.Pxf (2.0f), pw, scaler.Pxf (5.0f), kPaper);
    }

    // Chassis.
    painter.FillGradientRect (x, y + scaler.Pxf (2.0f), w, h - scaler.Pxf (2.0f), kChassisTop, kChassisBot);
    painter.OutlineRect      (x, y + scaler.Pxf (2.0f), w, h - scaler.Pxf (2.0f), 1.0f, kOutline);

    // Front output slot (dark horizontal line) about two-thirds down.
    {
        float  sy = y + h * 0.60f;

        painter.FillRect (x + scaler.Pxf (4.0f), sy, w - scaler.Pxf (8.0f), scaler.Pxf (1.5f), kSlot);
    }

    // Sheet emerging from the output slot.
    {
        float  pw = w * 0.5f;
        float  px = x + (w - pw) * 0.5f;

        painter.FillRect (px, y + h - scaler.Pxf (1.0f), pw, scaler.Pxf (4.0f), kPaper);
    }

    // Status LED (halo + core) at the front-right corner.
    {
        uint32_t  core = StatusCore (m_status);
        uint32_t  halo = (core & 0x00FFFFFFu) | 0x66000000u;
        float     r    = scaler.Pxf (2.4f);
        float     cx   = x + w - scaler.Pxf (5.0f);
        float     cy   = y + h - scaler.Pxf (5.0f);

        painter.FillCircleApprox (cx, cy, r * 1.9f, halo);
        painter.FillCircleApprox (cx, cy, r,        core);
    }
}
