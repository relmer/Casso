#include "Pch.h"

#include "Ui/Chrome/UpdateIndicatorButton.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::UpdateIndicatorButton
//
//  Hidden until the shell knows of a release to offer.
//
////////////////////////////////////////////////////////////////////////////////

UpdateIndicatorButton::UpdateIndicatorButton()
{
    m_focusable = false;
    SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::ContainsDip
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorButton::ContainsDip (POINT pointDip) const
{
    RECT  bounds = GetBounds();



    return IsVisible() &&
           pointDip.x >= bounds.left && pointDip.x < bounds.right &&
           pointDip.y >= bounds.top  && pointDip.y < bounds.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::SetHovered
//
//  True when the state changed, so the caller knows to repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorButton::SetHovered (bool isHovered)
{
    bool  isChanged = isHovered != m_isHovered;



    m_isHovered = isHovered;

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::SetPressed
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorButton::SetPressed (bool isPressed)
{
    bool  isChanged = isPressed != m_isPressed;



    m_isPressed = isPressed;

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::Layout
//
////////////////////////////////////////////////////////////////////////////////

void UpdateIndicatorButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::Paint
//
//  The same hover and press fills as the system buttons beside it, so the
//  row reads as one strip; the glyph takes the accent color, which is what
//  makes it noticeable without a badge or animation.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateIndicatorButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float    kGlyphDip     = 14.0f;
    constexpr wchar_t  kMdl2Family[] = L"Segoe MDL2 Assets";



    RECT     bounds = GetBounds();
    float    x      = (float) m_scaler.ToPx (bounds.left);
    float    y      = (float) m_scaler.ToPx (bounds.top);
    float    w      = (float) m_scaler.ToPx (bounds.right  - bounds.left);
    float    h      = (float) m_scaler.ToPx (bounds.bottom - bounds.top);
    HRESULT  hr     = S_OK;



    BAIL_OUT_IF (!m_visible, S_OK);

    if (m_isHovered || m_isPressed)
    {
        painter.FillRect (x, y, w, h, m_isPressed ? theme.SystemButtonPressed() : theme.SystemButtonHover());
    }

    hr = text.DrawString (s_kpszMdl2Download,
                          x,
                          y,
                          w,
                          h,
                          theme.Accent(),
                          m_scaler.ToPxf (kGlyphDip),
                          kMdl2Family,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::ClassifyHit
//
//  Client, not caption: a press here is a click, not the start of a drag.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind UpdateIndicatorButton::ClassifyHit (POINT clientDip) const
{
    (void) clientDip;

    return DxuiHitTestKind::Client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::GetAccessibleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateIndicatorButton::GetAccessibleName() const
{
    return m_toolTip;
}
