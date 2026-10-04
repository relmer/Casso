#include "Pch.h"

#include "Ui/Debugger/HistoryBand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::IsShown
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryBand::IsShown (const HistoryStatus & status)
{
    bool  isStoppedShort = status.outcome.has_value() && *status.outcome != ReverseOutcome::Moved;



    return status.isBehindLive || isStoppedShort;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetText (const HistoryStatus & status)
{
    std::wstring  text;



    if (status.outcome.has_value())
    {
        text = GetOutcomeText (*status.outcome);
    }

    if (status.isBehindLive)
    {
        text += (text.empty() ? L"" : L" ") + GetDistanceText (status.instructionsBehind, status.cyclesBehind);
    }

    if (status.unsavedDisks > 0)
    {
        text += (text.empty() ? L"" : L" ") + GetUnsavedText (status.unsavedDisks);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetUnsavedText
//
//  The disks whose guest writes have not reached their image files, which
//  stay unwritten while the machine is behind live.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetUnsavedText (int disks)
{
    if (disks <= 0)
    {
        return {};
    }

    return std::format (L"{} {} with writes not saved.", disks, (disks == 1) ? L"disk" : L"disks");
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetCompactText
//
//  The short outcome and the distance behind live, for a pane too narrow for
//  the outcome in full.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetCompactText (const HistoryStatus & status)
{
    std::wstring  text;
    bool          isStoppedShort = status.outcome.has_value() && *status.outcome != ReverseOutcome::Moved;



    if (isStoppedShort)
    {
        text = GetShortText (status) + L".";
    }

    if (status.isBehindLive)
    {
        text += (text.empty() ? L"" : L" ") + GetDistanceText (status.instructionsBehind, status.cyclesBehind);
    }

    if (status.unsavedDisks > 0)
    {
        text += (text.empty() ? L"" : L" ") + GetUnsavedText (status.unsavedDisks);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetShortText
//
//  For a pane too narrow for the whole text: what stopped the command short,
//  or only that the machine is behind live.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetShortText (const HistoryStatus & status)
{
    switch (status.outcome.value_or (ReverseOutcome::Moved))
    {
    case ReverseOutcome::AtHistoryStart: return L"Start of history";
    case ReverseOutcome::AtHistoryGap:   return L"Gap in history";
    case ReverseOutcome::HistoryCut:     return L"History cut";
    case ReverseOutcome::Stopped:        return L"Stopped";
    case ReverseOutcome::NoCaller:       return L"No caller";
    default:                            return status.isBehindLive ? L"Behind live" : L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetOutcomeText
//
//  What happened, in plain words, for each way a reverse command can stop
//  short; nothing for one that moved where it was asked to.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetOutcomeText (ReverseOutcome outcome)
{
    switch (outcome)
    {
    case ReverseOutcome::AtHistoryStart:
        return L"Stopped at the start of the recorded history: nothing earlier was recorded.";

    case ReverseOutcome::AtHistoryGap:
        return L"Stopped at a gap in the history: recording was paused while the machine ran at Maximum speed.";

    case ReverseOutcome::HistoryCut:
        return L"History after this point was dropped: replaying it did not reproduce the recorded machine. The machine is live here.";

    case ReverseOutcome::Stopped:
        return L"Stopped before the command finished: the machine is at the last position it reached.";

    case ReverseOutcome::NoCaller:
        return L"No caller to step back out to: no call in the recorded history entered the code running here. The machine has not moved.";

    default:
        return {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetDistanceText
//
//  Time at the Apple II's clock: milliseconds under a second, to three places
//  under one millisecond, and seconds above.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GetDistanceText (uint64_t instructions, uint64_t cycles)
{
    constexpr double  kMsPerSecond = 1000.0;
    double            seconds      = (double) cycles / kCyclesPerSecond;
    std::wstring      time;



    if (seconds * kMsPerSecond < 1.0)
    {
        time = std::format (L"{:.3f} ms", seconds * kMsPerSecond);
    }
    else if (seconds < 1.0)
    {
        time = std::format (L"{:.1f} ms", seconds * kMsPerSecond);
    }
    else
    {
        time = std::format (L"{:.2f} s", seconds);
    }

    return std::format (L"{} {} ({}) behind live.", GroupDigits (instructions), (instructions == 1) ? L"instruction" : L"instructions", time);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GroupDigits
//
//  Commas every three digits, whatever the locale, as the rest of the
//  debugger writes counts.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryBand::GroupDigits (uint64_t value)
{
    constexpr size_t  kGroup = 3;
    std::wstring      digits = std::to_wstring (value);
    std::wstring      grouped;
    size_t            i      = 0;



    for (i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % kGroup == 0)
        {
            grouped += L',';
        }

        grouped += digits[i];
    }

    return grouped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::Layout
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBand::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::Paint
//
//  The banner fill and edge, the text from the left, and the Go live link
//  at the right in the accent, its place kept for the mouse. The link is
//  drawn at the weight it was measured at and right-aligned against the
//  pad, so it never runs into the pane's edge; a narrow pane shortens the
//  text, never the link.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBand::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT                    hr          = S_OK;
    DxuiFontHandle             font        = theme.BodyFont();
    RECT                       bounds      = GetBounds();
    float                      left        = (float) bounds.left;
    float                      right       = (float) bounds.right;
    float                      top         = (float) bounds.top;
    float                      width       = right - left;
    float                      height      = (float) (bounds.bottom - bounds.top);
    float                      sizePx      = m_scaler.ToPxf (font.sizeDip);
    float                      pad         = m_scaler.ToPxf (kPadDip);
    float                      edge        = m_scaler.ToPxf ((float) kEdgeDip);
    bool                       isWarning   = m_status.outcome.has_value() && *m_status.outcome != ReverseOutcome::Moved;
    uint32_t                   fill        = isWarning ? theme.InfoBannerWarningBackground() : theme.InfoBannerBackground();
    uint32_t                   border      = isWarning ? theme.InfoBannerWarningBorder()     : theme.InfoBannerBorder();
    float                      linkWidth   = 0.0f;
    float                      measured    = 0.0f;
    float                      textHeight  = 0.0f;
    std::vector<std::wstring>  texts       = { GetText (m_status), GetCompactText (m_status), GetShortText (m_status) };
    std::vector<float>         widths;
    Placement                  placement;



    painter.FillRect    (left, top, width, height, fill);
    painter.FillRect    (left, top + height - edge, width, edge, border);
    m_linkRect = {};

    if (CanGoLive (m_status))
    {
        hr = text.MeasureString (kGoLiveText, sizePx, font.face, linkWidth, textHeight);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const std::wstring & each : texts)
    {
        hr = text.MeasureString (each.c_str(), sizePx, font.face, measured, textHeight);
        IGNORE_RETURN_VALUE (hr, S_OK);

        widths.push_back (measured);
    }

    placement = Place (left, right, pad, linkWidth, widths);

    if (CanGoLive (m_status))
    {
        m_linkRect = { (LONG) placement.linkLeft, bounds.top, (LONG) std::ceil (right - pad), bounds.bottom };

        hr = text.DrawString (kGoLiveText, placement.linkLeft, top, right - pad - placement.linkLeft, height, theme.Accent(),
                              sizePx, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.DrawString (texts[placement.textIndex].c_str(), left + pad, top, (std::max) (0.0f, placement.textRight - left - pad), height,
                          theme.InfoBannerForeground(), sizePx, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::Place
//
//  The link's right edge sits pad inside the band's; the text runs from
//  pad inside the left edge to pad before the link, and is the first of
//  textWidths, longest first, that fits there, or the last when none does.
//  A band too narrow for the link keeps it whole from the left pad.
//
////////////////////////////////////////////////////////////////////////////////

HistoryBand::Placement HistoryBand::Place (
    float                       left,
    float                       right,
    float                       pad,
    float                       linkWidth,
    const std::vector<float>  & textWidths)
{
    Placement  placement;
    float      room      = 0.0f;



    placement.linkLeft  = (std::max) (left + pad, right - pad - linkWidth);
    placement.textRight = (linkWidth > 0.0f) ? placement.linkLeft - pad : right - pad;
    room                = placement.textRight - left - pad;

    placement.textIndex = textWidths.empty() ? 0 : textWidths.size() - 1;

    for (size_t i = 0; i < textWidths.size(); i++)
    {
        if (textWidths[i] <= room)
        {
            placement.textIndex = i;
            break;
        }
    }

    return placement;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::OnMouse
//
//  A press on the link goes live; the band takes any other press too, so it
//  never falls through to the pane below.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryBand::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isPress = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left;



    if (isPress && IsOverLink (ev.positionDip) && m_onGoLive)
    {
        m_onGoLive();
    }

    return isPress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR HistoryBand::GetCursorForPoint (POINT clientPx) const
{
    return IsOverLink (clientPx) ? IDC_HAND : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand::IsOverLink
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryBand::IsOverLink (POINT point) const
{
    return CanGoLive (m_status) && PtInRect (&m_linkRect, point);
}





