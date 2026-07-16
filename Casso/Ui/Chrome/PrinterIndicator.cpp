#include "Pch.h"

#include "PrinterIndicator.h"




namespace
{
    // ImageWriter II platinum chassis.
    constexpr uint32_t  kPlatTop  = 0xFFEDE9DD;
    constexpr uint32_t  kPlatBot  = 0xFFCAC5B5;
    constexpr uint32_t  kEdge     = 0xFF8C8879;
    constexpr uint32_t  kSmoked   = 0xFF42454B;   // smoked paper cover / hood
    constexpr uint32_t  kSlot     = 0xFF2A2A2C;   // paper exit slit
    constexpr uint32_t  kPanel    = 0xFFB4AF9F;   // recessed front control panel
    constexpr uint32_t  kPaper    = 0xFFFBFBF6;   // fanfold paper
    constexpr uint32_t  kPaperFld = 0xFFD7D5CB;   // fold / edge shading
    constexpr uint32_t  kHole     = 0xFF74716A;   // tractor sprocket holes
    constexpr uint32_t  kShadow   = 0x55101012;   // soft ground shadow

    uint32_t  StatusCore (PrinterStatus s)
    {
        switch (s)
        {
        case PrinterStatus::Receiving: return 0xFF3FD35A;   // green: printing now
        case PrinterStatus::Pending:   return 0xFFF5A623;   // amber: page waiting
        case PrinterStatus::Error:     return 0xFFE5484D;   // red:   failed
        case PrinterStatus::Idle:
        default:                       return 0xFF3B7A46;   // dim green: powered, idle
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterIndicator::Layout
//
//  Honours the caller-supplied rect verbatim (the shell computes the fitted
//  size + shelf position). Clears the hidden latch; the shell calls Hide()
//  when the machine has no printer card.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterIndicator::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_hidden   = false;
    m_dpi      = scaler.Dpi();
    m_bodyRect = boundsDip;

    SetBounds (m_bodyRect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterIndicator::Paint
//
//  A small skeuomorphic Apple ImageWriter II: fanfold paper (with tractor
//  sprocket holes) rising from the platen, a wide platinum chassis with a
//  smoked paper cover, a front print slot with a sheet emerging, and a front
//  control-panel status light whose colour tracks PrinterStatus. Draws nothing
//  while hidden or unsized.
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

    const float  bodyTop = y + h * 0.32f;          // chassis top
    const float  bodyBot = y + h;                  // chassis bottom
    const float  bodyH   = bodyBot - bodyTop;

    // --- Fanfold paper rising from the platen (drawn first; the chassis + hood
    //     overlap its lower edge so it reads as feeding into the printer). ---
    {
        float  paperW = w * 0.56f;
        float  paperX = x + (w - paperW) * 0.5f;
        float  paperB = bodyTop + bodyH * 0.20f;
        float  holeR  = scaler.Pxf (0.9f);
        float  step   = scaler.Pxf (3.4f);
        float  inset  = scaler.Pxf (1.8f);

        painter.FillRect (paperX, y, paperW, paperB - y, kPaper);
        painter.FillRect (paperX, y + (paperB - y) * 0.5f, paperW, scaler.Pxf (1.0f), kPaperFld);

        for (float hy = y + step; hy < paperB - step * 0.5f; hy += step)
        {
            painter.FillCircleApprox (paperX + inset,          hy, holeR, kHole);
            painter.FillCircleApprox (paperX + paperW - inset, hy, holeR, kHole);
        }
    }

    // --- Platinum chassis. ---
    painter.FillGradientRect (x, bodyTop, w, bodyH, kPlatTop, kPlatBot);
    painter.OutlineRect      (x, bodyTop, w, bodyH, 1.0f, kEdge);

    // Smoked paper cover / hood across the top of the chassis (where the paper
    // enters). A thin platinum lip sits just below it.
    painter.FillRect (x + scaler.Pxf (1.5f), bodyTop + scaler.Pxf (1.5f),
                      w - scaler.Pxf (3.0f), bodyH * 0.30f, kSmoked);

    // --- Front print slot + the printed sheet emerging over the front. ---
    {
        float  slotY = bodyTop + bodyH * 0.52f;
        float  ftW   = w * 0.46f;
        float  ftX   = x + (w - ftW) * 0.42f;

        painter.FillRect (ftX, slotY, ftW, bodyBot - slotY - scaler.Pxf (2.0f), kPaper);
        painter.FillRect (x + scaler.Pxf (3.0f), slotY, w - scaler.Pxf (6.0f), scaler.Pxf (1.4f), kSlot);
    }

    // --- Front-right control panel + status light. ---
    {
        float     panW = scaler.Pxf (13.0f);
        float     panH = bodyH * 0.30f;
        float     panX = x + w - panW - scaler.Pxf (3.0f);
        float     panY = bodyTop + bodyH * 0.60f;
        uint32_t  core = StatusCore (m_status);
        uint32_t  halo = (core & 0x00FFFFFFu) | 0x66000000u;
        float     r    = scaler.Pxf (1.9f);
        float     cx   = panX + panW * 0.5f;
        float     cy   = panY + panH * 0.5f;

        painter.FillRect         (panX, panY, panW, panH, kPanel);
        painter.FillCircleApprox (cx, cy, r * 1.9f, halo);
        painter.FillCircleApprox (cx, cy, r,        core);
    }

    // Soft ground shadow along the chassis foot.
    painter.FillRect (x + scaler.Pxf (2.0f), bodyBot - scaler.Pxf (1.0f),
                      w - scaler.Pxf (4.0f), scaler.Pxf (1.5f), kShadow);
}
