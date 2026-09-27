#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiSlide.h"
#include "Widgets/DxuiTimedInfoBanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStack
//
//  Notices over the content, one under another in the order they arrived,
//  each up for its own full time. A later notice never shortens or restarts
//  an earlier one; it appears below it. When a notice expires the ones below
//  it slide up into its place over the time and curve a menu opens with, or
//  jump there with animations off.
//
//  Where the stack goes is the caller's: it places notices from the top of
//  the bounds it is given. THE CLOCK AND THE ANIMATION FLAG ARE PASSED IN, so
//  a test can expire and slide notices without waiting or reading the system.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiNoticeStack : public IDxuiControl
{
public:

    DxuiNoticeStack  ();
    ~DxuiNoticeStack () override = default;

    //  Appends a notice below those showing, up for the full duration from nowMs.
    void  Push  (const std::wstring & text, int64_t nowMs);

    //  Takes every notice down now.
    void  Clear ();

    bool  IsShowing   (int64_t nowMs) const;
    bool  IsAnimating (int64_t nowMs) const;

    //  The next expiry or slide end; absent while nothing is showing.
    std::optional<int64_t>  GetNextChangeMs () const;

    size_t                  GetCount        () const { return m_notices.size(); }
    const std::wstring &    GetText         (size_t index) const;
    float                   GetOffsetPx     (size_t index, int64_t nowMs) const;

    //  Applies to notices pushed from now on.
    void  SetDurationMs        (int64_t durationMs) { m_durationMs = durationMs; }
    void  SetDpi               (UINT dpi);
    void  SetAnimationsEnabled (bool isEnabled)     { m_isAnimated = isEnabled; }

    //  Measures every notice at this width, with the renderer when there is
    //  one and the estimate when there is not, and returns the height the
    //  stack covers at the last tick.
    float  MeasureHeightPx (IDxuiTextRenderer    * text,
                            float                  widthPx,
                            const DxuiDpiScaler  & scaler);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void  Tick   (int64_t nowMs) override;

    size_t          GetChildCount () const override { return m_notices.size(); }
    IDxuiControl *  GetChild      (size_t index) const override;

    //  Enough space that two notices read as two rather than as one bar.
    static constexpr int  kGapDip = 4;

private:

    struct Notice
    {
        std::unique_ptr<DxuiTimedInfoBanner>  banner;
        DxuiSlide                             slide;
        float                                 heightPx = 0.0f;
    };

    void   RemoveNotice    (size_t index, int64_t nowMs);
    float  GetRestingTopPx (size_t index) const;

    std::vector<Notice>  m_notices;
    int64_t              m_durationMs = DxuiTimedInfoBanner::ResolveDefaultDurationMs();
    int64_t              m_nowMs      = 0;
    float                m_gapPx      = (float) kGapDip;
    UINT                 m_dpi        = DxuiDpiScaler::kBaseDpi;
    bool                 m_isAnimated = true;
};
