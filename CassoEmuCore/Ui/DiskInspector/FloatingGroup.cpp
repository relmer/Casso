#include "Pch.h"

#include "Ui/DiskInspector/FloatingGroup.h"





static constexpr uint32_t  s_kFillAlpha   = 0xD8000000u;
static constexpr uint32_t  s_kShadowArgb  = 0x40000000u;
static constexpr float     s_kShadowDip   = 2.0f;
static constexpr float     s_kRadiusDip   = 20.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingGroup::Layout
//
//  The capsule's bounds only; its owner places the controls in it.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingGroup::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingGroup::Paint
//
//  A soft shadow, the capsule in the elevated surface's color at a little
//  under full opacity, so the platter is faintly visible through it, its border,
//  and then the controls.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  x      = static_cast<float> (m_boundsDip.left);
    float  y      = static_cast<float> (m_boundsDip.top);
    float  w      = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float  h      = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float  radius = std::min (m_scaler.ToPxf (s_kRadiusDip), std::min (w, h) / 2.0f);
    float  shadow = m_scaler.ToPxf (s_kShadowDip);



    if (w > 0 && h > 0)
    {
        painter.FillRoundedRect    (x + shadow, y + shadow, w, h, radius, s_kShadowArgb);
        painter.FillRoundedRect    (x, y, w, h, radius, (theme.BackgroundElevated() & 0x00FFFFFFu) | s_kFillAlpha);
        painter.OutlineRoundedRect (x, y, w, h, radius, m_scaler.ToPxf (1.0f), theme.Border());
        DxuiPanel::Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingGroup::OnMouse
//
//  The controls first; then any press, release or move over the capsule
//  stops here.
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingGroup::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = DxuiPanel::OnMouse (ev);



    return isHandled || (ev.kind != DxuiMouseEventKind::Leave && Contains (ev.positionDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingGroup::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingGroup::Contains (POINT pointPx) const
{
    return IsVisible() && PtInRect (&m_boundsDip, pointPx) != FALSE;
}
