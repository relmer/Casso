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
    m_counterRect = {};
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
//  Returns whether anything drawn changed -- the hovered region, or the
//  controls' magnification, which follows every move near them -- so the
//  caller repaints.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::UpdateHover (int x, int y)
{
    TapeDeckRegion  region  = TapeDeckRegion::None;
    bool            moved   = false;
    bool            isNear  = IsNearControls (x, y);
    bool            wasNear = m_isNear;
    int64_t         nowMs   = GetNowMs();



    if (isNear != m_isNear)
    {
        m_presenceFrom = GetPresence (nowMs);
        m_presenceMs   = nowMs;
        m_isNear       = isNear;
    }

    m_mouseX = x;
    m_mouseY = y;
    region   = HitTest (x, y);
    moved    = region != m_hover || isNear || wasNear;



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
//      TAPE  [   adventure.wav    ]
//            [=======.............]
//            [<<][>][#][o][^]  1:23
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
    int   controlY = 0;



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
    // as a drive's band is. It stops at the rail; the controls row is below.
    m_bandRect = { x, y, nameX + nameW, m_railRect.bottom + Scale (kControlsGapYPx, dpi) / 2 };

    // The buttons from the rail's left edge, the counter in what is left of
    // its width, so the whole recorder is no wider than its name.
    controlY = m_railRect.bottom + Scale (kControlsGapYPx, dpi);

    for (size_t i = 0; i < kButtonCount; i++)
    {
        int  left = nameX + (int) i * (button + gap);

        m_buttons[i] = { left, controlY, left + button, controlY + button };
    }

    m_counterRect = { m_buttons[kButtonCount - 1].right + Scale (kCounterGapXPx, dpi), controlY,
                      nameX + nameW, controlY + button };

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
    ControlBox  rects[kButtonCount + 1]  = {};
    float       scales[kButtonCount + 1] = {};



    if (m_hidden)
    {
        return TapeDeckRegion::None;
    }

    // Against the controls as drawn, so a click lands on what is seen.
    ComputeControlRects (GetNowMs(), rects, scales);

    for (size_t i = 0; i < kButtonCount; i++)
    {
        if (rects[i].Contains (x, y))
        {
            return GetButtonRegion (i);
        }
    }

    if (rects[kButtonCount].Contains (x, y))
    {
        return TapeDeckRegion::Counter;
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



    if (m_hidden)
    {
        return {};
    }

    outer.bottom = m_counterRect.bottom + Scale (kLabelStripPx, m_dpi);

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
    bool  hasTape     = view.transport != TapeTransport::Empty;
    bool  isRecording = view.transport == TapeTransport::Recording;
    bool  isMoving    = view.transport != TapeTransport::Empty && view.transport != TapeTransport::Stopped;
    bool  isLoading   = !view.loadingPath.empty();



    // While a tape loads, only the picker and eject (which cancels the load)
    // do anything.
    if (isLoading)
    {
        return region == TapeDeckRegion::Name || region == TapeDeckRegion::Eject;
    }

    switch (region)
    {
        case TapeDeckRegion::Name:        return true;
        case TapeDeckRegion::Rewind:      return hasTape && !isRecording;
        case TapeDeckRegion::FastForward: return hasTape && !isRecording;
        case TapeDeckRegion::Play:        return hasTape && !isRecording && view.transport != TapeTransport::Playing;   // from fast-forward or rewind too
        case TapeDeckRegion::Stop:        return isMoving;
        case TapeDeckRegion::Record:      return hasTape && view.isWritable && (isRecording || view.transport == TapeTransport::Stopped);
        case TapeDeckRegion::Eject:       return true;   // with no tape it just opens the picker
        case TapeDeckRegion::Counter:     return hasTape;
        default:                          return false;
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
//  FormatCounter
//
//  The position, as a deck's counter shows it. Empty with no tape, so no
//  counter shows without one.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TapeDeckWidget::FormatCounter (const TapeDeckView & view)
{
    if (view.transport == TapeTransport::Empty)
    {
        return {};
    }

    return FormatTime (view.positionSeconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseTime
//
//  A position as a person types it: seconds ("90"), minutes and seconds
//  ("1:30"), or hours, minutes and seconds ("1:02:30"). Each field after the
//  first is under 60. Spaces around the text are ignored; anything else that
//  is not a digit or a colon makes it unreadable.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::ParseTime (const std::wstring & text, double & seconds)
{
    constexpr int     kMaxFields = 3;
    constexpr double  kPerField  = 60.0;
    size_t            first      = text.find_first_not_of (L" \t");
    size_t            last       = text.find_last_not_of  (L" \t");
    std::wstring      body;
    double            total      = 0.0;
    int               fields     = 0;
    bool              ok         = true;



    if (first == std::wstring::npos)
    {
        return false;
    }

    body = text.substr (first, last - first + 1);

    for (size_t start = 0; ok && start <= body.size(); )
    {
        size_t        colon = body.find (L':', start);
        std::wstring  field = body.substr (start, colon == std::wstring::npos ? std::wstring::npos : colon - start);
        double        value = 0.0;

        ok = !field.empty() && field.size() <= 4 &&
             field.find_first_not_of (L"0123456789") == std::wstring::npos;

        if (ok)
        {
            value = (double) std::stoi (field);
            ok    = fields == 0 || value < kPerField;
            total = total * kPerField + value;
            fields++;
        }

        start = (colon == std::wstring::npos) ? body.size() + 1 : colon + 1;
    }

    ok = ok && fields <= kMaxFields;

    if (ok)
    {
        seconds = total;
    }

    return ok;
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
//  GetMagnification
//
//  A raised cosine: full size right under the pointer, falling off smoothly
//  with no corner at either end, so a control grows and shrinks without a
//  jump as the pointer passes.
//
////////////////////////////////////////////////////////////////////////////////

float TapeDeckWidget::GetMagnification (float distance, float reach)
{
    constexpr float  kPi   = 3.14159265f;
    float            t     = 0.0f;



    // Either side alike, and nothing past the reach on either side: comparing
    // the signed distance let every control to the right of the pointer
    // through, where the cosine comes back up to full size two reaches out.
    t = (reach > 0.0f) ? fabsf (distance) / reach : 1.0f;

    if (t >= 1.0f)
    {
        return 1.0f;
    }

    return 1.0f + (kMagnifyMax - 1.0f) * 0.5f * (1.0f + cosf (kPi * t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMagnifying
//
//  Whether the controls are magnified or still easing in or out, so the shell
//  keeps the frames coming until they settle.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::IsMagnifying() const
{
    return m_isNear || GetPresence (GetNowMs()) > 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPresence
//
//  How strongly the magnification applies, 0 to 1: ramping up after the
//  pointer arrives and down after it leaves.
//
////////////////////////////////////////////////////////////////////////////////

float TapeDeckWidget::GetPresence (int64_t nowMs) const
{
    float  step = (float) (nowMs - m_presenceMs) / (float) kMagnifyFadeMs;



    return clamp (m_presenceFrom + (m_isNear ? step : -step), 0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsNearControls
//
//  Over the controls row as it is drawn right now -- magnified and spread,
//  which reaches past the resting row -- or above it where a magnified
//  control stands, or over the label strip under it. Measured against the
//  resting row instead, moving onto an end control that had grown out past
//  the row's edge dropped the magnification under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeckWidget::IsNearControls (int x, int y) const
{
    ControlBox  rects[kButtonCount + 1]  = {};
    float       scales[kButtonCount + 1] = {};
    int         button = m_buttons[0].bottom - m_buttons[0].top;
    int         left   = m_buttons[0].left;
    int         right  = m_counterRect.right;
    int         top    = m_buttons[0].top - (int) ((kMagnifyMax - 1.0f) * (float) button);
    int         bottom = m_counterRect.bottom + Scale (kLabelStripPx, m_dpi);



    if (m_hidden)
    {
        return false;
    }

    ComputeControlRects (GetNowMs(), rects, scales);

    left  = min (left,  (int) floorf (rects[0].left));
    right = max (right, (int) ceilf  (rects[kButtonCount].right));

    return x >= left && x < right && y >= top && y < bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeControlRects
//
//  The buttons and the counter as drawn this moment: each scaled by how near
//  the pointer is to it, then laid side by side again so they spread apart
//  rather than overlap, the row centered where it stands and every control
//  standing on the row's bottom edge. At rest this is exactly the layout.
//
//  The row stays where it is as the pointer moves along it. The reach is
//  exactly two pitches, and raised cosines that far apart at that spacing add
//  up to the same total wherever the pointer stands -- so the row's width,
//  and with it a centered row's position, does not change; only the controls
//  within it grow and shrink. Anchoring the row to the pointer instead slid
//  the whole row a few pixels as the pointer crossed each control.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeckWidget::ComputeControlRects (int64_t nowMs, ControlBox (& rects)[kButtonCount + 1], float (& scales)[kButtonCount + 1]) const
{
    constexpr size_t  kCount         = kButtonCount + 1;
    RECT              base[kCount]   = {};
    float             widths[kCount] = {};
    float             gaps[kCount]   = {};
    float             presence       = GetPresence (nowMs);
    float             pitch          = (float) (m_buttons[1].left - m_buttons[0].left);
    float             reach          = pitch * kMagnifyReachPitches;
    float             total          = 0.0f;
    float             center         = (float) (m_buttons[0].left + m_counterRect.right) * 0.5f;
    float             x              = 0.0f;
    float             bottom         = (float) m_counterRect.bottom;



    for (size_t i = 0; i < kButtonCount; i++)
    {
        base[i] = m_buttons[i];
    }

    base[kButtonCount] = m_counterRect;

    for (size_t i = 0; i < kCount; i++)
    {
        float  cx    = (float) (base[i].left + base[i].right) * 0.5f;
        float  full  = GetMagnification ((float) m_mouseX - cx, reach);

        scales[i] = 1.0f + (full - 1.0f) * presence;
        widths[i] = (float) (base[i].right - base[i].left) * scales[i];
        gaps[i]   = (i + 1 < kCount) ? (float) (base[i + 1].left - base[i].right) : 0.0f;
        total    += widths[i] + gaps[i];
    }

    x = center - total * 0.5f;

    for (size_t i = 0; i < kCount; i++)
    {
        float  height = (float) (base[i].bottom - base[i].top) * scales[i];

        rects[i] = { x, bottom - height, x + widths[i], bottom };
        x       += widths[i] + gaps[i];
    }
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
//  A name that fits is centered. One that does not shows its head, and while
//  the pointer is on it scrolls through the row as a marquee: two copies a
//  name-plus-gap apart, clipped to the row, so as the first leaves on the left
//  the second follows it in from the right and the scroll ends exactly where
//  it began.
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

    // Only under the pointer. A name scrolling on its own pulls the eye to
    // the band for no reason; at rest it shows its head, and the user hovers
    // it to read the rest.
    if (name != m_marqueeName)
    {
        m_marqueeName    = name;
        m_marqueeStartMs = nowMs;
    }

    period = textW + kMarqueeGapDip * dipScale;
    offset = (m_hover == TapeDeckRegion::Name) ? GetMarqueeOffset (nowMs, m_marqueeStartMs, period, speed) : 0.0f;

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
    std::wstring        counter    = FormatCounter (m_view);
    ControlBox          rects[kButtonCount + 1]  = {};
    float               scales[kButtonCount + 1] = {};
    const ControlBox  & counterBox = rects[kButtonCount];



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    BAIL_OUT_IF (m_hidden, S_OK);

    ComputeControlRects (GetNowMs(), rects, scales);

    // The highlight spans the name and the rail under it, not the caption,
    // as a drive's spans its name and head bar.
    if (m_hover == TapeDeckRegion::Name)
    {
        painter.FillRect ((float) m_nameRect.left, (float) m_nameRect.top,
                          (float) (m_nameRect.right - m_nameRect.left),
                          (float) (m_railRect.bottom - m_nameRect.top),
                          theme.buttonHover);
    }

    PaintRail (painter, theme);

    hr = text.DrawString (kCaption,
                          (float) m_captionRect.left, (float) m_captionRect.top,
                          (float) (m_captionRect.right - m_captionRect.left),
                          (float) (m_captionRect.bottom - m_captionRect.top),
                          theme.dropdownAccel, kCaptionFontDip * dipScale, kFontFamily,
                          DxuiTextRenderer::HAlign::Left, DxuiTextRenderer::VAlign::Bottom);
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintName (text, name, hasTape && m_view.loadingPath.empty() ? theme.driveLabel : theme.dropdownAccel);

    // The controls go last, in front. A magnified control stands up over the
    // rail and the name, and stands on the band's own color so what it covers
    // does not show through its mark.
    for (size_t i = 0; i <= kButtonCount; i++)
    {
        if (scales[i] > 1.0f)
        {
            painter.FillRect (rects[i].left, rects[i].top, rects[i].right - rects[i].left,
                              rects[i].bottom - rects[i].top, theme.navStrip);
        }
    }

    if (m_hover == TapeDeckRegion::Counter && IsRegionEnabled (TapeDeckRegion::Counter, m_view))
    {
        painter.FillRect (counterBox.left, counterBox.top,
                          counterBox.right - counterBox.left, counterBox.bottom - counterBox.top,
                          theme.buttonHover);
    }

    PaintButtons (painter, theme, rects);

    hr = text.DrawString (counter.c_str(),
                          counterBox.left, counterBox.top,
                          counterBox.right - counterBox.left, counterBox.bottom - counterBox.top,
                          theme.dropdownAccel, kCounterFontDip * dipScale * scales[kButtonCount], kFontFamily,
                          DxuiTextRenderer::HAlign::Center, DxuiTextRenderer::VAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    // The nearest control's label shows under it while the pointer is near
    // the row. One label only: under a magnified row, several at once
    // would run into each other.
    if (m_isNear)
    {
        size_t  nearest = 0;
        float   best    = FLT_MAX;
        float   labelW  = (float) Scale (kLabelWidthPx, m_dpi);
        float   cx      = 0.0f;

        for (size_t i = 0; i <= kButtonCount; i++)
        {
            const RECT &  baseBox  = (i < kButtonCount) ? m_buttons[i] : m_counterRect;
            float         distance = fabsf ((float) m_mouseX - (float) (baseBox.left + baseBox.right) * 0.5f);

            if (distance < best)
            {
                best    = distance;
                nearest = i;
            }
        }

        cx = (rects[nearest].left + rects[nearest].right) * 0.5f;

        hr = text.DrawString (GetControlLabel (nearest),
                              cx - labelW * 0.5f, (float) m_counterRect.bottom,
                              labelW, (float) Scale (kLabelStripPx, m_dpi),
                              theme.driveLabel, kLabelFontDip * dipScale, kFontFamily,
                              DxuiTextRenderer::HAlign::Center, DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

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

void TapeDeckWidget::PaintButtons (IDxuiPainter & painter, const CassoTheme & theme, const ControlBox (& rects)[kButtonCount + 1])
{
    for (size_t i = 0; i < kButtonCount; i++)
    {
        TapeDeckRegion      region = GetButtonRegion (i);
        const ControlBox  & box    = rects[i];



        if (m_hover == region && IsRegionEnabled (region, m_view))
        {
            painter.FillRect (box.left, box.top,
                              box.right - box.left, box.bottom - box.top,
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



    // The key that is down lights.
    if ((region == TapeDeckRegion::Play        && m_view.transport == TapeTransport::Playing)        ||
        (region == TapeDeckRegion::FastForward && m_view.transport == TapeTransport::FastForwarding) ||
        (region == TapeDeckRegion::Rewind      && m_view.transport == TapeTransport::Rewinding))
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

void TapeDeckWidget::PaintMark (IDxuiPainter & painter, TapeDeckRegion region, const ControlBox & box, uint32_t argb)
{
    constexpr float  kHalf      = 0.5f;
    constexpr float  kEjectBar  = 0.22f;    // of the mark height, under the eject triangle
    float            size       = box.right - box.left;
    float            inset      = size * kMarkInsetRatio;
    float            l          = box.left + inset;
    float            t          = box.top  + inset;
    float            r          = box.right  - inset;
    float            b          = box.bottom - inset;
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

        case TapeDeckRegion::FastForward:
            painter.FillConvexQuad (l, t, cx, cy, l, b, l, b, argb);
            painter.FillConvexQuad (cx, t, r, cy, cx, b, cx, b, argb);
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
//  GetControlLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * TapeDeckWidget::GetControlLabel (size_t index)
{
    static constexpr const wchar_t *  kLabels[kButtonCount + 1] = { L"Record",
                                                                    L"Rewind",
                                                                    L"Fast-forward",
                                                                    L"Play",
                                                                    L"Stop",
                                                                    L"Eject",
                                                                    L"Set position" };



    return index < std::size (kLabels) ? kLabels[index] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRegion
//
//  Left to right, in the order the RQ-309DS's own keys run.
//
////////////////////////////////////////////////////////////////////////////////

TapeDeckRegion TapeDeckWidget::GetButtonRegion (size_t index)
{
    static constexpr TapeDeckRegion  kOrder[kButtonCount] = { TapeDeckRegion::Record,
                                                              TapeDeckRegion::Rewind,
                                                              TapeDeckRegion::FastForward,
                                                              TapeDeckRegion::Play,
                                                              TapeDeckRegion::Stop,
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
