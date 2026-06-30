#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiButton.h"
#include "Theme/DxuiColor.h"





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

    return x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom;
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


void DxuiButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float     s_kFocusRingPx      = 1.5f;
    constexpr float     s_kFocusInsetPx     = -2.0f;
    constexpr float     s_kEmphasisPx       = 1.5f;
    constexpr uint32_t  s_kDisabledMask     = 0x80FFFFFF;
    constexpr uint32_t  s_kPrimaryTextArgb  = 0xFFFFFFFFu;
    constexpr float     s_kPrimaryTextRatio = 4.5f;
    constexpr float     s_kPrimaryHover     = 0.12f;
    constexpr float     s_kPrimaryPressed   = 0.82f;
    constexpr float     s_kLinkHover        = 0.25f;

    HRESULT  hr           = S_OK;
    uint32_t idle         = theme.ButtonIdle();
    uint32_t hover        = theme.ButtonHover();
    uint32_t pressed      = theme.ButtonPressed();
    uint32_t textColor    = theme.ButtonText();
    uint32_t borderColor  = theme.ButtonBorder();
    uint32_t color        = 0;
    float    fontDip      = m_scaler.Pxf (13.0f);
    float    autoBorderPx = m_scaler.Pxf (1.0f);



    if (!m_visible)
    {
        return;
    }

    //
    //  Link variant: accent-colored text, no fill / border, left-aligned
    //  (a clickable hyperlink). The consumer wires SetClick to open the URL.
    //
    if (m_variant == Variant::Link)
    {
        uint32_t  linkColor = theme.Accent();

        if (m_hover || m_pressed)
        {
            linkColor = DxuiColor::Lighten (linkColor, s_kLinkHover);
        }

        if (!m_enabled)
        {
            linkColor = (linkColor & s_kDisabledMask);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                                  (float) m_boundsDip.left,
                                                  (float) m_boundsDip.top,
                                                  (float) (m_boundsDip.right  - m_boundsDip.left),
                                                  (float) (m_boundsDip.bottom - m_boundsDip.top),
                                                  linkColor,
                                                  fontDip,
                                                  DxuiTheme::kBodyFace,
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));

        if (m_focused)
        {
            painter.OutlineRect ((float) m_boundsDip.left + m_scaler.Pxf (s_kFocusInsetPx),
                                 (float) m_boundsDip.top  + m_scaler.Pxf (s_kFocusInsetPx),
                                 (float) (m_boundsDip.right  - m_boundsDip.left) - m_scaler.Pxf (s_kFocusInsetPx) * 2.0f,
                                 (float) (m_boundsDip.bottom - m_boundsDip.top)  - m_scaler.Pxf (s_kFocusInsetPx) * 2.0f,
                                 m_scaler.Pxf (s_kFocusRingPx),
                                 theme.FocusRing());
        }

        return;
    }

    if (m_variant == Variant::Primary)
    {
        // White label on this fill, so the fill must clear the WCAG
        // 1.4.3 text-contrast threshold (4.5:1) against white.
        idle      = DxuiColor::AccentForWhiteContrast (theme.Accent(), s_kPrimaryTextRatio);
        hover     = DxuiColor::Lighten (idle, s_kPrimaryHover);
        pressed   = DxuiColor::Darken  (idle, s_kPrimaryPressed);
        textColor = s_kPrimaryTextArgb;
    }

    color = m_pressed ? pressed : (m_hover ? hover : idle);

    if (!m_enabled)
    {
        color     = (color     & 0x00FFFFFF) | (((color     >> 24) / 2) << 24);
        textColor = (textColor & s_kDisabledMask);
    }

    painter.FillRect ((float) m_boundsDip.left,
                      (float) m_boundsDip.top,
                      (float) (m_boundsDip.right  - m_boundsDip.left),
                      (float) (m_boundsDip.bottom - m_boundsDip.top),
                      color);

    if (m_emphasis)
    {
        painter.OutlineRect ((float) m_boundsDip.left,
                             (float) m_boundsDip.top,
                             (float) (m_boundsDip.right  - m_boundsDip.left),
                             (float) (m_boundsDip.bottom - m_boundsDip.top),
                             m_scaler.Pxf (s_kEmphasisPx),
                             theme.HoverBackground());
    }
    else if (borderColor != 0)
    {
        // Themed buttons always paint a 1dip border so the
        // shape is legible against the panel background even when the
        // button fill is similar to the surface.
        painter.OutlineRect ((float) m_boundsDip.left,
                             (float) m_boundsDip.top,
                             (float) (m_boundsDip.right  - m_boundsDip.left),
                             (float) (m_boundsDip.bottom - m_boundsDip.top),
                             autoBorderPx,
                             borderColor);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_label.c_str(),
                                              (float) m_boundsDip.left,
                                              (float) m_boundsDip.top,
                                              (float) (m_boundsDip.right  - m_boundsDip.left),
                                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                                              textColor,
                                              fontDip,
                                              DxuiTheme::kBodyFace,
                                              DxuiTextHAlign::Center,
                                              DxuiTextVAlign::Center));

    if (m_focused)
    {
        float  focusInset = m_scaler.Pxf (s_kFocusInsetPx);
        float  focusThick = m_scaler.Pxf (s_kFocusRingPx);

        painter.OutlineRect ((float) m_boundsDip.left + focusInset,
                             (float) m_boundsDip.top  + focusInset,
                             (float) (m_boundsDip.right  - m_boundsDip.left) - focusInset * 2.0f,
                             (float) (m_boundsDip.bottom - m_boundsDip.top)  - focusInset * 2.0f,
                             focusThick,
                             theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButton::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButton::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiButton::OnMouse (const DxuiMouseEvent & ev)
{
    bool  prevHover   = m_hover;
    bool  prevPressed = m_pressed;


    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouse (ev.positionDip.x, ev.positionDip.y, m_pressed);
        return m_hover != prevHover || m_pressed != prevPressed;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            SetMouse (ev.positionDip.x, ev.positionDip.y, true);
            return m_pressed;
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            bool  fire = m_pressed && HitTest (ev.positionDip.x, ev.positionDip.y);
            SetMouse (ev.positionDip.x, ev.positionDip.y, false);
            if (fire)
            {
                Click();
            }
            return fire;
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButton::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiButton::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}
