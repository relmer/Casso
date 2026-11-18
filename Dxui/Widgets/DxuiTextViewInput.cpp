#include "Pch.h"

#include "DxuiTextViewInput.h"
#include "DxuiScrollbar.h"
#include "DxuiTextLayout.h"
#include "DxuiTextViewSelection.h"
#include "IDxuiTextInputHost.h"
#include "IDxuiTextScroller.h"
#include "Core/DxuiSystemSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnMouse
//
//  A press places both ends of the selection, or moves only the caret with
//  Shift, and a drag moves the caret, scrolling past either edge. A second
//  press at the same place within the double-click time selects the word.
//  A release on the link the press began on, with nothing selected, is a
//  click on the link.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        handled = OnWheel (ev);
        break;

    case DxuiMouseEventKind::Down:
        handled = OnPress (ev);
        break;

    case DxuiMouseEventKind::Move:
        handled = OnDragMove (ev.positionDip);
        break;

    case DxuiMouseEventKind::Up:
        handled = OnRelease (ev.positionDip);
        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnWheel
//
//  The horizontal wheel's positive turn is to the right, the opposite of the
//  Shift+wheel's, and wrapped text doesn't scroll sideways at all. A
//  precision touchpad sends part notches, which add up until they move a
//  whole line or column.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnWheel (const DxuiMouseEvent & ev)
{
    bool  noWrap = m_layout.IsNoWrap();



    if (ev.wheelHorizontal || (ev.shift && noWrap))
    {
        if (noWrap)
        {
            m_wheelColumns += (ev.wheelHorizontal ? ev.wheelDelta : -ev.wheelDelta) * (float) kWheelColumns;
            m_scroller.ScrollColumns ((int) m_wheelColumns);
            m_wheelColumns -= (float) (int) m_wheelColumns;
        }
    }
    else
    {
        m_wheelLines += -ev.wheelDelta * (float) kWheelLines;
        m_scroller.ScrollLines ((int) m_wheelLines);
        m_wheelLines -= (float) (int) m_wheelLines;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnPress
//
//  A right press opens the context menu; a left press on a scroll bar
//  goes to the bar. A triple-click selects everything, as in a browser's
//  text; a double-click selects the word, and dragging on from it takes
//  whole words until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnPress (const DxuiMouseEvent & ev)
{
    POINT            point    = ev.positionDip;
    int64_t          nowMs    = (int64_t) GetTickCount64();
    DxuiScrollbar  * bar      = m_host.GetInputBarAt (point);
    bool             handled  = false;
    bool             twice    = false;
    bool             thrice   = false;
    int              linkRow  = -1;
    int              linkSpan = -1;
    bool             isOnLink = false;
    Position         pos;



    if (ev.button == DxuiMouseButton::Right)
    {
        m_host.ShowInputMenu (point);
        return true;
    }

    if (ev.button == DxuiMouseButton::Left && bar == &m_vertBar)
    {
        handled = m_vertBar.OnMouseDown (point.x, point.y);
        m_scroller.SetTopLine (m_vertBar.GetScrollPos());
        return handled;
    }

    if (ev.button == DxuiMouseButton::Left && bar == &m_horzBar)
    {
        handled = m_horzBar.OnMouseDown (point.x, point.y);
        m_scroller.SetLeftColumn (m_horzBar.GetScrollPos());
        return handled;
    }

    if (ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    pos    = m_host.HitTestInput (point);
    twice  = (nowMs - m_lastClickMs) <= (int64_t) DxuiSystemSettings::Instance().GetDoubleClickMs() && pos == m_lastClick;
    thrice = twice && m_clicks == 2;

    m_clicks      = twice ? m_clicks + 1 : 1;
    m_lastClickMs = nowMs;
    m_lastClick   = pos;
    m_wordDrag    = twice && !thrice;

    m_host.FocusRowOnPress (pos.row);

    isOnLink    = !twice && !ev.shift && m_host.TryGetInputLink (point, linkRow, linkSpan);
    m_pressRow  = isOnLink ? linkRow  : -1;
    m_pressSpan = isOnLink ? linkSpan : -1;

    if (thrice)
    {
        m_selection.SelectAll();
        m_clicks   = 0;
        m_dragging = false;
    }
    else if (twice)
    {
        m_selection.GetWordBounds (pos, m_wordFirst, m_wordLast);
        m_selection.Select (m_wordFirst, m_wordLast);
        m_dragging = true;
    }
    else if (ev.shift)
    {
        m_selection.SetCaret (pos);
        m_dragging = true;
    }
    else
    {
        m_selection.Select (pos, pos);
        m_dragging = true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnDragMove
//
//  A scroll bar's drag goes to the bar; a selection's moves the caret, by
//  whole words after a double-click, scrolling past either edge.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnDragMove (POINT point)
{
    DxuiScrollbar  & vert    = m_vertBar;
    DxuiScrollbar  & horz    = m_horzBar;
    RECT             bounds  = m_host.GetInputBounds();
    bool             noWrap  = m_layout.IsNoWrap();
    bool             handled = false;
    Position         pos;
    Position         first;
    Position         last;



    if (vert.IsDragging())
    {
        handled = vert.OnMouseMove (point.x, point.y);
        m_scroller.SetTopLine (vert.GetScrollPos());
        return handled;
    }

    if (horz.IsDragging())
    {
        handled = horz.OnMouseMove (point.x, point.y);
        m_scroller.SetLeftColumn (horz.GetScrollPos());
        return handled;
    }

    if (!m_dragging)
    {
        return false;
    }

    if (point.y < bounds.top)
    {
        m_scroller.ScrollLines (-1);
    }
    else if (point.y >= bounds.bottom)
    {
        m_scroller.ScrollLines (1);
    }

    if (noWrap && point.x < m_host.GetInputTextLeft())
    {
        m_scroller.SetLeftColumn (m_scroller.GetLeftColumn() - 1);
    }
    else if (noWrap && point.x >= bounds.right)
    {
        m_scroller.SetLeftColumn (m_scroller.GetLeftColumn() + 1);
    }

    pos = m_host.HitTestInput (point);

    if (m_wordDrag)
    {
        m_selection.GetWordBounds (pos, first, last);
        m_selection.Select ((pos < m_wordFirst) ? m_wordLast : m_wordFirst,
                            (pos < m_wordFirst) ? first      : (std::max) (last, m_wordLast));
    }
    else
    {
        m_selection.SetCaret (pos);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnRelease
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnRelease (POINT point)
{
    DxuiScrollbar  & vert     = m_vertBar;
    DxuiScrollbar  & horz     = m_horzBar;
    int              linkRow  = -1;
    int              linkSpan = -1;
    bool             isOnLink = false;



    if (vert.IsDragging())
    {
        return vert.OnMouseUp();
    }

    if (horz.IsDragging())
    {
        return horz.OnMouseUp();
    }

    if (!m_dragging)
    {
        return false;
    }

    m_dragging = false;
    isOnLink   = m_pressRow >= 0 && !m_selection.HasSelection() && m_host.TryGetInputLink (point, linkRow, linkSpan) &&
                 linkRow == m_pressRow && linkSpan == m_pressSpan;
    m_pressRow = -1;

    if (isOnLink)
    {
        m_host.ClickInputLink (linkRow, linkSpan);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::OnKey
//
//  The arrow and page keys scroll; there is no caret to move in read-only
//  text. With row focus on, they move the focused row instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::OnKey (const DxuiKeyEvent & ev)
{
    //  With Ctrl or Alt down the key is a shortcut, left to the host, but for
    //  the keys that move the focused row.
    bool  handled = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_host.TryMoveInputRow (ev.vk, ev.ctrl))
    {
        return true;
    }

    if (!handled)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_UP:    m_scroller.ScrollLines (-1);                          break;
    case VK_DOWN:  m_scroller.ScrollLines (1);                           break;
    case VK_PRIOR: m_scroller.ScrollLines (-m_scroller.GetLineCap());    break;
    case VK_NEXT:  m_scroller.ScrollLines (m_scroller.GetLineCap());     break;
    case VK_HOME:  m_scroller.SetTopLine (0);                            break;
    case VK_END:   m_scroller.SetTopLine (m_layout.GetLineCount());      break;
    default:       handled = false;                                      break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewInput::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
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
//  DxuiTextViewInput::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewInput::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        m_host.CopyInputSelection();
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        m_selection.SelectAll();
    }

    return handled;
}
