#include "Pch.h"

#include "Ui/DiskInspector/InspectorMarquee.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::InspectorMarquee
//
//  The dashes march a pixel every kStepMs, a dash pair every two dashes'
//  worth of steps, which reads as motion without flicker.
//
////////////////////////////////////////////////////////////////////////////////

InspectorMarquee::InspectorMarquee (const DiskInspectorPalette & palette, float dashPx, float thicknessPx, int64_t nowMs) :
    m_dashPx      (std::max (dashPx, 1.0f)),
    m_thicknessPx (thicknessPx),
    m_colorA      (palette.colors.selection),
    m_colorB      (DiskInspectorPalette::GetTextColorOn (palette.colors.selection))
{
    m_phasePx = static_cast<float> ((nowMs / kStepMs) % static_cast<int64_t> (std::ceil (m_dashPx * 2.0f)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::DrawLine
//
////////////////////////////////////////////////////////////////////////////////

float InspectorMarquee::DrawLine (IDxuiPainter & painter, float x0, float y0, float x1, float y1, float startPx) const
{
    float  length = std::hypot (x1 - x0, y1 - y0);
    float  s      = startPx;
    float  next   = 0.0f;
    float  k      = 0.0f;



    while (length > 0.0f && s < startPx + length)
    {
        k    = std::floor ((s + m_phasePx) / m_dashPx);
        next = std::min ((k + 1.0f) * m_dashPx - m_phasePx, startPx + length);

        painter.DrawLine (x0 + (x1 - x0) * (s - startPx) / length,    y0 + (y1 - y0) * (s - startPx) / length,
                          x0 + (x1 - x0) * (next - startPx) / length, y0 + (y1 - y0) * (next - startPx) / length,
                          m_thicknessPx, (std::abs (static_cast<int64_t> (k)) % 2 == 0) ? m_colorA : m_colorB);
        s = next;
    }

    return startPx + length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::Dim
//
////////////////////////////////////////////////////////////////////////////////

uint32_t InspectorMarquee::Dim (const DiskInspectorPalette & palette, uint32_t argb)
{
    return DxuiColor::Lerp (argb, palette.background, 0.5f);
}
