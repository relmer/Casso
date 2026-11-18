#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"


class DxuiDpiScaler;
class IDxuiPainter;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps
//
//  The room between a dock site's panes and around them, as in Visual
//  Studio, and the sashes at the splits between them: where each split lies,
//  the band a press drags it by, and the fill of the gaps and margin.
//  Zero for both, the default, puts the panes edge to edge.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockGaps
{
public:
    using SplitRect = DxuiPaneLayout::SplitRect;

    static constexpr int  kSashDip = 6;

    void  Set         (int gapDip, int marginDip) { m_gapDip = gapDip; m_marginDip = marginDip; }
    int   GetGapDip   () const { return m_gapDip; }
    int   GetMarginDip() const { return m_marginDip; }
    bool  IsSet       () const { return m_gapDip != 0 || m_marginDip != 0; }

    void                            SetSplits (std::vector<SplitRect> splits) { m_splits = std::move (splits); }
    const std::vector<SplitRect>  & GetSplits () const { return m_splits; }

    static RECT                GetInsetForGap    (const RECT & rect, const RECT & paneArea, long gapPx);
    DxuiPaneLayout::MinSizeFn  GetMinSizeWithGap (const DxuiPaneLayout::MinSizeFn & minSize, const DxuiDpiScaler & scaler) const;
    RECT                       GetSashRect       (const SplitRect & split, const DxuiDpiScaler & scaler) const;
    int                        HitTestSash       (POINT pointDip, const DxuiDpiScaler & scaler) const;
    void                       Paint             (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & dockedArea, const RECT & paneArea, const DxuiDpiScaler & scaler) const;

private:
    int                     m_gapDip    = 0;
    int                     m_marginDip = 0;
    std::vector<SplitRect>  m_splits;
};
