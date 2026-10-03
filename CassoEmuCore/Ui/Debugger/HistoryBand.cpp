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

    return text;
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
    default:                             return status.isBehindLive ? L"Behind live" : L"";
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
//  at the right in the accent, its place kept for the mouse.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBand::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT          hr          = S_OK;
    DxuiFontHandle   font        = theme.BodyFont();
    RECT             bounds      = GetBounds();
    float            left        = (float) bounds.left;
    float            top         = (float) bounds.top;
    float            width       = (float) (bounds.right - bounds.left);
    float            height      = (float) (bounds.bottom - bounds.top);
    float            sizePx      = m_scaler.ToPxf (font.sizeDip);
    float            pad         = m_scaler.ToPxf (kPadDip);
    float            edge        = m_scaler.ToPxf (kEdgeDip);
    bool             isWarning   = m_status.outcome.has_value() && *m_status.outcome != ReverseOutcome::Moved;
    uint32_t         fill        = isWarning ? theme.InfoBannerWarningBackground() : theme.InfoBannerBackground();
    uint32_t         border      = isWarning ? theme.InfoBannerWarningBorder()     : theme.InfoBannerBorder();
    float            linkWidth   = 0.0f;
    float            linkHeight  = 0.0f;
    float            linkLeft    = left + width;
    float            textWidth   = 0.0f;
    float            textHeight  = 0.0f;
    std::wstring     message     = GetText (m_status);



    painter.FillRect    (left, top, width, height, fill);
    painter.FillRect    (left, top + height - edge, width, edge, border);
    m_linkRect = {};

    if (CanGoLive (m_status))
    {
        hr = text.MeasureString (kGoLiveText, sizePx, font.face, linkWidth, linkHeight);
        IGNORE_RETURN_VALUE (hr, S_OK);

        linkLeft   = left + width - pad - linkWidth;
        m_linkRect = { (LONG) linkLeft, bounds.top, (LONG) (linkLeft + linkWidth), bounds.bottom };

        hr = text.DrawString (kGoLiveText, linkLeft, top, linkWidth, height, theme.Accent(), sizePx, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Bold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.MeasureString (message.c_str(), sizePx, font.face, textWidth, textHeight);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  Too wide for the pane: the short outcome with the distance, then the
    //  short text alone.
    if (textWidth > linkLeft - left - pad - pad)
    {
        message = GetCompactText (m_status);

        hr = text.MeasureString (message.c_str(), sizePx, font.face, textWidth, textHeight);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (textWidth > linkLeft - left - pad - pad)
    {
        message = GetShortText (m_status);
    }

    hr = text.DrawString (message.c_str(), left + pad, top, (std::max) (0.0f, linkLeft - left - pad - pad), height,
                          theme.InfoBannerForeground(), sizePx, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
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





