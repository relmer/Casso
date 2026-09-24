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
//  DebuggerPaneFrame::IsPartShown
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerPaneFrame::IsPartShown (const Part & part) const
{
    return IsVisible() && (part.shown == nullptr || part.shown());
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
//  the rest; a gap separates each shown part from the next.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerPaneFrame::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                width   = (int) (boundsDip.right - boundsDip.left);
    int                total   = std::max (0, (int) (boundsDip.bottom - boundsDip.top) - scaler.ToPx (m_bottomMarginDip));
    int                fixed   = 0;
    int                shown   = 0;
    int                fill    = 0;
    long               y       = boundsDip.top;
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
            heights[i] = std::max (0, m_parts[i].height (width, scaler));
            fixed     += heights[i];
        }
    }

    fill = std::max (0, total - fixed - std::max (0, shown - 1) * kGapDip);

    for (size_t i = 0; i < m_parts.size(); i++)
    {
        bool  on = IsPartShown (m_parts[i]);

        m_parts[i].control->SetVisible (on);

        if (!on)
        {
            continue;
        }

        heights[i] = (m_parts[i].height != nullptr) ? heights[i] : fill;
        m_parts[i].control->Layout (RECT { boundsDip.left, y, boundsDip.right, y + heights[i] }, scaler);
        y += heights[i] + kGapDip;
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