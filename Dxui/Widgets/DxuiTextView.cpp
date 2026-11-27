#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_layout.SetRows (std::move (rows));
    m_selection.Reset();
    m_pictures.ClearOutline();
    m_scroll.ResetTopLine();
    m_scroll.ResetLeftColumn();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetProportional
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetProportional (bool proportional)
{
    if (m_layout.SetProportional (proportional))
    {
        Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetNoWrap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetNoWrap (bool noWrap)
{
    if (!m_layout.SetNoWrap (noWrap))
    {
        return;
    }

    m_scroll.ResetLeftColumn();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetCellSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetCellSize (int widthPx, int heightPx)
{
    m_layout.SetCellSize (widthPx, heightPx);

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextColumns
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextColumns (bool scrollbar) const
{
    int  width = (int) (m_boundsDip.right - m_boundsDip.left) - m_scaler.ToPx (s_kPadDip) * 2;



    width -= scrollbar ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;

    return (m_layout.GetCellWidthPx() > 0) ? (std::max) (1, (int) ((float) width / m_layout.GetCellAdvance())) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Rebuild
//
//  The drawn lines, laid out again narrower if they turn out to require the
//  scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Rebuild()
{
    const std::vector<DxuiTextLayout::Line> &  lines   = m_layout.GetLines();
    int                                        topLine = m_scroll.GetTopLine();
    DxuiTextLayout::Line                       top     = (topLine >= 0 && topLine < (int) lines.size()) ? lines[(size_t) topLine] : DxuiTextLayout::Line();
    float                                      perDip  = m_scaler.ToPxf (1.0f);



    m_layout.ClearLines();
    m_scroll.SetHorzScrollbarVisible (false);

    if (!m_layout.HasCells())
    {
        return;
    }

    //  Proportional text is laid out once its characters are measured, by
    //  their places rather than by cells.
    if (m_layout.IsProportional())
    {
        if (!m_layout.IsMeasured())
        {
            return;
        }

        m_layout.BuildProportionalLines ((float) GetTextColumns (false) * m_layout.GetCellAdvance(), perDip);

        if (m_scroll.IsScrollbarVisible())
        {
            m_layout.BuildProportionalLines ((float) GetTextColumns (true) * m_layout.GetCellAdvance(), perDip);
        }
    }
    else
    {
        m_layout.BuildColumnCells();
        m_layout.BuildLines (GetTextColumns (false), perDip);

        if (m_scroll.IsScrollbarVisible())
        {
            m_layout.BuildLines (GetTextColumns (true), perDip);
        }

        //  The horizontal bar takes a line of height, which can bring on the
        //  vertical bar, which takes columns, so the layout runs twice.
        for (int pass = 0; m_layout.IsNoWrap() && pass < 2; pass++)
        {
            m_scroll.SetHorzScrollbarVisible (m_layout.GetContentColumns() > GetTextColumns (m_scroll.IsScrollbarVisible()));
        }
    }

    //  A new wrap moves every line, so the top is the line that now has the
    //  text the old top started with.
    m_scroll.SetTopLine (m_layout.FindLine (top));
    m_scroll.SetLeftColumn (m_scroll.GetLeftColumn());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetZoom
//
//  A new size is measured again on the next paint, unless the host set the
//  cells itself.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetZoom (float zoom)
{
    if (zoom == m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_zoom = zoom;
    m_layout.ClearMeasurement();
    m_layout.ResetCellSize();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    if (!m_layout.IsCellPinned() && m_layout.GetMeasuredDpi() != scaler.GetDpi())
    {
        m_layout.ResetCellSize();
        m_layout.ClearMeasurement();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Measure
//
//  What Paint measures, done ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Measure (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();



    EnsureCellSize (text, theme);

    if (m_layout.IsProportional() && !m_layout.IsMeasured() && m_layout.GetCellWidthPx() > 0)
    {
        font.sizeDip = m_scaler.ToPxf (font.sizeDip * m_zoom);

        m_layout.MeasureCharacters (text, font);
        Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetContentHeightPx
//
//  Zero before the text is measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetContentHeightPx() const
{
    int  bar   = m_scroll.IsHorzScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;
    int  lineH = m_layout.GetCellHeightPx();



    return (lineH > 0) ? m_layout.GetLineCount() * lineH + m_scaler.ToPx (s_kPadDip) * 2 + bar : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::EnsureCellSize
//
//  Measured at the size the face is drawn at, so cells and glyphs match.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();



    if (m_layout.MeasureCellSize (text, m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, m_scaler.GetDpi()))
    {
        Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.ContentBackground());

    if (m_layout.GetCellWidthPx() <= 0 || m_layout.GetCellHeightPx() <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);

    if (m_layout.IsProportional() && !m_layout.IsMeasured() && m_layout.GetCellWidthPx() > 0)
    {
        m_layout.MeasureCharacters (text, font);
        Rebuild();
    }

    m_painter.Paint (painter, text, theme, font, m_iconFace, m_textStrength);
    m_scroll.Paint (painter, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::DoColumnsFit
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::DoColumnsFit() const
{
    return m_layout.DoColumnsFit ((float) GetTextColumns (m_scroll.IsScrollbarVisible()) * m_layout.GetCellAdvance());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnMouse
//
//  The wheel and a press on a scrollbar scroll, the wheel and a drag on a
//  map picture zoom and pan it, and a press and drag elsewhere select.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point   = ev.positionDip;
    bool   handled = false;
    bool   onBar   = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        //  Ctrl with the wheel over a zoomable picture zooms it, so the wheel
        //  alone scrolls past a map.
        handled = (ev.ctrl && !ev.wheelHorizontal && m_pictures.Zoom (point, ev.wheelDelta)) || m_scroll.OnWheel (ev);
        break;

    case DxuiMouseEventKind::Down:
        onBar = ev.button == DxuiMouseButton::Left && m_scroll.TryStartDrag (point, handled);

        if (ev.button == DxuiMouseButton::Right)
        {
            if (m_onContextMenu)
            {
                m_onContextMenu (point);
            }

            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && !onBar && GetLinkRowAt (point) >= 0)
        {
            m_pressedLink = GetLinkRowAt (point);
            handled       = true;
        }
        else if (ev.button == DxuiMouseButton::Left && !onBar && m_pictures.TryStartPan (point))
        {
            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && !onBar)
        {
            m_selection.OnPress (point, ev.shift, (int64_t) GetTickCount64());
            handled = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_pictures.IsPanning())
        {
            m_pictures.Pan (point);
            handled = true;
        }
        else if (!m_scroll.TryDrag (point, handled) && m_selection.OnDrag (point))
        {
            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_pressedLink >= 0)
        {
            if (GetLinkRowAt (point) == m_pressedLink)
            {
                m_onRowClick (m_pressedLink);
            }

            m_pressedLink = -1;
            handled       = true;
        }
        else if (m_pictures.IsPanning())
        {
            m_pictures.EndPan();
            handled = true;
        }
        else if (!m_scroll.TryEndDrag (handled) && m_selection.OnRelease())
        {
            handled = true;
        }

        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiTextView::GetCursorForPoint (POINT clientPx) const
{
    return (GetLinkRowAt (clientPx) >= 0) ? IDC_HAND : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinkRowAt
//
//  The link row whose lines are under the point, or -1. A row is a link
//  only while a row click function is set.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLinkRowAt (POINT point) const
{
    POINT                        origin = GetTextOrigin();
    int                          lineH  = m_layout.GetCellHeightPx();
    int                          shown  = m_layout.GetLineCount() - m_scroll.GetTopLine();
    DxuiTextSelection::Position  pos;
    bool                         inside = false;



    inside = m_onRowClick && lineH > 0
          && point.x >= m_boundsDip.left && point.x < m_boundsDip.right
          && point.y >= origin.y && point.y < origin.y + shown * lineH;

    if (!inside)
    {
        return -1;
    }

    pos = m_selection.HitTest (point);

    return (pos.row >= 0 && pos.row < (int) m_layout.GetRows().size() && m_layout.GetRows()[(size_t) pos.row].link) ? pos.row : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    bool  handled = false;



    if (command == DxuiStandardCommand::Copy)
    {
        outEnabled = m_selection.HasSelection();
        handled    = true;
    }
    else if (command == DxuiStandardCommand::SelectAll)
    {
        outEnabled = !m_layout.GetRows().empty();
        handled    = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        m_selection.CopySelection (m_hwnd);
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        m_selection.SelectAll();
    }

    return handled;
}
