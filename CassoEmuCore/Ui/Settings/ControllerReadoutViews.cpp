#include "Pch.h"

#include "Ui/Settings/ControllerReadoutViews.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





static constexpr const wchar_t *  s_kpszReadoutFont   = L"Segoe UI";
static constexpr float            s_kReadoutFontDip   = 12.0f;
static constexpr float            s_kLabelBandDip     = 18.0f;
static constexpr float            s_kRingThicknessDip = 2.0f;
static constexpr float            s_kDotRadiusDip     = 6.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  SetValues
//
////////////////////////////////////////////////////////////////////////////////

void StickPositionView::SetValues (Byte pdl0, Byte pdl1)
{
    m_pdl0 = pdl0;
    m_pdl1 = pdl1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActive
//
//  Whether a controller is being read. Without one the dot is drawn muted,
//  so a centered dot is not mistaken for a stick at rest.
//
////////////////////////////////////////////////////////////////////////////////

void StickPositionView::SetActive (bool isActive)
{
    m_isActive = isActive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisLabels
//
//  What the two axes are called. A player holding the second joystick drives
//  PDL2 and PDL3, so the circle has to say so rather than always naming the
//  controller's own first two targets.
//
////////////////////////////////////////////////////////////////////////////////

void StickPositionView::SetAxisLabels (const std::wstring & horizontal, const std::wstring & vertical)
{
    m_horizontal = horizontal;
    m_vertical   = vertical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void StickPositionView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The circle fills the bounds less a band below for PDL0's label and a band
//  to the right for PDL1's, so both labels sit beside the axis they name.
//
//  THE DOT STAYS INSIDE THE CIRCLE. The two paddle values are independent, so
//  a stick pushed into a corner reads both at an end; drawn as they stand,
//  that puts the dot out past the rim. Its offset from center is scaled back
//  to the rim whenever it would reach further.
//
//  Drawn through the text renderer, whose shapes are anti-aliased; the
//  painter's circles show a staircase at this size.
//
////////////////////////////////////////////////////////////////////////////////

void StickPositionView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT      bounds    = GetBounds();
    float     band      = m_scaler.ToPxf (s_kLabelBandDip);
    float     ring      = m_scaler.ToPxf (s_kRingThicknessDip);
    float     dot       = m_scaler.ToPxf (s_kDotRadiusDip);
    float     fontPx    = m_scaler.ToPxf (s_kReadoutFontDip);
    float     width     = (float) (bounds.right - bounds.left);
    float     height    = (float) (bounds.bottom - bounds.top);
    float     diameter  = std::max (0.0f, std::min (width - band * 3.0f, height - band));
    float     radius    = diameter * 0.5f;
    float     cx        = (float) bounds.left + radius;
    float     cy        = (float) bounds.top  + radius;
    float     reach     = std::max (0.0f, radius - dot - ring);
    float     dx        = (m_pdl0 / 255.0f) * 2.0f - 1.0f;
    float     dy        = (m_pdl1 / 255.0f) * 2.0f - 1.0f;
    float     length    = std::sqrt (dx * dx + dy * dy);
    uint32_t  dotColor  = m_isActive ? theme.Accent() : theme.ForegroundDisabled();
    HRESULT   hr        = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (!IsVisible() || radius <= ring)
    {
        return;
    }

    if (length > 1.0f)
    {
        dx /= length;
        dy /= length;
    }

    hr = text.FillEllipse (cx, cy, radius, radius, theme.BackgroundElevated());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawEllipse (cx, cy, radius - ring * 0.5f, radius - ring * 0.5f, ring, theme.Border());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - radius + ring, cy, cx + radius - ring, cy, 1.0f, theme.Divider());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx, cy - radius + ring, cx, cy + radius - ring, 1.0f, theme.Divider());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (cx + reach * dx, cy + reach * dy, dot, dot, dotColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (std::format (L"{}  {}", m_horizontal, m_pdl0).c_str(),
                          (float) bounds.left, cy + radius, diameter, band,
                          theme.ForegroundMuted(), fontPx, s_kpszReadoutFont,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (m_vertical.empty() ? L"" : std::format (L"{}  {}", m_vertical, m_pdl1).c_str(),
                          cx + radius + ring * 2.0f, cy - band * 0.5f, band * 3.0f, band,
                          theme.ForegroundMuted(), fontPx, s_kpszReadoutFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLit
//
////////////////////////////////////////////////////////////////////////////////

void ButtonLightView::SetLit (bool isLit)
{
    m_isLit = isLit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void ButtonLightView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void ButtonLightView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds = GetBounds();
    float    ring   = m_scaler.ToPxf (s_kRingThicknessDip);
    float    radius = std::min ((float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top)) * 0.5f;
    float    cx     = (float) bounds.left + (bounds.right - bounds.left) * 0.5f;
    float    cy     = (float) bounds.top  + (bounds.bottom - bounds.top) * 0.5f;
    HRESULT  hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (!IsVisible() || radius <= ring)
    {
        return;
    }

    hr = text.FillEllipse (cx, cy, radius, radius, m_isLit ? theme.Accent() : theme.BackgroundElevated());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawEllipse (cx, cy, radius - ring * 0.5f, radius - ring * 0.5f, ring, m_isLit ? theme.Accent() : theme.Border());
    IGNORE_RETURN_VALUE (hr, S_OK);
}
