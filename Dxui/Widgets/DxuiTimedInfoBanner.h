#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiSurface.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBanner
//
//  A message bar that reports something that just happened and takes itself
//  down after a few seconds: a centered DxuiInfoBanner over a translucent
//  scrim.
//
//  AN OVERLAY, NOT A DOCKED BAND. A band that appears and vanishes on a timer
//  would reflow whatever sits beneath it twice per notice, so this one is
//  laid over the content and the scrim dims the strip it covers rather than
//  hiding it. Where it goes is the caller's; it fills the bounds it is given.
//
//  THE CLOCK IS PASSED IN. Show and Tick take the time rather than reading it,
//  so a test can expire a notice without waiting for one.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTimedInfoBanner : public IDxuiControl
{
public:

    DxuiTimedInfoBanner  ();
    ~DxuiTimedInfoBanner () override = default;

    //  Replaces whatever is showing and restarts the countdown from nowMs.
    void  Show            (const std::wstring & text, int64_t nowMs);

    //  Takes the notice down now, whatever is left on its countdown.
    void  Dismiss         ();

    //  Whether the notice is inside its countdown at nowMs.
    bool  IsShowing       (int64_t nowMs) const { return nowMs < m_untilMs; }

    void  SetDurationMs   (int64_t durationMs)                  { m_durationMs = durationMs; }
    void  SetScrimOpacity (float opacity)                       { m_scrim.SetOpacity (opacity); }
    void  SetSeverity     (DxuiInfoBanner::Severity severity)   { m_banner.SetSeverity (severity); }
    void  SetDpi          (UINT dpi)                            { m_banner.SetDpi (dpi); }

    const std::wstring &  GetText () const { return m_banner.GetText(); }

    //  The height the text needs at this width: measured when there is a
    //  renderer to ask, estimated (and rounded up) when there is not.
    float  GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const;
    float  GetMeasuredHeightPx  (IDxuiTextRenderer   &  text,
                                 float                  widthPx,
                                 const DxuiDpiScaler &  scaler) const;

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void  Tick   (int64_t nowMs) override;

    std::wstring        GetAccessibleName () const override { return m_banner.GetText(); }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

    //  Long enough to read a filename without hunting for it, short enough
    //  that it is gone before the next thing the user wants to look at.
    static constexpr int64_t  kDefaultDurationMs = 4000;

    //  The duration a banner takes when nobody sets one: this default, or
    //  the system's notification duration when the user has raised it above
    //  this. Somebody who has never touched the setting keeps the tuned 4 s;
    //  somebody who asked for thirty seconds because four is not enough time
    //  to read a banner gets thirty.
    static int64_t  ResolveDefaultDurationMs ();

    //  Enough dimming that the text stays legible over a bright picture,
    //  little enough that what is underneath is still visible.
    static constexpr float    kDefaultScrimOpacity = 0.82f;

private:

    DxuiInfoBanner  m_banner;
    DxuiSurface     m_scrim;
    int64_t         m_durationMs = ResolveDefaultDurationMs();
    int64_t         m_untilMs    = 0;
};
