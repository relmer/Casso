#include "Pch.h"

#include "DxuiSplitter.h"

#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::GetExtentDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSplitter::GetExtentDip() const
{
    int  extentPx = (m_orientation == Orientation::Vertical)
                  ? (m_boundsDip.right - m_boundsDip.left)
                  : (m_boundsDip.bottom - m_boundsDip.top);



    return MulDiv (extentPx, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::ClampPositionDip
//
//  The first pane keeps its minimum, and so does the second, which gets
//  whatever lies past the sash. When the two minimums cannot both fit, the
//  first wins, since it is the one the user drags from.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSplitter::ClampPositionDip (int dip) const
{
    int  extent  = GetExtentDip();
    int  highest = extent - kSashDip - m_minSecondDip;



    if (extent <= 0)
    {
        return (std::max) (dip, m_minFirstDip);
    }

    if (dip > highest)
    {
        dip = highest;
    }

    if (dip < m_minFirstDip)
    {
        dip = m_minFirstDip;
    }

    return dip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::SetPositionDip
//
//  Programmatic, so it does not report a move: the caller already knows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSplitter::SetPositionDip (int dip)
{
    m_positionDip = ClampPositionDip (dip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::SetLimitsDip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSplitter::SetLimitsDip (int minFirst, int minSecond)
{
    m_minFirstDip  = (std::max) (minFirst, 0);
    m_minSecondDip = (std::max) (minSecond, 0);
    m_positionDip  = ClampPositionDip (m_positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::MoveTo
//
//  A user move: clamped, and reported only when it changed something.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSplitter::MoveTo (int dip)
{
    int  clamped = ClampPositionDip (dip);



    if (clamped == m_positionDip)
    {
        return;
    }

    m_positionDip = clamped;

    if (m_onMoved)
    {
        m_onMoved (m_positionDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::GetSashRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiSplitter::GetSashRect() const
{
    RECT  sash   = m_boundsDip;
    int   offset = m_scaler.ToPx (m_positionDip);
    int   thick  = m_scaler.ToPx (kSashDip);



    if (m_orientation == Orientation::Vertical)
    {
        sash.left  = m_boundsDip.left + offset;
        sash.right = sash.left + thick;
    }
    else
    {
        sash.top    = m_boundsDip.top + offset;
        sash.bottom = sash.top + thick;
    }

    return sash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::IsOverSash
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSplitter::IsOverSash (POINT px) const
{
    RECT  sash = GetSashRect();



    return px.x >= sash.left && px.x < sash.right && px.y >= sash.top && px.y < sash.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::Layout
//
//  A new extent can squeeze the second pane below its minimum, so the
//  position is clamped again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSplitter::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    m_positionDip = ClampPositionDip (m_positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::Paint
//
//  A HAIRLINE OVER A WIDE GRAB BAND. The sash rect is the area the pointer can
//  grab; the drawing is two one-pixel lines down its middle, a dark one and a
//  lighter one beside it. With that pair, Explorer's sash appears in relief
//  (measured at 120 dpi: #202020 and #2B2B2B). A splitter that fills its whole grab band
//  with one flat gray looks like a bar, not a seam.
//
//  There is NO hover fill. Explorer indicates a draggable sash only by
//  changing the cursor.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSplitter::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   sash     = GetSashRect();
    float  line     = (std::max) (m_scaler.ToPxf (1.0f), 1.0f);
    bool   vertical = (m_orientation == Orientation::Vertical);
    float  midX     = (float) sash.left + (float) (sash.right - sash.left) * 0.5f - line;
    float  midY     = (float) sash.top  + (float) (sash.bottom - sash.top)  * 0.5f - line;



    UNREFERENCED_PARAMETER (text);

    //  The band either side of the lines belongs to the panes, not to the
    //  window behind them: in Explorer the two content surfaces run right up
    //  to the seam, and a strip of panel color showing through is what makes
    //  a hairline read as a bar after all.
    painter.FillRect ((float) sash.left, (float) sash.top,
                      (float) (sash.right - sash.left), (float) (sash.bottom - sash.top),
                      theme.ContentBackground());

    if (vertical)
    {
        painter.FillRect (midX,        (float) sash.top, line, (float) (sash.bottom - sash.top), theme.ContentEdge());
        painter.FillRect (midX + line, (float) sash.top, line, (float) (sash.bottom - sash.top), theme.SplitterHighlight());
    }
    else
    {
        painter.FillRect ((float) sash.left, midY,        (float) (sash.right - sash.left), line, theme.ContentEdge());
        painter.FillRect ((float) sash.left, midY + line, (float) (sash.right - sash.left), line, theme.SplitterHighlight());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::OnMouse
//
//  A press on the sash starts a drag that keeps the grab point under the
//  pointer; moves follow until the release. Moves elsewhere only update the
//  hover and are not consumed, so the panes beside the sash still get them.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSplitter::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  sash    = GetSashRect();
    bool  over    = IsOverSash (ev.positionDip);
    int   along   = (m_orientation == Orientation::Vertical) ? ev.positionDip.x : ev.positionDip.y;
    int   start   = (m_orientation == Orientation::Vertical) ? m_boundsDip.left : m_boundsDip.top;
    int   sashAt  = (m_orientation == Orientation::Vertical) ? sash.left : sash.top;
    bool  handled = false;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Down:
            if (over && ev.button == DxuiMouseButton::Left)
            {
                m_dragging     = true;
                m_dragOffsetPx = along - sashAt;
                handled        = true;
            }

            break;

        case DxuiMouseEventKind::Move:
            m_hovered = over;

            if (m_dragging)
            {
                MoveTo (MulDiv (along - m_dragOffsetPx - start, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi()));
                handled = true;
            }

            break;

        case DxuiMouseEventKind::Up:
            handled    = m_dragging;
            m_dragging = false;
            break;

        case DxuiMouseEventKind::Leave:
            m_hovered = false;
            break;

        default:
            break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSplitter::OnKey (const DxuiKeyEvent & ev)
{
    bool    vertical = m_orientation == Orientation::Vertical;
    WPARAM  back     = vertical ? VK_LEFT  : VK_UP;
    WPARAM  ahead    = vertical ? VK_RIGHT : VK_DOWN;



    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    if (ev.vk == back)
    {
        MoveTo (m_positionDip - kKeyStepDip);
        return true;
    }

    if (ev.vk == ahead)
    {
        MoveTo (m_positionDip + kKeyStepDip);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiSplitter::GetCursorForPoint (POINT clientPx) const
{
    if (!IsOverSash (clientPx) && !m_dragging)
    {
        return nullptr;
    }

    return (m_orientation == Orientation::Vertical) ? IDC_SIZEWE : IDC_SIZENS;
}
