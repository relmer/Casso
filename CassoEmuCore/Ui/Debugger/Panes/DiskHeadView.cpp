#include "Pch.h"

#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::SetHead
//
//  The payload's drives as the choice has them: the drive this view draws,
//  and with both chosen, drive 2 on the view beside or under it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::SetHead (const DiagnosticsDiskHead & head)
{
    int  drive = head.drive;



    m_payload = head;

    switch (m_choice)
    {
    case DriveChoice::Drive1:  drive = 0; break;
    case DriveChoice::Both:    drive = 0; break;
    case DriveChoice::Drive2:  drive = 1; break;
    default:                   break;
    }

    SetDriveHead (MakeDriveHead (head, drive));

    if (m_choice == DriveChoice::Both && m_other != nullptr)
    {
        m_other->SetHead (MakeDriveHead (head, 1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::SetChoice
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::SetChoice (DriveChoice choice)
{
    m_choice = choice;

    if (choice == DriveChoice::Both && m_other == nullptr)
    {
        m_other = std::make_unique<DiskHeadView>();
    }

    SetHead (m_payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::MakeDriveHead
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsDiskHead DiskHeadView::MakeDriveHead (const DiagnosticsDiskHead & payload, int drive)
{
    DiagnosticsDiskHead  head = payload;



    if (drive == payload.drive || drive < 0 || drive >= (int) payload.quarterTracks.size())
    {
        return head;
    }

    head.drive        = drive;
    head.quarterTrack = payload.quarterTracks[(size_t) drive];
    head.phases       = 0;
    head.motorOn      = false;
    return head;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::IsSideBySide
//
////////////////////////////////////////////////////////////////////////////////

bool DiskHeadView::IsSideBySide (int widthPx, const DxuiDpiScaler & scaler)
{
    return widthPx >= scaler.ToPx (kSideBySideDip) * 2 + scaler.ToPx (kLabelGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::SetDriveHead
//
//  The first head, or another drive's, places the marker at once; a seek on
//  the same drive leaves it to sweep there on the ticks that follow.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::SetDriveHead (const DiagnosticsDiskHead & head)
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

    if (m_choice == DriveChoice::Both && m_other != nullptr)
    {
        m_other->Tick (nowMs);
    }
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



    if (m_choice == DriveChoice::Both && m_other != nullptr && !inside)
    {
        return m_visible ? m_other->GetColorTipAt (clientPx) : std::wstring();
    }

    if (!m_visible || !inside)
    {
        return {};
    }

    if (clientPx.y >= m_boundsDip.top + m_scaler.ToPx (kRulerDip))
    {
        return ColorLegend::GetText (ColorLegend::Meaning::LampLit);
    }

    if ((float) clientPx.x < GetRulerLeft())
    {
        return {};
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
    int   gap  = scaler.ToPx (kLabelGapDip);
    int   half = (widthPx - gap) / 2;
    auto  one  = [&scaler] (int width)
    {
        (void) width;
        return scaler.ToPx (kRulerDip + kGapDip + kRowDip);
    };



    if (m_choice != DriveChoice::Both)
    {
        return one (widthPx);
    }

    return IsSideBySide (widthPx, scaler) ? one (half) : one (widthPx) * 2 + gap;
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



    return std::format (L"Track {}.{:02}", m_head.quarterTrack / kQuarters, (m_head.quarterTrack % kQuarters) * (kHundredths / kQuarters));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetLampScale
//
//  The row under the ruler never wraps or runs past it: where the ruler is
//  narrower than the row, the text, the lamps and their spacing all shrink by
//  the same factor to fit. The widest track is assumed, so the row's size
//  doesn't change as the head moves.
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetLampScale (float widthPx) const
{
    constexpr int     kPhases        = 4;
    constexpr float   kAdvancePerDip = 0.6f;
    constexpr size_t  kWidestTrack   = std::size (L"Track 39.75") - 1;
    constexpr size_t  kMotor         = std::size (L"Motor") - 1;
    float             text           = m_fontDip * kAdvancePerDip * (float) (kWidestTrack + kMotor);
    float             row            = m_scaler.ToPxf (text + (float) (kLabelGapDip * 2 + kCaptionDip + kLampStepDip * kPhases));



    return std::clamp (widthPx / row, 0.0f, 1.0f);
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



    return GetRulerLeft() + GetHeadWidth() * (float) quarter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetRulerLeft
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetRulerLeft() const
{
    return std::min ((float) m_boundsDip.left + std::round (m_scaler.ToPxf ((float) kDriveDip)), (float) m_boundsDip.right);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadWidth
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetHeadWidth() const
{
    return ((float) m_boundsDip.right - GetRulerLeft()) / (float) GetQuarterCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   width = (int) (boundsPx.right - boundsPx.left);
    int   gap   = scaler.ToPx (kLabelGapDip);
    RECT  other = boundsPx;



    m_boundsDip = boundsPx;
    m_scaler    = scaler;

    if (m_choice != DriveChoice::Both || m_other == nullptr)
    {
        return;
    }

    if (IsSideBySide (width, scaler))
    {
        m_boundsDip.right = boundsPx.left + (width - gap) / 2;
        other.left        = m_boundsDip.right + gap;
    }
    else
    {
        m_boundsDip.bottom = boundsPx.top + (int) (boundsPx.bottom - boundsPx.top - gap) / 2;
        other.top          = m_boundsDip.bottom + gap;
    }

    m_other->SetVisible (true);
    m_other->Layout     (other, scaler);
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

    if (m_choice == DriveChoice::Both && m_other != nullptr)
    {
        m_other->Paint (painter, text, theme);
    }
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
    float     left  = GetRulerLeft();
    float     width = GetHeadWidth();
    uint32_t  trail = m_head.motorOn ? theme.Accent() : theme.ForegroundMuted();



    painter.FillRect (left, top, (float) m_boundsDip.right - left, ruler, theme.ControlBackground());

    for (int quarter = 0; quarter < GetQuarterCount(); quarter += kQuarters)
    {
        bool   major = (quarter / kQuarters) % kMajorTrack == 0;
        float  tall  = major ? ruler : ruler / 2;

        painter.FillRect (left + width * (float) quarter, top + ruler - tall, line, tall, theme.Divider());
    }

    for (const auto & [quarter, leftMs] : m_trail)
    {
        float  fade = 1.0f - std::clamp ((float) (m_nowMs - leftMs) / (float) kFadeMs, 0.0f, 1.0f);

        painter.FillRect (left + width * (float) quarter, top, std::max (line, width), ruler,
                          DxuiColor::ScaleAlpha (trail, fade));
    }

    painter.FillRect (GetHeadX(), top, std::max (line, width), ruler, GetHeadColor (theme));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::PaintLamps
//
//  The drive's name ahead of the ruler. Under the ruler, from its left edge,
//  the head's track, then the word Motor in the accent while the motor is on,
//  then the stepper's phase magnets under a caption, 3 2 1 0, the last
//  ending at the ruler's right edge.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int     kPhases        = 4;
    constexpr float   kAdvancePerDip = 0.6f;
    constexpr size_t  kWidestTrack   = std::size (L"Track 39.75") - 1;
    DxuiFontHandle    font           = theme.MonospaceFont();
    float             left           = GetRulerLeft();
    float             scale          = GetLampScale ((float) m_boundsDip.right - left);
    float             row            = m_scaler.ToPxf ((float) kRowDip);
    float             lamp           = m_scaler.ToPxf ((float) kLampDip) * scale;
    float             step           = m_scaler.ToPxf ((float) kLampStepDip) * scale;
    float             caption        = m_scaler.ToPxf ((float) kCaptionDip) * scale;
    float             gap            = m_scaler.ToPxf ((float) kLabelGapDip) * scale;
    float             top            = (float) m_boundsDip.top + m_scaler.ToPxf ((float) (kRulerDip + kGapDip));
    float             size           = m_scaler.ToPxf (font.sizeDip) * scale;
    float             trackW         = m_scaler.ToPxf (font.sizeDip * kAdvancePerDip * (float) kWidestTrack) * scale;
    float             x              = (float) m_boundsDip.right - caption - step * kPhases;
    std::wstring      label;
    HRESULT           hr             = S_OK;



    label = GetDriveText();
    hr    = text.DrawString (label.c_str(), (float) m_boundsDip.left, (float) m_boundsDip.top, left - (float) m_boundsDip.left, m_scaler.ToPxf ((float) kRulerDip),
                             theme.Foreground(), m_scaler.ToPxf (font.sizeDip), font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    label = GetTrackText();
    hr    = text.DrawString (label.c_str(), left, top, trackW, row, theme.Foreground(), size, font.face,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (L"Motor", left + trackW + gap, top, std::max (0.0f, x - (left + trackW + gap)), row,
                          m_head.motorOn ? theme.Accent() : theme.ForegroundDisabled(), size, font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, m_head.motorOn ? DxuiFontWeight::Bold : DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (L"Phases", x, top, caption, row, theme.ForegroundMuted(), size, font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    x += caption;

    for (int phase = kPhases - 1; phase >= 0; phase--)
    {
        bool  on = ((m_head.phases >> phase) & 1) != 0;

        //  The digit, then its lamp, so the last lamp ends at the edge.
        label = std::format (L"{}", phase);
        hr    = text.DrawString (label.c_str(), x, top, step - lamp, row, theme.ForegroundMuted(), size, font.face,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.FillRect (x + step - lamp, top + (row - lamp) / 2, lamp, lamp, on ? theme.Accent() : theme.ControlBackground());

        x += step;
    }
}