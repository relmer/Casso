#include "Pch.h"

#include "Widgets/DxuiReorderStack.h"
#include "Core/DxuiAnimation.h"
#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::SetRows (const std::vector<IDxuiControl *> & rows)
{
    ClearAdopted();

    m_rows = rows;

    for (IDxuiControl * row : m_rows)
    {
        Adopt (*row);
    }

    m_movable = std::min (m_movable, m_rows.size());
    m_dragged = -1;

    SnapSlides();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::SetMovableCount
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::SetMovableCount (size_t count)
{
    m_movable = std::min (count, m_rows.size());
    m_dragged = -1;

    SnapSlides();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::MeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiReorderStack::MeasureHeightPx (const DxuiDpiScaler & scaler) const
{
    int  rows = (int) m_rows.size();



    return rows == 0 ? 0 : rows * scaler.ToPx (m_rowHeightDip) + (rows - 1) * scaler.ToPx (kRowGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::IsAnimating
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiReorderStack::IsAnimating() const
{
    int64_t  now = GetNowMs();



    if (m_dragged >= 0)
    {
        return true;
    }

    return m_animate && std::any_of (m_slides.begin(), m_slides.end(), [now] (const Slide & slide) { return slide.from != slide.to && now - slide.startMs < kSlideMs; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    bool  moved = !EqualRect (&boundsDip, &m_bounds) || scaler.GetDpi() != m_scaler.GetDpi();



    SetBounds (boundsDip);

    m_bounds = boundsDip;
    m_scaler = scaler;

    //  The slots move with the stack, so the rows go straight to them.
    if (moved && m_dragged < 0)
    {
        SnapSlides();
    }

    LayoutRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::Paint
//
//  The dragged row is drawn last, over the rows it passes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  lifted = m_dragged >= 0 ? GetRowRect (GetRowTop ((size_t) m_dragged)) : RECT {};



    LayoutRows();

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        RECT  box     = GetRowRect (GetRowTop (row));
        RECT  visible = box;

        if ((int) row == m_dragged)
        {
            continue;
        }

        //  Text is drawn after every fill and outline, so the dragged row's
        //  box can't cover a row it passes. The row is clipped to the part the
        //  dragged row doesn't cover instead.
        if (m_dragged >= 0 && lifted.top < box.bottom && lifted.bottom > box.top)
        {
            visible.top    = lifted.top    <= box.top    ? lifted.bottom : box.top;
            visible.bottom = lifted.bottom >= box.bottom ? lifted.top    : box.bottom;
        }

        if (visible.bottom <= visible.top)
        {
            continue;
        }

        PushClip (painter, text, visible);
        PaintRow (painter, text, theme, row, GetRowTop (row));
        PopClip  (painter, text);
    }

    if (m_dragged >= 0)
    {
        PaintRow (painter, text, theme, (size_t) m_dragged, GetRowTop ((size_t) m_dragged));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::OnMouse
//
//  A press on a movable row's gripper lifts the row. Anything else goes to
//  the rows' controls.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiReorderStack::OnMouse (const DxuiMouseEvent & ev)
{
    int  row  = -1;
    int  from = m_dragged;
    int  to   = m_gapSlot;



    if (m_dragged >= 0 && ev.kind == DxuiMouseEventKind::Move)
    {
        m_pointerY = ev.positionDip.y;
        RetargetSlides();
        return true;
    }

    if (m_dragged >= 0 && ev.kind == DxuiMouseEventKind::Up)
    {
        Slide  settle = { GetRowTop ((size_t) m_dragged), (float) GetSlotTop ((size_t) to), GetNowMs() };



        m_dragged = -1;
        m_gapSlot = -1;

        SnapSlides();

        if (from != to && m_reorder)
        {
            m_reorder ((size_t) from, (size_t) to);
        }

        //  The dropped row's data is now at `to`, and slides there from where
        //  it was let go.
        m_slides[(size_t) to] = settle;
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        row = FindGripRow (ev.positionDip);
    }

    if (row >= 0)
    {
        //  Measured before the row is lifted, while it is at its slot.
        m_grabOffset = ev.positionDip.y - (int) GetRowTop ((size_t) row);
        m_dragged    = row;
        m_gapSlot    = row;
        m_pointerY   = ev.positionDip.y;

        RetargetSlides();
        return true;
    }

    return DxuiPanel::OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiReorderStack::GetCursorForPoint (POINT clientPx) const
{
    if (m_dragged >= 0 || FindGripRow (clientPx) >= 0)
    {
        return IDC_SIZENS;
    }

    return DxuiPanel::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiReorderStack::GetNowMs() const
{
    return m_clock ? m_clock() : (int64_t) GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetPitchPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiReorderStack::GetPitchPx() const
{
    return m_scaler.ToPx (m_rowHeightDip) + m_scaler.ToPx (kRowGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetSlotTop
//
////////////////////////////////////////////////////////////////////////////////

int DxuiReorderStack::GetSlotTop (size_t slot) const
{
    return m_bounds.top + (int) slot * GetPitchPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetRowTop
//
//  The dragged row follows the pointer, clamped to the movable rows' slots.
//  Any other row is where its slide has it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiReorderStack::GetRowTop (size_t row) const
{
    const Slide  & slide   = m_slides[row];
    int64_t        elapsed = GetNowMs() - slide.startMs;
    int            highest = GetSlotTop (0);
    int            lowest  = GetSlotTop (m_movable == 0 ? 0 : m_movable - 1);



    if ((int) row == m_dragged)
    {
        return (float) std::clamp (m_pointerY - m_grabOffset, highest, lowest);
    }

    if (!m_animate || slide.from == slide.to || elapsed >= kSlideMs || elapsed < 0)
    {
        return slide.to;
    }

    return slide.from + (slide.to - slide.from) * DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, (float) elapsed / (float) kSlideMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetRowRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiReorderStack::GetRowRect (float top) const
{
    int  y = (int) std::lround (top);



    return RECT { m_bounds.left, y, m_bounds.right, y + m_scaler.ToPx (m_rowHeightDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetGripRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiReorderStack::GetGripRect (const RECT & row) const
{
    return RECT { row.left, row.top, row.left + m_scaler.ToPx (kGripDip), row.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::FindGripRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiReorderStack::FindGripRow (POINT point) const
{
    for (size_t row = 0; row < m_movable && m_movable > 1; row++)
    {
        RECT  grip = GetGripRect (GetRowRect (GetRowTop (row)));

        if (PtInRect (&grip, point))
        {
            return (int) row;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::GetGapSlot
//
//  The slot nearest the dragged row's top. The rows are of one height, so
//  the slot is the distance from the first one over the pitch, rounded.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiReorderStack::GetGapSlot() const
{
    float  top  = GetRowTop ((size_t) m_dragged);
    int    slot = (int) std::lround ((top - (float) GetSlotTop (0)) / (float) GetPitchPx());



    return std::clamp (slot, 0, (int) m_movable - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::RetargetSlides
//
//  The movable rows other than the dragged one close up around the gap: a
//  row above the dragged row's place moves down a slot when the gap is at or
//  above it, and a row below moves up a slot when the gap is at or below it.
//  A row whose place changes slides there from where it is drawn now.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::RetargetSlides()
{
    int64_t  now = GetNowMs();
    int      gap = GetGapSlot();



    m_gapSlot = gap;

    for (size_t row = 0; row < m_movable; row++)
    {
        int    slot   = (int) row;
        float  target = 0.0f;

        if ((int) row == m_dragged)
        {
            continue;
        }

        if ((int) row < m_dragged && (int) row >= gap)
        {
            slot++;
        }
        else if ((int) row > m_dragged && (int) row <= gap)
        {
            slot--;
        }

        target = (float) GetSlotTop ((size_t) slot);

        if (m_slides[row].to != target)
        {
            m_slides[row] = Slide { GetRowTop (row), target, now };
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::SnapSlides
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::SnapSlides()
{
    m_slides.resize (m_rows.size());

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        float  top = (float) GetSlotTop (row);

        m_slides[row] = Slide { top, top, 0 };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::LayoutRows
//
//  Each row's control in its box after the gripper, inset by the padding,
//  where the row is drawn now.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::LayoutRows()
{
    int  pad = m_scaler.ToPx (kPaddingDip);



    if (m_slides.size() != m_rows.size())
    {
        SnapSlides();
    }

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        RECT  box = GetRowRect (GetRowTop (row));

        m_rows[row]->Layout (RECT { box.left + m_scaler.ToPx (kGripDip), box.top + pad, box.right - pad, box.bottom - pad }, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::PushClip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::PushClip (IDxuiPainter & painter, IDxuiTextRenderer & text, const RECT & clip)
{
    HRESULT  hr = S_OK;



    painter.PushClipRect ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top));

    hr = text.PushClipRect ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top));
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::PopClip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::PopClip (IDxuiPainter & painter, IDxuiTextRenderer & text)
{
    HRESULT  hr = S_OK;



    painter.PopClipRect();

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack::PaintRow
//
//  A movable row is a filled box with a border and two columns of gripper
//  dots, drawn in the accent color while it is dragged. A row past the
//  movable count is an outline with no gripper.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiReorderStack::PaintRow (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, size_t row, float top) const
{
    RECT      box     = GetRowRect (top);
    RECT      grip    = GetGripRect (box);
    bool      movable = row < m_movable;
    bool      dragged = (int) row == m_dragged;
    float     corner  = m_scaler.ToPxf (kCornerDip);
    float     dot     = m_scaler.ToPxf (kDotDip);
    float     pitch   = m_scaler.ToPxf (kDotPitchDip);
    float     cx      = (float) (grip.left + grip.right) * 0.5f;
    float     cy      = (float) (grip.top + grip.bottom) * 0.5f;
    uint32_t  edge    = dragged ? theme.Accent() : theme.Border();



    if (movable)
    {
        painter.FillRoundedRect ((float) box.left, (float) box.top, (float) (box.right - box.left), (float) (box.bottom - box.top), corner, theme.ControlBackground());
    }

    painter.OutlineRoundedRect ((float) box.left, (float) box.top, (float) (box.right - box.left), (float) (box.bottom - box.top), corner, m_scaler.ToPxf (dragged ? kDraggedEdgeDip : kEdgeDip), edge);

    for (int i = 0; movable && m_movable > 1 && i < kGripDots; i++)
    {
        float  y = cy + (float) (i - kGripDots / 2) * pitch;

        painter.FillCircle (cx - pitch * 0.5f, y, dot, theme.ForegroundMuted());
        painter.FillCircle (cx + pitch * 0.5f, y, dot, theme.ForegroundMuted());
    }

    if (!movable)
    {
        painter.SetGlobalAlpha (kDimAlpha);
    }

    m_rows[row]->Paint (painter, text, theme);

    if (!movable)
    {
        painter.SetGlobalAlpha (1.0f);
    }
}
