#include "Pch.h"

#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::AddPart
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::AddPart (IDxuiControl * control, HeightFn height, ShownFn shown)
{
    m_parts.push_back (Part { control, std::move (height), std::move (shown) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::SetPartBeside
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::SetPartBeside (IDxuiControl * control, IDxuiControl * beside, WidthFn width)
{
    for (Part & part : m_parts)
    {
        if (part.control == control)
        {
            part.beside = beside;
            part.width  = width;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::AddOverlay
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::AddOverlay (IDxuiControl * control, int widthDip, HeightFn height, ShownFn shown)
{
    AddPart (control, std::move (height), std::move (shown));
    m_parts.back().overlayDip = widthDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::SetPartInsetDip
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::SetPartInsetDip (IDxuiControl * control, int dip)
{
    for (Part & part : m_parts)
    {
        if (part.control == control)
        {
            part.insetDip = dip;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::SetPartTextAligned
//
//  The part's sides move in so its text, which it keeps innerPadDip in from
//  its own left, starts at the pane's text inset, the same distance from the
//  pane's outer edge as the pane's title. A part that keeps no room ahead of
//  its text, a label, passes 0.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::SetPartTextAligned (IDxuiControl * control, int innerPadDip)
{
    for (Part & part : m_parts)
    {
        if (part.control == control)
        {
            part.textAligned = true;
            part.innerPadDip = innerPadDip;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::IsPartShown
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerPaneFrame::IsPartShown (const Part & part) const
{
    return IsVisible() && (part.shown == nullptr || part.shown());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::GetSideInsetPx
//
//  A text-aligned part is kept in by the pane's text inset less its own
//  room ahead of its text, and never by a negative amount; any other part
//  by its inset.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerPaneFrame::GetSideInsetPx (const Part & part, const DxuiDpiScaler & scaler)
{
    int  side = scaler.ToPx (part.insetDip);



    if (part.textAligned)
    {
        side = std::max (0, DxuiPaneMetrics::GetContentTextInsetPx (scaler) - scaler.ToPx (part.innerPadDip));
    }

    return side;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::Relayout()
{
    Layout (m_boundsDip, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::Layout
//
//  Fixed parts take their heights first, then the part without one takes
//  the rest; a gap separates each shown part from the next. A part's top
//  inset is added to a fixed part's height and taken from a filling part's.
//  A fixed part's height is measured at the width it will be given.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                width   = (int) (boundsDip.right - boundsDip.left);
    int                total   = std::max (0, (int) (boundsDip.bottom - boundsDip.top) - scaler.ToPx (m_bottomMarginDip));
    int                fixed   = 0;
    int                shown   = 0;
    int                fill    = 0;
    int                gap     = scaler.ToPx (kGapDip);
    long               y       = boundsDip.top;
    int                inset   = 0;
    int                side    = 0;
    bool               hasFill = false;
    std::vector<int>   heights;
    RECT               filled  = boundsDip;



    SetBounds (boundsDip);
    m_scaler = scaler;

    fixed = MeasureFixedParts (width, scaler, heights, shown, hasFill);
    fill  = std::max (0, total - fixed - std::max (0, shown - 1) * gap);

    for (size_t i = 0; i < m_parts.size(); i++)
    {
        bool  on   = IsPartShown (m_parts[i]);
        long  left = 0;

        m_parts[i].control->SetVisible (on);

        if (!on || m_parts[i].beside != nullptr || m_parts[i].overlayDip > 0)
        {
            continue;
        }

        inset      = scaler.ToPx (m_parts[i].insetDip);
        side       = GetSideInsetPx (m_parts[i], scaler);
        heights[i] = (m_parts[i].height != nullptr) ? heights[i] : fill;
        left       = boundsDip.left + side;

        //  A part beside this one takes the left of its place.
        for (const Part & besidePart : m_parts)
        {
            int  besideSide  = 0;
            int  besideInset = 0;
            int  besideWidth = 0;

            if (besidePart.beside != m_parts[i].control || !IsPartShown (besidePart))
            {
                continue;
            }

            besideSide  = GetSideInsetPx (besidePart, scaler);
            besideInset = scaler.ToPx (besidePart.insetDip);
            besideWidth = std::max (0, besidePart.width (heights[i] - besideInset, scaler));

            besidePart.control->Layout (RECT { boundsDip.left + besideSide, y + besideInset, boundsDip.left + besideSide + besideWidth, y + heights[i] }, scaler);
            left = std::max (left, (long) (boundsDip.left + besideSide + besideWidth + gap));
        }

        m_parts[i].control->Layout (RECT { left, y + inset, boundsDip.right - side, y + heights[i] }, scaler);
        filled = (m_parts[i].height == nullptr) ? RECT { left, y + inset, boundsDip.right - side, y + heights[i] } : filled;
        y += heights[i] + gap;
    }

    LayOutOverlays (filled, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::LayOutOverlays
//
//  Each visible overlay centered over the part that fills, a gap in from its
//  sides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::LayOutOverlays (const RECT & filled, const DxuiDpiScaler & scaler)
{
    int  gap = scaler.ToPx (kGapDip);



    for (const Part & part : m_parts)
    {
        int  width  = 0;
        int  height = 0;
        int  left   = 0;
        int  top    = 0;

        if (part.overlayDip <= 0 || !IsPartShown (part))
        {
            continue;
        }

        width  = std::max (0, std::min<int> (scaler.ToPx (part.overlayDip), (filled.right - filled.left) - 2 * gap));
        height = (part.height != nullptr) ? part.height (width, scaler) : 0;
        left   = (filled.left + filled.right - width) / 2;
        top    = (filled.top + filled.bottom - height) / 2;

        part.control->Layout (RECT { left, top, left + width, top + height }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::MeasureFixedParts
//
//  A fixed part's height is measured at the width it will be given, with its
//  top inset added.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerPaneFrame::MeasureFixedParts (
    int                     width,
    const DxuiDpiScaler   & scaler,
    std::vector<int>      & heights,
    int                   & shownCount,
    bool                  & hasFill) const
{
    int  fixed = 0;
    int  inset = 0;
    int  side  = 0;



    heights.assign (m_parts.size(), 0);
    shownCount = 0;
    hasFill    = false;

    for (size_t i = 0; i < m_parts.size(); i++)
    {
        if (!IsPartShown (m_parts[i]) || m_parts[i].beside != nullptr || m_parts[i].overlayDip > 0)
        {
            continue;
        }

        shownCount++;

        if (m_parts[i].height == nullptr)
        {
            hasFill = true;
            continue;
        }

        inset      = scaler.ToPx (m_parts[i].insetDip);
        side       = GetSideInsetPx (m_parts[i], scaler);
        heights[i] = std::max (0, m_parts[i].height (width - side * 2, scaler)) + inset;
        fixed     += heights[i];
    }

    return fixed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::GetMinimumHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerPaneFrame::GetMinimumHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    std::vector<int>  heights;
    int               shown   = 0;
    bool              hasFill = false;
    int               fixed   = MeasureFixedParts (widthPx, scaler, heights, shown, hasFill);



    return fixed + std::max (0, shown - 1) * scaler.ToPx (kGapDip) + scaler.ToPx (m_bottomMarginDip) + (hasFill ? scaler.ToPx (kMinFillDip) : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::Paint
//
//  The parts are the window's children and are painted with it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    (void) painter;
    (void) text;
    (void) theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame::OnVisibilityChanged
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::OnVisibilityChanged()
{
    for (const Part & part : m_parts)
    {
        part.control->SetVisible (IsPartShown (part));
    }
}