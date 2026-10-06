#include "Pch.h"

#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::SetHead
//
//  The first head, or another drive's, places the marker at once; a seek on
//  the same drive leaves it to sweep there on the ticks that follow.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::SetHead (const DiagnosticsDiskHead & head)
{
    if (!m_placed || head.drive != m_head.drive)
    {
        m_placed   = true;
        m_shown    = (double) head.quarterTrack;
        m_arriveMs = INT64_MIN / 2;
        m_trail.clear();
    }

    m_head = head;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Tick
//
//  The marker moves toward the head at the stepping speed. Each quarter track
//  it leaves starts to fade, and the one it lands on is no longer trail.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Tick (int64_t nowMs)
{
    double  target  = (double) m_head.quarterTrack;
    double  elapsed = (double) std::max<int64_t> (0, nowMs - m_nowMs);
    double  reach   = elapsed / kMsPerQuarter;
    int     from    = (int) std::lround (m_shown);
    int     to      = 0;
    int     step    = 0;



    m_nowMs = nowMs;

    if (m_shown != target)
    {
        m_shown = (m_shown < target) ? std::min (target, m_shown + reach) : std::max (target, m_shown - reach);
        to      = (int) std::lround (m_shown);
        step    = (to > from) ? 1 : -1;

        for (int quarter = from; quarter != to; quarter += step)
        {
            //  Left as long ago as the rest of the way took to step.
            m_trail[quarter] = nowMs - (int64_t) (std::abs (m_shown - (double) quarter) * kMsPerQuarter);
        }

        m_trail.erase (to);

        if (m_shown == target)
        {
            m_arriveMs = nowMs;
        }
    }

    std::erase_if (m_trail, [nowMs] (const auto & left) { return nowMs - left.second > kFadeMs; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetFlashColor
//
//  White on a dark ruler and the text color on a light one, so the flash
//  stands out from the accent either way.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskHeadView::GetFlashColor (const IDxuiTheme & theme)
{
    constexpr float     kDarkRuler = 0.4f;
    constexpr uint32_t  kWhite     = 0xFFFFFFFFu;



    return DxuiColor::ComputeRelativeLuminance (theme.ControlBackground()) < kDarkRuler ? kWhite : theme.Foreground();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadColor
//
//  Muted while the motor rests, the flash on the move and as the head
//  arrives, then settling to the accent.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskHeadView::GetHeadColor (const IDxuiTheme & theme) const
{
    uint32_t  flash   = GetFlashColor (theme);
    float     settled = 0.0f;



    if (!m_head.motorOn)
    {
        return theme.ForegroundMuted();
    }

    if (m_shown != (double) m_head.quarterTrack)
    {
        return flash;
    }

    settled = std::clamp ((float) (m_nowMs - m_arriveMs) / (float) kSettleMs, 0.0f, 1.0f);
    return DxuiColor::Mix (flash, theme.Accent(), settled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetColorTipAt
//
//  Over the ruler, what the head's color says of it: resting with the motor
//  off, stepping, or settled on its track. Below it, what a lit lamp means.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskHeadView::GetColorTipAt (POINT clientPx) const
{
    ColorLegend::Meaning  meaning = ColorLegend::Meaning::HeadSettled;
    bool                  inside  = clientPx.x >= m_boundsDip.left && clientPx.x < m_boundsDip.right &&
                                    clientPx.y >= m_boundsDip.top  && clientPx.y < m_boundsDip.bottom;



    if (!m_visible || !inside)
    {
        return {};
    }

    if (clientPx.y >= m_boundsDip.top + m_scaler.ToPx (kRulerDip))
    {
        return ColorLegend::GetText (ColorLegend::Meaning::LampLit);
    }

    if (!m_head.motorOn)
    {
        meaning = ColorLegend::Meaning::HeadMotorOff;
    }
    else if (m_shown != (double) m_head.quarterTrack)
    {
        meaning = ColorLegend::Meaning::HeadMoving;
    }

    return ColorLegend::GetText (meaning);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DiskHeadView::GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  rows = 1 + (IsLabelBelow ((float) widthPx, scaler) ? 1 : 0) + (IsLabelSplit ((float) widthPx, scaler) ? 1 : 0);



    return scaler.ToPx (kRulerDip + kGapDip + kRowDip * rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskHeadView::GetLabel() const
{
    return std::format (L"{}  {}", GetDriveText(), GetTrackText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetDriveText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskHeadView::GetDriveText() const
{
    return std::format (L"Drive {}", m_head.drive + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetTrackText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskHeadView::GetTrackText() const
{
    constexpr int  kHundredths = 100;



    return std::format (L"track {}.{:02}", m_head.quarterTrack / kQuarters, (m_head.quarterTrack % kQuarters) * (kHundredths / kQuarters));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::IsLabelBelow
//
//  The lamps and the label side by side where they fit, measured by the
//  monospace advance; otherwise the label takes a row of its own rather than
//  being cut off at the pane's edge. The widest track is assumed, so the
//  label does not jump between rows as the head moves.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskHeadView::IsLabelBelow (float widthPx, const DxuiDpiScaler & scaler) const
{
    static constexpr float  kAdvancePerDip = 0.6f;
    static constexpr int    kPhases        = 4;
    static constexpr int    kCaptions      = 2;     // "Phases" and the motor's lamp
    static constexpr size_t kWidestLabel   = std::size (L"Drive 2  track 39.75") - 1;
    float                   lamps          = scaler.ToPxf ((float) kLampStepDip) * kPhases + scaler.ToPxf ((float) kCaptionDip) * kCaptions;
    float                   label          = scaler.ToPxf (m_fontDip) * kAdvancePerDip * (float) kWidestLabel;



    return lamps + label > widthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::IsLabelSplit
//
//  Where even a row of its own is too narrow for the whole label, the drive
//  and the track take a row each. The break falls only between the two, so a
//  word or a track number is never divided. The widest track is assumed, as
//  for IsLabelBelow.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskHeadView::IsLabelSplit (float widthPx, const DxuiDpiScaler & scaler) const
{
    static constexpr float  kAdvancePerDip = 0.6f;
    static constexpr size_t kWidestLabel   = std::size (L"Drive 2  track 39.75") - 1;
    float                   label          = scaler.ToPxf (m_fontDip) * kAdvancePerDip * (float) kWidestLabel;



    return label > widthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetLampScale
//
//  The lamp row never wraps or runs past the pane: where the pane is narrower
//  than the row, the lamps, their spacing and their captions all shrink by
//  the same factor to fit.
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetLampScale (float widthPx) const
{
    constexpr int  kPhases   = 4;
    constexpr int  kCaptions = 2;     // "Phases" and the motor's lamp
    float          lamps     = m_scaler.ToPxf ((float) (kLampStepDip * kPhases + kCaptionDip * kCaptions));



    return std::clamp (widthPx / lamps, 0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadX
//
//  Each quarter track has an equal share of the width; the marker covers the
//  head's share.
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetHeadX() const
{
    double  quarter = std::clamp (m_shown, 0.0, (double) (GetQuarterCount() - 1));



    return (float) m_boundsDip.left + GetHeadWidth() * (float) quarter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadWidth
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetHeadWidth() const
{
    return (float) (m_boundsDip.right - m_boundsDip.left) / (float) GetQuarterCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    //  Whether the label fits beside the lamps is judged at this size before
    //  the next layout.
    m_fontDip = theme.MonospaceFont().sizeDip;

    PaintRuler (painter, theme);
    PaintLamps (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::PaintRuler
//
//  A tick at the start of every track, a longer one every fifth, the tracks
//  the head has just left fading out, and the head itself.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintRuler (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    float     ruler = m_scaler.ToPxf ((float) kRulerDip);
    float     line  = std::max (1.0f, std::round (m_scaler.ToPxf (1.0f)));
    float     top   = (float) m_boundsDip.top;
    float     width = GetHeadWidth();
    uint32_t  trail = m_head.motorOn ? theme.Accent() : theme.ForegroundMuted();



    painter.FillRect ((float) m_boundsDip.left, top, (float) (m_boundsDip.right - m_boundsDip.left), ruler, theme.ControlBackground());

    for (int quarter = 0; quarter < GetQuarterCount(); quarter += kQuarters)
    {
        bool   major = (quarter / kQuarters) % kMajorTrack == 0;
        float  tall  = major ? ruler : ruler / 2;

        painter.FillRect ((float) m_boundsDip.left + width * (float) quarter, top + ruler - tall, line, tall, theme.Divider());
    }

    for (const auto & [quarter, leftMs] : m_trail)
    {
        float  fade = 1.0f - std::clamp ((float) (m_nowMs - leftMs) / (float) kFadeMs, 0.0f, 1.0f);

        painter.FillRect ((float) m_boundsDip.left + width * (float) quarter, top, std::max (line, width), ruler,
                          DxuiColor::ScaleAlpha (trail, fade));
    }

    painter.FillRect (GetHeadX(), top, std::max (line, width), ruler, GetHeadColor (theme));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::PaintLamps
//
//  The stepper's four phase magnets under a caption, then the motor, then
//  the drive and track as text: beside the lamps, or on the row below them
//  where it would not fit.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kPhases = 4;
    DxuiFontHandle  font    = theme.MonospaceFont();
    float           scale   = GetLampScale ((float) (m_boundsDip.right - m_boundsDip.left));
    float           row     = m_scaler.ToPxf ((float) kRowDip);
    float           lamp    = m_scaler.ToPxf ((float) kLampDip) * scale;
    float           step    = m_scaler.ToPxf ((float) kLampStepDip) * scale;
    float           caption = m_scaler.ToPxf ((float) kCaptionDip) * scale;
    float           top     = (float) m_boundsDip.top + m_scaler.ToPxf ((float) (kRulerDip + kGapDip));
    float           x       = (float) m_boundsDip.left;
    float           size    = m_scaler.ToPxf (font.sizeDip) * scale;
    std::wstring    label;
    HRESULT         hr      = S_OK;



    hr = text.DrawString (L"Phases", x, top, caption, row, theme.ForegroundMuted(), size, font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    x += caption;

    for (int phase = 0; phase <= kPhases; phase++)
    {
        bool  isMotor = (phase == kPhases);
        bool  on      = isMotor ? m_head.motorOn : ((m_head.phases >> phase) & 1) != 0;

        painter.FillRect (x, top + (row - lamp) / 2, lamp, lamp, on ? theme.Accent() : theme.ControlBackground());

        label = isMotor ? std::wstring (L"Motor") : std::format (L"{}", phase);
        hr    = text.DrawString (label.c_str(), x + lamp, top, (isMotor ? caption : step) - lamp, row, theme.ForegroundMuted(), size, font.face,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += isMotor ? caption : step;
    }

    if (IsLabelBelow ((float) (m_boundsDip.right - m_boundsDip.left), m_scaler))
    {
        x    = (float) m_boundsDip.left;
        top += row;
    }

    size = m_scaler.ToPxf (font.sizeDip);

    if (IsLabelSplit ((float) (m_boundsDip.right - m_boundsDip.left), m_scaler))
    {
        label = GetDriveText();
        hr    = text.DrawString (label.c_str(), x, top, std::max (0.0f, (float) m_boundsDip.right - x), row, theme.Foreground(), size, font.face,
                                 DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        top   += row;
        label  = GetTrackText();
    }
    else
    {
        label = GetLabel();
    }

    hr = text.DrawString (label.c_str(), x, top, std::max (0.0f, (float) m_boundsDip.right - x), row, theme.Foreground(), size, font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
