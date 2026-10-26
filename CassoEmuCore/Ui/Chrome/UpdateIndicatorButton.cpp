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
//  UpdateIndicatorButton::GetTipAnchorPx
//
//  The rect the tip is placed against: the indicator's bounds grown by the
//  pointer's image above and below its hot spot, plus `gapPx`, all halved.
//  The full reach keeps the tip clear of the pointer wherever it rests over
//  the indicator but leaves it looking detached; half sits it closer while
//  keeping its dwell and fade.
//
////////////////////////////////////////////////////////////////////////////////

RECT UpdateIndicatorButton::GetTipAnchorPx (const RECT & boundsPx, const DxuiTooltip::PointerExtent & pointer, int gapPx)
{
    DxuiTooltip::PointerExtent  half = { pointer.aboveHotspotPx / 2, pointer.belowHotspotPx / 2 };



    return DxuiTooltip::MakePointerClearAnchor (boundsPx, half, gapPx / 2);
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
//  DxuiShimmer's band and glints across the arrow and text: the band lights
//  both, drawn again in its color, and the glints span only the text's width.
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
    constexpr float    kGlintDip     = 2.5f;
    constexpr wchar_t  kTextFamily[] = L"Segoe UI";
    constexpr wchar_t  kMdl2Family[] = L"Segoe MDL2 Assets";



    float  glyphW  = m_scaler.ToPxf ((float) UpdateIndicatorModel::kGlyphColumnDip);
    float  glyphPx = m_scaler.ToPxf (UpdateIndicatorModel::kGlyphFontDip);



    DxuiShimmer::Paint (text, x, y, w, h, x + glyphW, w - glyphW, fontPx, m_scaler.ToPxf (kGlintDip), progress, m_sweepGlints,
                        [&] (uint32_t argb)
                        {
                            HRESULT  hr = S_OK;

                            hr = text.DrawString (s_kpszMdl2Download, x, y, glyphW, h, argb, glyphPx,
                                                  kMdl2Family, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
                            IGNORE_RETURN_VALUE (hr, S_OK);

                            hr = text.DrawString (m_text.c_str(), x + glyphW, y, w, h, argb, fontPx,
                                                  kTextFamily, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                            IGNORE_RETURN_VALUE (hr, S_OK);
                        });
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



    if (m_isAnimated && !m_isQuiet && m_showsText && m_visible)
    {
        sweep = UpdateIndicatorModel::GetSweepProgress (nowMs - m_shownAtMs);
    }

    // A new sweep scatters its glints afresh.
    if (sweep.has_value() && !m_sweep.has_value() && m_random)
    {
        m_sweepGlints = DxuiShimmer::MakeSweepGlints (m_random);
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



    if (m_isAnimated && !m_isQuiet && m_showsText && m_visible)
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





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton::OnPointer
//
//  The pointer moved, or left the window (isInside false). Entering raises
//  the tooltip and, with animations on, starts a sweep at once unless one is
//  already running -- a sweep is never restarted midway. The hover sweep
//  restarts the schedule, so the next periodic one is a full period later.
//  Leaving takes the tooltip down immediately rather than after a dwell: a
//  tip over a control the pointer has left explains nothing.
//
////////////////////////////////////////////////////////////////////////////////

UpdateIndicatorButton::PointerResult UpdateIndicatorButton::OnPointer (bool isInside, int64_t nowMs)
{
    PointerResult  result;
    bool           isSweeping = false;



    result.repaint = SetHovered (isInside);
    result.showTip = result.repaint && isInside;
    result.hideTip = result.repaint && !isInside;

    if (result.showTip && m_isAnimated && !m_isQuiet && m_showsText && m_visible)
    {
        isSweeping = UpdateIndicatorModel::GetSweepProgress (nowMs - m_shownAtMs).has_value();

        if (!isSweeping)
        {
            m_shownAtMs = UpdateIndicatorModel::GetHoverClockStart (nowMs);
        }
    }

    return result;
}