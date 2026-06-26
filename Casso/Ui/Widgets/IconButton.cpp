#include "Pch.h"

#include "IconButton.h"




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool IconButton::HitTest (int x, int y) const
{
    return m_enabled &&
           x >= m_rect.left && x < m_rect.right &&
           y >= m_rect.top  && y < m_rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void IconButton::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
    if (!m_hover)
    {
        m_pressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool IconButton::OnLButtonDown (int x, int y)
{
    bool  hit = HitTest (x, y);



    if (hit)
    {
        m_pressed = true;
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Fires the click when released over the button after a press.
//
////////////////////////////////////////////////////////////////////////////////

bool IconButton::OnLButtonUp (int x, int y)
{
    bool  fire = m_pressed && HitTest (x, y);



    m_pressed = false;

    if (fire && m_click)
    {
        m_click();
    }
    return fire;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void IconButton::Paint (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme) const
{
    HRESULT   hr        = S_OK;
    float     x         = (float) m_rect.left;
    float     y         = (float) m_rect.top;
    float     w         = (float) (m_rect.right  - m_rect.left);
    float     h         = (float) (m_rect.bottom - m_rect.top);
    float     glyphDip  = m_scaler.Pxf (s_kGlyphFontDp);
    uint32_t  glyphArgb = m_enabled ? theme.navItemTextArgb : s_kDisabledFg;



    if (m_enabled && (m_hover || m_pressed))
    {
        painter.FillRect (x, y, w, h, theme.navHoverArgb);
        glyphArgb = theme.linkArgb;
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_glyph,
                                              x,
                                              y,
                                              w,
                                              h,
                                              glyphArgb,
                                              glyphDip,
                                              s_kpszMdl2Family,
                                              DwriteTextRenderer::HAlign::Center,
                                              DwriteTextRenderer::VAlign::Center,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              false));
}
