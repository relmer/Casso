#include "Pch.h"

#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButton::SetLabel
//
//  Stores the label with a single ampersand stripped (Win32 accelerator
//  convention). Captures the character after the first single `&` as
//  the lowercase accelerator key. `&&` is preserved as a literal `&`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiButton::SetLabel (const std::wstring & label)
{
    std::wstring  out;
    wchar_t       accel = 0;
    size_t        i     = 0;


    out.reserve (label.size());

    while (i < label.size())
    {
        if (label[i] == L'&' && i + 1 < label.size())
        {
            if (label[i + 1] == L'&')
            {
                out.push_back (L'&');
                i += 2;
                continue;
            }
            if (accel == 0)
            {
                accel = (wchar_t) towlower (label[i + 1]);
            }
            out.push_back (label[i + 1]);
            i += 2;
            continue;
        }
        out.push_back (label[i]);
        ++i;
    }

    m_label       = std::move (out);
    m_accelerator = accel;
}



bool DxuiButton::HitTest (int x, int y) const
{
    if (!m_visible || !m_enabled)
    {
        return false;
    }

    return x >= m_rect.left && x < m_rect.right && y >= m_rect.top && y < m_rect.bottom;
}


void DxuiButton::SetMouse (int x, int y, bool down)
{
    if (!m_visible || !m_enabled)
    {
        m_hover   = false;
        m_pressed = false;
        return;
    }

    m_hover   = HitTest (x, y);
    m_pressed = m_hover && down;
}


void DxuiButton::Click ()
{
    if (!m_visible || !m_enabled)
    {
        return;
    }

    if (m_click)
    {
        m_click();
    }
}


bool DxuiButton::OnKey (WPARAM vk)
{
    if (!m_visible || !m_enabled || !m_focused)
    {
        return false;
    }

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        Click();
        return true;
    }

    return false;
}


void DxuiButton::Paint (DxuiPainter & painter, DxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr uint32_t  s_kFocusRingArgb = 0xFFAACCFF;
    constexpr float     s_kFocusRingPx   = 1.5f;
    constexpr float     s_kFocusInsetPx  = -2.0f;
    constexpr uint32_t  s_kDisabledMask  = 0x80FFFFFF;

    HRESULT  hr        = S_OK;
    uint32_t themeIdle    = m_useOverrides ? m_idleOverride    : theme.ButtonIdle();
    uint32_t themeHover   = m_useOverrides ? m_hoverOverride   : theme.ButtonHover();
    uint32_t themePressed = m_useOverrides ? m_pressedOverride : theme.ButtonPressed();
    uint32_t color        = m_pressed ? themePressed : (m_hover ? themeHover : themeIdle);
    uint32_t textColor    = m_useTextOverride ? m_textOverride : theme.ButtonText();
    uint32_t borderColor  = theme.ButtonBorder();
    float    fontDip      = m_scaler.Pxf (13.0f);
    float    autoBorderPx = m_scaler.Pxf (1.0f);



    if (!m_visible)
    {
        return;
    }

    if (!m_enabled)
    {
        color     = (color     & 0x00FFFFFF) | (((color     >> 24) / 2) << 24);
        textColor = (textColor & s_kDisabledMask);
    }

    painter.FillRect ((float) m_rect.left,
                      (float) m_rect.top,
                      (float) (m_rect.right  - m_rect.left),
                      (float) (m_rect.bottom - m_rect.top),
                      color);

    if (m_outlineThick > 0.0f)
    {
        painter.OutlineRect ((float) m_rect.left,
                             (float) m_rect.top,
                             (float) (m_rect.right  - m_rect.left),
                             (float) (m_rect.bottom - m_rect.top),
                             m_outlineThick,
                             m_outlineArgb);
    }
    else if (borderColor != 0)
    {
        // Themed buttons always paint a 1dip border so the
        // shape is legible against the panel background even when the
        // button fill is similar to the surface.
        painter.OutlineRect ((float) m_rect.left,
                             (float) m_rect.top,
                             (float) (m_rect.right  - m_rect.left),
                             (float) (m_rect.bottom - m_rect.top),
                             autoBorderPx,
                             borderColor);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                              (float) m_rect.left,
                                              (float) m_rect.top,
                                              (float) (m_rect.right  - m_rect.left),
                                              (float) (m_rect.bottom - m_rect.top),
                                              textColor,
                                              fontDip,
                                              L"Segoe UI",
                                              DxuiTextRenderer::HAlign::Center,
                                              DxuiTextRenderer::VAlign::Center));

    if (m_focused)
    {
        float  focusInset = m_scaler.Pxf (s_kFocusInsetPx);
        float  focusThick = m_scaler.Pxf (s_kFocusRingPx);

        painter.OutlineRect ((float) m_rect.left + focusInset,
                             (float) m_rect.top  + focusInset,
                             (float) (m_rect.right  - m_rect.left) - focusInset * 2.0f,
                             (float) (m_rect.bottom - m_rect.top)  - focusInset * 2.0f,
                             focusThick,
                             s_kFocusRingArgb);
    }
}
