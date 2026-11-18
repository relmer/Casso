#include "Pch.h"

#include "DxuiDockGaps.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps::GetInsetForGap
//
//  A group's rect less its share of the gaps. A side inside the pane area
//  moves in by half the gap, a left or top side by the larger half, so the
//  neighbors at a split are exactly the gap apart; a side on the pane area's
//  edge stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGaps::GetInsetForGap (const RECT & rect, const RECT & paneArea, long gapPx)
{
    long  lo    = gapPx / 2;
    long  hi    = gapPx - lo;
    RECT  inset = rect;



    if (gapPx <= 0)
    {
        return rect;
    }

    if (rect.left > paneArea.left)
    {
        inset.left += hi;
    }

    if (rect.top > paneArea.top)
    {
        inset.top += hi;
    }

    if (rect.right < paneArea.right)
    {
        inset.right -= lo;
    }

    if (rect.bottom < paneArea.bottom)
    {
        inset.bottom -= lo;
    }

    inset.right  = std::max (inset.right,  inset.left);
    inset.bottom = std::max (inset.bottom, inset.top);
    return inset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps::GetMinSizeWithGap
//
//  Each pane's minimum size as the layout takes it. A pane loses up to the
//  gap on each axis, so while a gap is set it needs that much more.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout::MinSizeFn DxuiDockGaps::GetMinSizeWithGap (
    const DxuiPaneLayout::MinSizeFn  & minSize,
    const DxuiDpiScaler              & scaler) const
{
    long                       gap    = scaler.ToPx (m_gapDip);
    DxuiPaneLayout::MinSizeFn  result = minSize;



    if (gap > 0 && minSize)
    {
        result = [base = minSize, gap] (const std::wstring & pane)
        {
            SIZE  size = base (pane);

            return SIZE { size.cx + gap, size.cy + gap };
        };
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps::Paint
//
//  The margin around the panes and the gap at every split, in the theme's
//  DockGap. None of it lies under a group: the main window's site paints
//  after the pane controls, so a fill there would cover a pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGaps::Paint (
    IDxuiPainter         & painter,
    const IDxuiTheme     & theme,
    const RECT           & dockedArea,
    const RECT           & paneArea,
    const DxuiDpiScaler  & scaler) const
{
    uint32_t  argb      = theme.DockGap();
    RECT      margins[] =
    {
        { dockedArea.left,  dockedArea.top,  dockedArea.right, paneArea.top       },
        { dockedArea.left,  paneArea.bottom, dockedArea.right, dockedArea.bottom  },
        { dockedArea.left,  paneArea.top,    paneArea.left,    paneArea.bottom    },
        { paneArea.right,   paneArea.top,    dockedArea.right, paneArea.bottom    },
    };
    auto      fill      = [&painter, argb] (const RECT & r)
    {
        if (r.right > r.left && r.bottom > r.top)
        {
            painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top), argb);
        }
    };



    if (!IsSet())
    {
        return;
    }

    for (const RECT & margin : margins)
    {
        fill (margin);
    }

    if (m_gapDip <= 0)
    {
        return;
    }

    for (const SplitRect & split : m_splits)
    {
        fill (GetSashRect (split, scaler));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps::GetSashRect
//
//  With a gap, exactly the gap, so a press anywhere between two panes drags
//  their split and a press on a pane never does; without one, a band
//  centered on the split.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGaps::GetSashRect (const SplitRect & split, const DxuiDpiScaler & scaler) const
{
    long  gap = scaler.ToPx (m_gapDip);
    long  lo  = (m_gapDip > 0) ? gap / 2 : scaler.ToPx (kSashDip) / 2;
    long  hi  = (m_gapDip > 0) ? gap - lo : lo;



    return split.horizontal ? RECT { split.position - lo, split.area.top,  split.position + hi, split.area.bottom }
                            : RECT { split.area.left, split.position - lo, split.area.right, split.position + hi };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGaps::HitTestSash
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockGaps::HitTestSash (POINT pointDip, const DxuiDpiScaler & scaler) const
{
    for (size_t i = 0; i < m_splits.size(); i++)
    {
        RECT  sash = GetSashRect (m_splits[i], scaler);

        if (pointDip.x >= sash.left && pointDip.x < sash.right && pointDip.y >= sash.top && pointDip.y < sash.bottom)
        {
            return (int) i;
        }
    }

    return -1;
}
