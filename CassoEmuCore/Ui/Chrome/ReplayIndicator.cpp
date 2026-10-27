#include "Pch.h"

#include "Ui/Chrome/ReplayIndicator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicator::IsShown
//
////////////////////////////////////////////////////////////////////////////////

bool ReplayIndicator::IsShown (
    bool  isBehindLive,
    bool  isPaused,
    bool  isRewinding,
    bool  isHidden)
{
    return isBehindLive && !isPaused && !isRewinding && !isHidden;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicator::GetOpacity
//
//  Full at the start of each pulse, down to kMinOpacity halfway through and
//  back, on a cosine so it eases at both ends.
//
////////////////////////////////////////////////////////////////////////////////

float ReplayIndicator::GetOpacity (int64_t nowMs)
{
    double  phase = (double) (nowMs % kPulseMs) / (double) kPulseMs;
    double  wave  = 0.5 + 0.5 * std::cos (2.0 * std::numbers::pi * phase);



    return kMinOpacity + (1.0f - kMinOpacity) * (float) wave;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicator::Adopt
//
////////////////////////////////////////////////////////////////////////////////

void ReplayIndicator::Adopt (DxuiPanel & root)
{
    m_text.SetVisible (false);

    root.Adopt (m_text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicator::Sync
//
////////////////////////////////////////////////////////////////////////////////

void ReplayIndicator::Sync (
    bool                   isShown,
    const std::wstring   & text,
    const RECT           & screen,
    LONG                   topEdge,
    const DxuiDpiScaler  & scaler,
    int64_t                nowMs)
{
    RECT  rc = screen;



    if (!isShown || text.empty())
    {
        m_text.SetVisible (false);
        return;
    }

    rc.top    = std::max (screen.top, topEdge) + scaler.ToPx (kInsetDip);
    rc.bottom = rc.top + scaler.ToPx (kHeightDip);

    m_text.SetText        (text);
    m_text.SetFontSizeDip (kFontDip);
    m_text.SetAlign       (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    m_text.SetDpi         (scaler.GetDpi());
    m_text.SetOpacity     (GetOpacity (nowMs));
    m_text.Layout         (rc, scaler);
    m_text.SetVisible     (true);
}
