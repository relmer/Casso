#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/ColorKeyButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::ColorKeyButton
//
////////////////////////////////////////////////////////////////////////////////

ColorKeyButton::ColorKeyButton (std::wstring pane, ColorLegend::Pane legend) :
    m_pane   (std::move (pane)),
    m_legend (legend)
{
    m_focusable = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::GetStripBeside
//
////////////////////////////////////////////////////////////////////////////////

RECT ColorKeyButton::GetStripBeside (const RECT & slot, const DxuiDpiScaler & scaler)
{
    RECT  strip = slot;



    strip.right = std::max (strip.left, strip.right - (long) scaler.ToPx (kWidthDip));

    return strip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool ColorKeyButton::Contains (POINT point) const
{
    return m_visible && point.x >= m_boundsDip.left && point.x < m_boundsDip.right &&
           point.y >= m_boundsDip.top && point.y < m_boundsDip.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::Paint
//
//  The strip's fill and bottom hairline, as the toolbar beside it draws
//  them, then the info glyph, on the hover fill while the pointer is over it
//  and the pressed fill while its key is open.
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kInsetDip = 3.0f;
    constexpr float  kGlyphDip = DxuiToolbar::kCompactIconDip;
    HRESULT          hr        = S_OK;
    float            left      = (float) m_boundsDip.left;
    float            top       = (float) m_boundsDip.top;
    float            width     = (float) (m_boundsDip.right - m_boundsDip.left);
    float            height    = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float            inset     = m_scaler.ToPxf (kInsetDip);
    uint32_t         ink       = theme.ButtonText();



    if (!m_visible || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    painter.FillRect (left, top, width, height, theme.Background());
    painter.FillRect (left, top + height - 1.0f, width, 1.0f, theme.ContentEdge());

    if (m_isPressed || m_isHovered)
    {
        painter.FillRect (left + inset, top + inset, width - inset * 2.0f, height - inset * 2.0f,
                          m_isPressed ? theme.ButtonPressed() : theme.ButtonHover());
    }

    hr = text.DrawString (s_kpszMdl2Info, left, top, width, height, ink, m_scaler.ToPxf (kGlyphDip),
                          DxuiToolbar::kMdl2IconFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::OnKey
//
//  Space and Enter open the key or close it, as a click does.
//
////////////////////////////////////////////////////////////////////////////////

bool ColorKeyButton::OnKey (const DxuiKeyEvent & ev)
{
    bool  isPress = ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_SPACE || ev.vk == VK_RETURN) && !ev.ctrl && !ev.alt;



    if (!isPress)
    {
        return false;
    }

    if (m_onActivate)
    {
        m_onActivate();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton::OnFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyButton::OnFocusChanged (bool focused)
{
    m_isFocused = focused;

    if (m_onFocus)
    {
        m_onFocus (focused);
    }
}
