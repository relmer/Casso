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
    std::vector<int>   heights (m_parts.size(), 0);



    SetBounds (boundsDip);
    m_scaler = scaler;

    for (size_t i = 0; i < m_parts.size(); i++)
    {
        if (!IsPartShown (m_parts[i]))
        {
            continue;
        }

        shown++;

        if (m_parts[i].height != nullptr)
        {
            inset      = scaler.ToPx (m_parts[i].insetDip);
            side       = GetSideInsetPx (m_parts[i], scaler);
            heights[i] = std::max (0, m_parts[i].height (width - side * 2, scaler)) + inset;
            fixed     += heights[i];
        }
    }

    fill = std::max (0, total - fixed - std::max (0, shown - 1) * gap);

    for (size_t i = 0; i < m_parts.size(); i++)
    {
        bool  on = IsPartShown (m_parts[i]);

        m_parts[i].control->SetVisible (on);

        if (!on)
        {
            continue;
        }

        inset      = scaler.ToPx (m_parts[i].insetDip);
        side       = GetSideInsetPx (m_parts[i], scaler);
        heights[i] = (m_parts[i].height != nullptr) ? heights[i] : fill;
        m_parts[i].control->Layout (RECT { boundsDip.left + side, y + inset, boundsDip.right - side, y + heights[i] }, scaler);
        y += heights[i] + gap;
    }
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