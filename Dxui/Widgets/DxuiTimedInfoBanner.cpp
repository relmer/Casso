#include "Pch.h"

#include "Widgets/DxuiTimedInfoBanner.h"
#include "Core/DxuiSystemSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::ResolveDefaultDurationMs
//
//  The system's notification duration is an accessibility setting -- Settings
//  > Accessibility > Visual effects, "Dismiss notifications after this amount
//  of time" -- set by people who cannot read a banner in the time a default
//  one is up. It is taken as a FLOOR rather than as the value, because its own
//  default of five seconds would otherwise quietly retune every banner in the
//  app away from the duration above.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiTimedInfoBanner::ResolveDefaultDurationMs()
{
    int64_t  systemMs = (int64_t) DxuiSystemSettings::Instance().GetMessageDurationMs();



    return (systemMs > kDefaultDurationMs) ? systemMs : kDefaultDurationMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::DxuiTimedInfoBanner
//
//  Starts hidden: nothing has been reported yet.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTimedInfoBanner::DxuiTimedInfoBanner()
{
    m_scrim.SetToken   (DxuiSurface::Token::Background);
    m_scrim.SetOpacity (kDefaultScrimOpacity);

    m_banner.SetSeverity (DxuiInfoBanner::Severity::Info);
    m_banner.SetCentered (true);

    SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::Show
//
//  REPLACED, NOT QUEUED. A second notice inside the first one's countdown is
//  newer news; it takes the bar and gets the full duration of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTimedInfoBanner::Show (const std::wstring & text, int64_t nowMs)
{
    m_banner.SetText (text);
    m_untilMs = nowMs + m_durationMs;

    SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::Dismiss
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTimedInfoBanner::Dismiss()
{
    m_untilMs = 0;

    SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTimedInfoBanner::GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const
{
    return m_banner.GetPreferredHeightPx (widthPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::GetMeasuredHeightPx
//
//  Preferred over the estimate wherever a renderer exists. The banner centers
//  its text and picks its line width from a measurement, and the estimate
//  works from an average glyph width, so a wide face or a long path can
//  measure past it and put the last line outside the strip.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTimedInfoBanner::GetMeasuredHeightPx (
    IDxuiTextRenderer   & text,
    float                 widthPx,
    const DxuiDpiScaler & scaler) const
{
    return m_banner.GetMeasuredHeightPx (text, widthPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::Layout
//
//  The scrim and the banner share the bounds exactly: the scrim is the
//  banner's backing, not a margin around it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTimedInfoBanner::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scrim.Layout  (boundsDip, scaler);
    m_banner.Layout (boundsDip, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::Paint
//
//  The scrim first, then the banner over it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTimedInfoBanner::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_scrim.Paint  (painter, text, theme);
    m_banner.Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner::Tick
//
//  Takes the notice down once its countdown runs out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTimedInfoBanner::Tick (int64_t nowMs)
{
    if (IsVisible() && !IsShowing (nowMs))
    {
        SetVisible (false);
    }
}
