#include "Pch.h"

#include "Ui/Chrome/UpdateIndicatorButton.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Chrome/UpdateIndicatorModel.h"





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
//  row reads as one strip. The arrow and the text take the accent color.
//  The text is measured here, where a renderer exists, and right-aligned
//  against the system buttons with the arrow directly before it, so the
//  layout's width estimate only ever adds air on the left. Without room for
//  text the arrow sits alone, centered in its column.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateIndicatorButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float    kGlyphDip     = UpdateIndicatorModel::kGlyphFontDip;
    constexpr wchar_t  kMdl2Family[] = L"Segoe MDL2 Assets";
    constexpr wchar_t  kTextFamily[] = L"Segoe UI";



    RECT     bounds      = GetBounds();
    float    x           = (float) m_scaler.ToPx (bounds.left);
    float    y           = (float) m_scaler.ToPx (bounds.top);
    float    w           = (float) m_scaler.ToPx (bounds.right  - bounds.left);
    float    h           = (float) m_scaler.ToPx (bounds.bottom - bounds.top);
    float    fontPx      = m_scaler.ToPxf (UpdateIndicatorModel::kFontDip);
    float    glyphW      = m_scaler.ToPxf ((float) UpdateIndicatorModel::kGlyphColumnDip);
    float    padPx       = m_scaler.ToPxf ((float) UpdateIndicatorModel::kTextPadDip);
    float    textW       = 0.0f;
    float    textH       = 0.0f;
    float    textX       = 0.0f;
    HRESULT  hr          = S_OK;
    bool     isArrowOnly = false;



    BAIL_OUT_IF (!m_visible, S_OK);

    if (m_isHovered || m_isPressed)
    {
        painter.FillRect (x, y, w, h, m_isPressed ? theme.SystemButtonPressed() : theme.SystemButtonHover());
    }

    isArrowOnly = !m_showsText || m_text.empty();

    if (isArrowOnly)
    {
        hr = text.DrawString (s_kpszMdl2Download, x, y, w, h, theme.Accent(), m_scaler.ToPxf (kGlyphDip),
                              kMdl2Family, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    BAIL_OUT_IF (isArrowOnly, S_OK);

    hr = text.MeasureString (m_text.c_str(), fontPx, kTextFamily, textW, textH);

    if (FAILED (hr))
    {
        textW = w - glyphW - padPx;
    }

    textW = std::min (textW, w - glyphW - padPx);
    textX = x + w - padPx - textW;

    hr = text.DrawString (s_kpszMdl2Download, textX - glyphW, y, glyphW, h, theme.Accent(), m_scaler.ToPxf (kGlyphDip),
                          kMdl2Family, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (m_text.c_str(), textX, y, textW + padPx, h, theme.Accent(), fontPx,
                          kTextFamily, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_sweep.has_value())
    {
        PaintShimmer (text, textX - glyphW, y, textW + glyphW, h, fontPx, *m_sweep);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::PaintShimmer
//
//  A bright band sweeping left to right across the arrow and text, leaning
//  45 degrees like light off glass: the text is drawn again in white,
//  clipped to the band one thin horizontal slice at a time, each slice
//  shifted right by its height above the baseline. At mid-sweep a small
//  four-point glint flares at the band's top.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateIndicatorButton::PaintShimmer (
    IDxuiTextRenderer   & text,
    float                 x,
    float                 y,
    float                 w,
    float                 h,
    float                 fontPx,
    float                 progress) const
{
    constexpr int       kSlices       = 8;
    constexpr float     kBandFraction = 0.22f;
    constexpr uint32_t  kHighlight    = 0xFFFFFFFF;
    constexpr float     kGlintDip     = 3.0f;
    constexpr float     kGlintFrom    = 0.35f;
    constexpr float     kGlintTo      = 0.65f;
    constexpr wchar_t   kTextFamily[] = L"Segoe UI";
    constexpr wchar_t   kMdl2Family[] = L"Segoe MDL2 Assets";



    float       bandW     = w * kBandFraction;
    float       sliceH    = h / (float) kSlices;
    float       travel    = w + bandW + h;
    float       left      = x - bandW - h + travel * progress;
    float       sliceLeft = 0.0f;
    float       glyphW    = m_scaler.ToPxf ((float) UpdateIndicatorModel::kGlyphColumnDip);
    float       glint     = 0.0f;
    float       cx        = 0.0f;
    float       cy        = 0.0f;
    float       r         = 0.0f;
    HRESULT     hr        = S_OK;
    int         i         = 0;
    DxuiPointF  star[8]   = {};



    for (i = 0; i < kSlices; i++)
    {
        sliceLeft = left + (float) (kSlices - 1 - i) * sliceH;

        hr = text.PushClipRect (sliceLeft, y + sliceH * (float) i, bandW, sliceH);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (s_kpszMdl2Download, x, y, glyphW, h, kHighlight, m_scaler.ToPxf (UpdateIndicatorModel::kGlyphFontDip),
                              kMdl2Family, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (m_text.c_str(), x + glyphW, y, w, h, kHighlight, fontPx,
                              kTextFamily, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (progress > kGlintFrom && progress < kGlintTo)
    {
        glint = 1.0f - std::abs ((progress - 0.5f) / (kGlintTo - 0.5f));
        r     = m_scaler.ToPxf (kGlintDip) * glint;
        cx    = left + h + bandW * 0.5f;
        cy    = y + h * 0.25f;

        star[0] = { cx,              cy - r * 2.0f };
        star[1] = { cx + r * 0.4f,   cy - r * 0.4f };
        star[2] = { cx + r * 2.0f,   cy };
        star[3] = { cx + r * 0.4f,   cy + r * 0.4f };
        star[4] = { cx,              cy + r * 2.0f };
        star[5] = { cx - r * 0.4f,   cy + r * 0.4f };
        star[6] = { cx - r * 2.0f,   cy };
        star[7] = { cx - r * 0.4f,   cy - r * 0.4f };

        hr = text.FillPolygon (star, std::size (star), kHighlight);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::TickShimmer
//
//  Advances the shimmer to `nowMs`. True when the indicator looks different
//  from its last paint -- every frame of a sweep, and the one frame after it
//  ends -- so the shell asks for a present only then, and the GPU rests
//  between sweeps.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateIndicatorButton::TickShimmer (int64_t nowMs)
{
    std::optional<float>  sweep;
    bool                  changed = false;



    if (m_isAnimated && m_showsText && m_visible)
    {
        sweep = UpdateIndicatorModel::GetSweepProgress (nowMs - m_shownAtMs);
    }

    changed = sweep.has_value() || m_sweep.has_value();
    m_sweep = sweep;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::GetMsUntilShimmer
//
//  When the idle loop next has to wake for the shimmer; nothing when the
//  indicator does not shimmer at all.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int64_t> UpdateIndicatorButton::GetMsUntilShimmer (int64_t nowMs) const
{
    std::optional<int64_t>  until;



    if (m_isAnimated && m_showsText && m_visible)
    {
        until = UpdateIndicatorModel::GetMsUntilSweep (nowMs - m_shownAtMs);
    }

    return until;
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
