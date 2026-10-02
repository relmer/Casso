#include "Pch.h"

#include "Ui/Chrome/TapeDeckWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
//  A machine with no cassette port shows no tape UI at all. Layout clears it.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::Hide()
{
    m_captionRect = {};
    m_nameRect    = {};
    m_bandRect    = {};
    m_railRect    = {};
    m_readoutRect = {};
    m_hover       = TapeDeckRegion::None;
    m_hidden      = true;

    for (RECT & button : m_buttons)
    {
        button = {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateHover
//
//  Returns whether the hovered region changed, so the caller knows to repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::UpdateHover (int x, int y)
{
    TapeDeckRegion  region = HitTest (x, y);
    bool            moved  = region != m_hover;



    // Arriving on the name scrolls a long one at once rather than after the
    // hold, as the drive widgets do.
    if (moved && region == TapeDeckRegion::Name)
    {
        m_marqueeStartMs = GetNowMs();
    }

    m_hover = region;

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  boundsDip.left / top is the anchor; the size is intrinsic, as with the
//  drive widgets.
//
//      TAPE  [ adventure.wav       ]  [<<][>][#][o][^]
//            [=======.............]  1:23 / 4:56
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UINT  dpi      = scaler.GetDpi() == 0 ? (UINT) kBaseDpi : scaler.GetDpi();
    int   x        = boundsDip.left;
    int   y        = boundsDip.top;
    int   nameX    = x + Scale (kCaptionWidthPx, dpi) + Scale (kCaptionGapXPx, dpi);
    int   nameW    = Scale (kNameWidthPx, dpi);
    int   button   = Scale (kButtonSizePx, dpi);
    int   gap      = Scale (kButtonGapPx, dpi);
    int   buttonsX = nameX + nameW + Scale (kButtonsGapXPx, dpi);



    m_hidden = false;
    m_dpi    = dpi;

    m_nameRect = { nameX, y, nameX + nameW, y + Scale (kNameHeightPx, dpi) };

    m_railRect.left   = nameX;
    m_railRect.top    = m_nameRect.bottom + Scale (kRailGapPx, dpi);
    m_railRect.right  = nameX + nameW;
    m_railRect.bottom = m_railRect.top + Scale (kRailHeightPx, dpi);

    m_captionRect.left   = x;
    m_captionRect.right  = x + Scale (kCaptionWidthPx, dpi);
    m_captionRect.bottom = m_railRect.bottom + Scale (kCaptionDescentPx, dpi);
    m_captionRect.top    = m_captionRect.bottom - Scale (kCaptionHeightPx, dpi);

    // The name band, caption included, is the control that opens the picker,
    // as a drive's band is.
    m_bandRect = { x, y, nameX + nameW, m_railRect.bottom + Scale (kBottomPadPx, dpi) };

    for (size_t i = 0; i < kButtonCount; i++)
    {
        int  left = buttonsX + (int) i * (button + gap);

        m_buttons[i] = { left, y, left + button, y + button };
    }

    m_readoutRect = { buttonsX, m_nameRect.bottom, m_buttons[kButtonCount - 1].right, m_bandRect.bottom };

    SetBounds (GetOuterRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
//  A hidden widget misses everywhere, so an invisible recorder is never
//  clickable.
//
////////////////////////////////////////////////////////////////////////////////

TapeDeckRegion TapeDeckWidget::HitTest (int x, int y) const
{
    if (m_hidden)
    {
        return TapeDeckRegion::None;
    }

    for (size_t i = 0; i < kButtonCount; i++)
    {
        if (IsPointInRect (m_buttons[i], x, y))
        {
            return GetButtonRegion (i);
        }
    }

    return IsPointInRect (m_bandRect, x, y) ? TapeDeckRegion::Name : TapeDeckRegion::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOuterRect
//
////////////////////////////////////////////////////////////////////////////////

RECT TapeDeckWidget::GetOuterRect() const
{
    RECT  outer = m_bandRect;



    outer.right  = max (outer.right,  m_buttons[kButtonCount - 1].right);
    outer.bottom = max (outer.bottom, m_readoutRect.bottom);

    return outer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRect
//
////////////////////////////////////////////////////////////////////////////////

RECT TapeDeckWidget::GetButtonRect (TapeDeckRegion region) const
{
    for (size_t i = 0; i < kButtonCount; i++)
    {
        if (GetButtonRegion (i) == region)
        {
            return m_buttons[i];
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRegionEnabled
//
//  When each control does anything, as on a real deck: record latches only on
//  a writable tape that is standing still.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::IsRegionEnabled (TapeDeckRegion region, const TapeDeckView & view)
{
    bool  hasTape   = view.transport != TapeTransport::Empty;
    bool  isMoving  = view.transport == TapeTransport::Playing || view.transport == TapeTransport::Recording;
    bool  isLoading = !view.loadingPath.empty();



    // While a tape loads, only the picker and eject (which cancels the load)
    // do anything.
    if (isLoading)
    {
        return region == TapeDeckRegion::Name || region == TapeDeckRegion::Eject;
    }

    switch (region)
    {
        case TapeDeckRegion::Name:   return true;
        case TapeDeckRegion::Rewind: return hasTape;
        case TapeDeckRegion::Play:   return view.transport == TapeTransport::Stopped;
        case TapeDeckRegion::Stop:   return isMoving;
        case TapeDeckRegion::Record: return hasTape && view.isWritable && !isMoving;
        case TapeDeckRegion::Eject:  return hasTape;
        default:                     return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatTime
//
//  Minutes and seconds, truncated, as a tape counter shows elapsed time.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TapeDeckWidget::FormatTime (double seconds)
{
    constexpr int  kSecondsPerMinute = 60;
    int            whole             = (int) max (0.0, floor (seconds));



    return std::format (L"{}:{:02}", whole / kSecondsPerMinute, whole % kSecondsPerMinute);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatReadout
//
//  Empty with no tape, so the row does not claim a length for nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TapeDeckWidget::FormatReadout (const TapeDeckView & view)
{
    if (view.transport == TapeTransport::Empty)
    {
        return {};
    }

    return FormatTime (view.positionSeconds) + L" / " + FormatTime (view.lengthSeconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetProgress
//
//  How far through the tape, 0 to 1. A recording that has run past the end
//  shows full.
//
////////////////////////////////////////////////////////////////////////////////

float TapeDeckWidget::GetProgress (const TapeDeckView & view)
{
    if (view.lengthSeconds <= 0.0)
    {
        return 0.0f;
    }

    return (float) clamp (view.positionSeconds / view.lengthSeconds, 0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDisplayName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TapeDeckWidget::GetDisplayName (const TapeDeckView & view)
{
    if (!view.loadingPath.empty())
    {
        return L"Loading " + std::filesystem::path (view.loadingPath).filename().wstring() + s_kchEllipsis;
    }

    if (view.transport == TapeTransport::Empty || view.path.empty())
    {
        return kEmptyLabel;
    }

    return std::filesystem::path (view.path).filename().wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMarqueeOffset
//
////////////////////////////////////////////////////////////////////////////////

float TapeDeckWidget::GetMarqueeOffset (int64_t nowMs, int64_t startMs, float periodPx, float speedPxPerSec)
{
    int64_t  scrollMs = 0;
    float    offset   = 0.0f;



    if (speedPxPerSec > 0.0f && periodPx > 0.0f)
    {
        scrollMs = (int64_t) (periodPx / speedPxPerSec * 1000.0f);
    }

    if (scrollMs > 0 && nowMs >= startMs && nowMs < startMs + scrollMs)
    {
        offset = periodPx * (float) (nowMs - startMs) / (float) scrollMs;
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t TapeDeckWidget::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
        std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintName
//
//  A name that fits is centered. One that does not scrolls through the row as
//  a marquee: two copies a name-plus-gap apart, clipped to the row, so as the
//  first leaves on the left the second follows it in from the right and the
//  scroll ends exactly where it began.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::PaintName (IDxuiTextRenderer & text, const std::wstring & name, uint32_t argb)
{
    HRESULT  hr       = S_OK;
    float    dipScale = (float) m_dpi / (float) kBaseDpi;
    float    fontDip  = kNameFontDip * dipScale;
    float    left     = (float) m_nameRect.left;
    float    top      = (float) m_nameRect.top;
    float    width    = (float) (m_nameRect.right - m_nameRect.left);
    float    height   = (float) (m_nameRect.bottom - m_nameRect.top);
    float    textW    = 0.0f;
    float    textH    = 0.0f;
    float    period   = 0.0f;
    float    speed    = kMarqueeSpeedDipPerSec * dipScale;
    float    offset   = 0.0f;
    bool     clipped  = false;
    int64_t  nowMs    = GetNowMs();



    hr = text.MeasureString (name.c_str(), fontDip, kFontFamily, textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (textW <= width)
    {
        m_marqueeName.clear();

        hr = text.DrawString (name.c_str(), left, top, width, height, argb, fontDip, kFontFamily,
                              DxuiTextRenderer::HAlign::Center, DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
        BAIL_OUT_IF (true, S_OK);
    }

    // A new name waits the hold before its first scroll.
    if (name != m_marqueeName)
    {
        m_marqueeName    = name;
        m_marqueeStartMs = nowMs + kMarqueeHoldMs;
    }

    period = textW + kMarqueeGapDip * dipScale;
    offset = GetMarqueeOffset (nowMs, m_marqueeStartMs, period, speed);

    // Finished, and the pointer is still on the name: go again after the hold.
    if (m_hover == TapeDeckRegion::Name && speed > 0.0f &&
        nowMs - (m_marqueeStartMs + (int64_t) (period / speed * 1000.0f)) >= kMarqueeHoldMs)
    {
        m_marqueeStartMs = nowMs;
    }

    hr      = text.PushClipRect (left, top, width, height);
    clipped = SUCCEEDED (hr);

    for (float x : { left - offset, left - offset + period })
    {
        hr = text.DrawString (name.c_str(), x, top, textW + 1.0f, height, argb, fontDip, kFontFamily,
                              DxuiTextRenderer::HAlign::Left, DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & dxuiTheme)
{
    HRESULT             hr         = S_OK;
    const CassoTheme  & theme      = static_cast<const CassoTheme &> (dxuiTheme);
    float               dipScale   = (float) m_dpi / (float) kBaseDpi;
    bool                hasTape    = m_view.transport != TapeTransport::Empty;
    std::wstring        name       = GetDisplayName (m_view);
    std::wstring        readout    = FormatReadout (m_view);



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    BAIL_OUT_IF (m_hidden, S_OK);

    if (m_hover == TapeDeckRegion::Name)
    {
        painter.FillRect ((float) m_bandRect.left, (float) m_bandRect.top,
                          (float) (m_bandRect.right - m_bandRect.left),
                          (float) (m_railRect.bottom - m_bandRect.top),
                          theme.buttonHover);
    }

    PaintRail    (painter, theme);
    PaintButtons (painter, theme);

    hr = text.DrawString (kCaption,
                          (float) m_captionRect.left, (float) m_captionRect.top,
                          (float) (m_captionRect.right - m_captionRect.left),
                          (float) (m_captionRect.bottom - m_captionRect.top),
                          theme.dropdownAccel, kCaptionFontDip * dipScale, kFontFamily,
                          DxuiTextRenderer::HAlign::Left, DxuiTextRenderer::VAlign::Bottom);
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintName (text, name, hasTape && m_view.loadingPath.empty() ? theme.driveLabel : theme.dropdownAccel);

    hr = text.DrawString (readout.c_str(),
                          (float) m_readoutRect.left, (float) m_readoutRect.top,
                          (float) (m_readoutRect.right - m_readoutRect.left),
                          (float) (m_readoutRect.bottom - m_readoutRect.top),
                          theme.dropdownAccel, kReadoutFontDip * dipScale, kFontFamily,
                          DxuiTextRenderer::HAlign::Center, DxuiTextRenderer::VAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintRail
//
//  The drive rail's track, filled from the left as far as the tape has run.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::PaintRail (IDxuiPainter & painter, const CassoTheme & theme)
{
    float  left   = (float) m_railRect.left;
    float  top    = (float) m_railRect.top;
    float  width  = (float) (m_railRect.right - m_railRect.left);
    float  height = (float) (m_railRect.bottom - m_railRect.top);
    float  filled = width * GetProgress (m_view);



    painter.FillRect (left, top, width, height, theme.ledIdle);
    painter.FillRect (left, top, width, height, DxuiColor::ScaleAlpha (theme.driveLabel, kRailTrackAlpha));

    if (filled > 0.0f)
    {
        painter.FillRect (left, top, filled, height, DxuiColor::ScaleAlpha (theme.ledActive, kRailFillAlpha));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintButtons
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::PaintButtons (IDxuiPainter & painter, const CassoTheme & theme)
{
    for (size_t i = 0; i < kButtonCount; i++)
    {
        TapeDeckRegion  region = GetButtonRegion (i);
        const RECT    & box    = m_buttons[i];



        if (m_hover == region && IsRegionEnabled (region, m_view))
        {
            painter.FillRect ((float) box.left, (float) box.top,
                              (float) (box.right - box.left), (float) (box.bottom - box.top),
                              theme.buttonHover);
        }

        PaintMark (painter, region, box, GetMarkColor (region, theme));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMarkColor
//
//  Play lights while the tape plays. Record is red: full while recording,
//  dimmer while armed, and dimmed with the rest when it cannot be used.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t TapeDeckWidget::GetMarkColor (TapeDeckRegion region, const CassoTheme & theme) const
{
    bool  isEnabled = IsRegionEnabled (region, m_view);



    if (region == TapeDeckRegion::Play && m_view.transport == TapeTransport::Playing)
    {
        return theme.ledActive;
    }

    if (region == TapeDeckRegion::Record)
    {
        if (m_view.transport == TapeTransport::Recording)
        {
            return kRecordRedArgb;
        }

        if (m_view.isRecordArmed)
        {
            return DxuiColor::ScaleAlpha (kRecordRedArgb, kArmedAlpha);
        }
    }

    return isEnabled ? theme.driveLabel : DxuiColor::ScaleAlpha (theme.driveLabel, kDisabledAlpha);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintMark
//
//  The transport marks, drawn rather than taken from an icon font: the font
//  has play and record but no eject, and a row of drawn shapes stays one set.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::PaintMark (IDxuiPainter & painter, TapeDeckRegion region, const RECT & box, uint32_t argb)
{
    constexpr float  kHalf      = 0.5f;
    constexpr float  kEjectBar  = 0.22f;    // of the mark height, under the eject triangle
    float            size       = (float) (box.right - box.left);
    float            inset      = size * kMarkInsetRatio;
    float            l          = (float) box.left + inset;
    float            t          = (float) box.top  + inset;
    float            r          = (float) box.right  - inset;
    float            b          = (float) box.bottom - inset;
    float            cx         = (l + r) * kHalf;
    float            cy         = (t + b) * kHalf;
    float            barTop     = b - (b - t) * kEjectBar;
    float            arrowBase  = b - (b - t) * kEjectBar * 2.0f;



    switch (region)
    {
        case TapeDeckRegion::Rewind:
            painter.FillConvexQuad (cx, t, cx, b, l, cy, l, cy, argb);
            painter.FillConvexQuad (r, t, r, b, cx, cy, cx, cy, argb);
            break;

        case TapeDeckRegion::Play:
            painter.FillConvexQuad (l, t, r, cy, l, b, l, b, argb);
            break;

        case TapeDeckRegion::Stop:
            painter.FillRect (l, t, r - l, b - t, argb);
            break;

        case TapeDeckRegion::Record:
            painter.FillCircle (cx, cy, (r - l) * kHalf, argb);
            break;

        case TapeDeckRegion::Eject:
            painter.FillConvexQuad (cx, t, r, arrowBase, l, arrowBase, l, arrowBase, argb);
            painter.FillRect (l, barTop, r - l, b - barTop, argb);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRegion
//
//  Left to right, in the order a deck's keys run.
//
////////////////////////////////////////////////////////////////////////////////

TapeDeckRegion TapeDeckWidget::GetButtonRegion (size_t index)
{
    static constexpr TapeDeckRegion  kOrder[kButtonCount] = { TapeDeckRegion::Rewind,
                                                              TapeDeckRegion::Play,
                                                              TapeDeckRegion::Stop,
                                                              TapeDeckRegion::Record,
                                                              TapeDeckRegion::Eject };



    return index < kButtonCount ? kOrder[index] : TapeDeckRegion::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scale
//
////////////////////////////////////////////////////////////////////////////////

int TapeDeckWidget::Scale (int value, UINT dpi)
{
    return MulDiv (value, (int) dpi, kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPointInRect
//
//  Right and bottom edges are outside, as with every Win32 RECT.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::IsPointInRect (const RECT & rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}
