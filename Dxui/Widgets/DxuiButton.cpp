#include "Pch.h"

#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous color helpers
//
//  WCAG relative-luminance / contrast math + accent darkening, kept local
//  so the Primary variant can derive a fill that clears 4.5:1 against its
//  white label from any theme accent (bright accents only score ~1.3:1).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    float ChannelLinear (uint32_t c8)
    {
        float  s = (float) (c8 & 0xFFu) / 255.0f;

        return (s <= 0.03928f) ? (s / 12.92f)
                               : std::pow ((s + 0.055f) / 1.055f, 2.4f);
    }


    float RelativeLuminance (uint32_t argb)
    {
        float  r = ChannelLinear (argb >> 16);
        float  g = ChannelLinear (argb >> 8);
        float  b = ChannelLinear (argb);

        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }


    float ContrastRatio (uint32_t a, uint32_t b)
    {
        float  la = RelativeLuminance (a);
        float  lb = RelativeLuminance (b);
        float  hi = (la > lb) ? la : lb;
        float  lo = (la > lb) ? lb : la;

        return (hi + 0.05f) / (lo + 0.05f);
    }


    uint32_t AccentForWhiteContrast (uint32_t accent, float minRatio)
    {
        constexpr uint32_t  s_kWhite    = 0xFFFFFFFFu;
        constexpr int       s_kMaxSteps = 32;
        constexpr float     s_kStepMul  = 0.9f;

        uint32_t  cur = accent;
        int       i   = 0;

        for (i = 0; i < s_kMaxSteps && ContrastRatio (cur, s_kWhite) < minRatio; ++i)
        {
            uint32_t  r = (uint32_t) (((cur >> 16) & 0xFFu) * s_kStepMul);
            uint32_t  g = (uint32_t) (((cur >>  8) & 0xFFu) * s_kStepMul);
            uint32_t  b = (uint32_t) (( cur        & 0xFFu) * s_kStepMul);

            cur = (cur & 0xFF000000u) | (r << 16) | (g << 8) | b;
        }

        return cur;
    }


    uint32_t Lighten (uint32_t argb, float f)
    {
        uint32_t  r = (uint32_t) (((argb >> 16) & 0xFFu) + (255 - ((argb >> 16) & 0xFFu)) * f);
        uint32_t  g = (uint32_t) (((argb >>  8) & 0xFFu) + (255 - ((argb >>  8) & 0xFFu)) * f);
        uint32_t  b = (uint32_t) (( argb        & 0xFFu) + (255 - ( argb        & 0xFFu)) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }


    uint32_t Darken (uint32_t argb, float f)
    {
        uint32_t  r = (uint32_t) (((argb >> 16) & 0xFFu) * f);
        uint32_t  g = (uint32_t) (((argb >>  8) & 0xFFu) * f);
        uint32_t  b = (uint32_t) (( argb        & 0xFFu) * f);

        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }
}





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

    if (m_variant == Variant::Primary)
    {
        // White label on this fill, so the fill must clear the WCAG
        // 1.4.3 text-contrast threshold (4.5:1) against white.
        idle      = AccentForWhiteContrast (theme.Accent(), s_kPrimaryTextRatio);
        hover     = Lighten (idle, s_kPrimaryHover);
        pressed   = Darken  (idle, s_kPrimaryPressed);
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
                                              L"Segoe UI",
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
