#include "Pch.h"

#include "Widgets/HexView/DxuiHexSelection.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/HexView/DxuiHexViewTypes.h"
#include "Core/DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::DxuiHexSelection
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexSelection::DxuiHexSelection (DxuiHexView & view)
    : m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::SelectByte
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::SelectByte (uint64_t offset, DxuiHexViewTypes::Column column)
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    m_anchor                = offset;
    m_caret        = offset;
    m_hasSelection = true;

    if (column != DxuiHexViewTypes::Column::None)
    {
        m_activeColumn = column;
    }

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::ExtendSelectionTo
//
//  Only the moving end moves. Extending before anything was selected starts
//  the run where it lands, so Shift with a click into an empty view behaves
//  like an ordinary click.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::ExtendSelectionTo (uint64_t offset)
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    if (!m_hasSelection)
    {
        SelectByte (offset, m_activeColumn);
        return;
    }

    m_caret = offset;

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::SelectAll
//
//  Every byte, which is the same run whichever column the user is in. What
//  differs is what it means: in the hex column Ctrl+A then Ctrl+C yields the
//  digits, in the text column the characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::SelectAll()
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;



    if (bytes == 0)
    {
        ClearSelection();
        return;
    }

    m_anchor                = 0;
    m_caret        = bytes - 1;
    m_hasSelection = true;

    NotifySelectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::ClearSelection()
{
    bool  had = m_hasSelection;



    m_hasSelection = false;
    m_anchor                = 0;
    m_caret        = 0;

    if (had)
    {
        NotifySelectionChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::GetSelectionFirst
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexSelection::GetSelectionFirst() const
{
    return (std::min) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::GetSelectionLast
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexSelection::GetSelectionLast() const
{
    return (std::max) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::GetSelectionCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexSelection::GetSelectionCount() const
{
    if (!m_hasSelection)
    {
        return 0;
    }

    return GetSelectionLast() - GetSelectionFirst() + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::IsByteSelected
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSelection::IsByteSelected (uint64_t offset) const
{
    return m_hasSelection
        && (offset >= GetSelectionFirst())
        && (offset <= GetSelectionLast());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::SetActiveColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::SetActiveColumn (DxuiHexViewTypes::Column column)
{
    if (column != DxuiHexViewTypes::Column::None)
    {
        m_activeColumn = column;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::GetSelectionText
//
//  The selection in the active column's format: hex digits or characters. A
//  single Copy command covers both, based on the active column rather than a
//  separate menu command for each format.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexSelection::GetSelectionText() const
{
    uint64_t  first = GetSelectionFirst();
    uint64_t  count = GetSelectionCount();



    if (count == 0)
    {
        return std::wstring();
    }

    //  With the values hidden, the text column is the only one there is.
    return (m_activeColumn == DxuiHexViewTypes::Column::Text || !m_view.m_showValues) ? m_view.m_painter.GetTextFor (first, count)
                                                             : m_view.m_painter.GetHexFor  (first, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::CopySelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::CopySelection() const
{
    DxuiClipboard::SetText (m_view.m_hwnd, GetSelectionText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::NotifySelectionChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::NotifySelectionChanged()
{
    if (m_view.m_onSelectionChanged)
    {
        m_view.m_onSelectionChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::MoveCaretTo
//
//  Keyboard movement does not change the active column, so moving the caret
//  out of a run selected in the text column does not switch to the hex column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::MoveCaretTo (uint64_t offset, bool extend)
{
    uint64_t  bytes = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  last  = m_view.GetLastOffset();



    if (bytes == 0)
    {
        return;
    }

    offset = (std::min) (offset, last);

    if (extend)
    {
        ExtendSelectionTo (offset);
    }
    else
    {
        SelectByte (offset, m_activeColumn);
    }

    m_view.m_scroll.EnsureByteVisible (offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::OnPress
//
//  A left press selects the byte under it, or with Shift extends the run to
//  it, and starts a drag. In the value column a press takes the whole value
//  under it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::OnPress (const DxuiMouseEvent & ev, const DxuiHexViewTypes::HitResult & hit)
{
    SetActiveColumn (hit.column);

    if (ev.shift)
    {
        ExtendSelectionTo (hit.offset);
    }
    else if ((hit.column == DxuiHexViewTypes::Column::Hex) && (m_view.m_grouping > 1))
    {
        m_pressFirst = m_view.m_layout.SnapToValue (hit.offset, false);
        m_pressLast  = m_view.m_layout.SnapToValue (hit.offset, true);

        SelectByte (m_pressFirst, hit.column);
        ExtendSelectionTo (m_pressLast);
    }
    else
    {
        SelectByte (hit.offset, hit.column);
    }

    m_dragging = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::OnContextPress
//
//  A right-click inside the selection leaves it unchanged; outside it, the
//  clicked byte is selected.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSelection::OnContextPress (const DxuiHexViewTypes::HitResult & hit)
{
    SetActiveColumn (hit.column);

    if (!IsByteSelected (hit.offset))
    {
        SelectByte (hit.offset, hit.column);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::OnDrag
//
//  Extends the run to the byte under the pointer. Back from the value the
//  press took, the anchor is that value's last byte, so the selection covers
//  all of it. Returns whether a drag was in progress.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSelection::OnDrag (const DxuiMouseEvent & ev)
{
    DxuiHexViewTypes::HitResult  hit;



    if (!m_dragging)
    {
        return false;
    }

    hit = m_view.m_layout.HitTestPoint (ev.positionDip);

    if (hit.hit && (hit.column == DxuiHexViewTypes::Column::Hex) && (m_view.m_grouping > 1) && !ev.shift)
    {
        m_anchor = (hit.offset >= m_pressFirst) ? m_pressFirst : m_pressLast;
        ExtendSelectionTo (m_view.m_layout.SnapToValue (hit.offset, hit.offset >= m_pressFirst));
        m_view.m_scroll.EnsureByteVisible (hit.offset);
    }
    else if (hit.hit)
    {
        ExtendSelectionTo (hit.offset);
        m_view.m_scroll.EnsureByteVisible (hit.offset);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::OnRelease
//
//  Returns whether a drag was in progress.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSelection::OnRelease()
{
    if (!m_dragging)
    {
        return false;
    }

    m_dragging = false;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSelection::OnKey
//
//  The arrows move by a byte or a row, Home and End move to the ends of the
//  row (with Ctrl, of the whole source), Page Up and Page Down move by the
//  visible row count, and Shift with any of them extends the selection. Tab
//  moves between the columns while another column remains, and returns
//  false after the last one, so focus moves on to the next control instead
//  of staying in the view.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSelection::OnKey (const DxuiKeyEvent & ev)
{
    uint64_t  caret  = m_caret;
    uint64_t  perRow = (uint64_t) m_view.m_bytesPerRow;
    uint64_t  page   = (uint64_t) (std::max) (m_view.m_layout.GetRowCap(), 1) * perRow;
    uint64_t  last   = m_view.GetLastOffset();
    bool      extend = ev.shift;



    //  Line mode's rows are not a fixed number of bytes, so the keys that move
    //  by rows find the row above or below instead of subtracting a width.
    if (m_view.IsLineMode() && (ev.vk == VK_UP || ev.vk == VK_DOWN || ev.vk == VK_PRIOR || ev.vk == VK_NEXT || ev.vk == VK_HOME || ev.vk == VK_END))
    {
        MoveCaretTo (m_view.m_layout.GetLineModeTarget (ev.vk, ev.ctrl), extend);
        return true;
    }

    switch (ev.vk)
    {
    case VK_LEFT:
        MoveCaretTo ((caret > 0) ? (caret - 1) : 0, extend);
        return true;

    case VK_RIGHT:
        MoveCaretTo (caret + 1, extend);
        return true;

    case VK_UP:
        MoveCaretTo ((caret >= perRow) ? (caret - perRow) : caret, extend);
        return true;

    case VK_DOWN:
        MoveCaretTo (((caret + perRow) <= last) ? (caret + perRow) : caret, extend);
        return true;

    case VK_PRIOR:
        MoveCaretTo ((caret >= page) ? (caret - page) : (caret % perRow), extend);
        return true;

    case VK_NEXT:
        MoveCaretTo (caret + page, extend);
        return true;

    case VK_HOME:
        MoveCaretTo (ev.ctrl ? 0 : (caret - (caret % perRow)), extend);
        return true;

    case VK_END:
        MoveCaretTo (ev.ctrl ? last : (caret - (caret % perRow) + perRow - 1), extend);
        return true;

    case VK_TAB:
        if (ev.ctrl || !m_view.m_showValues)
        {
            return false;
        }

        if (!ev.shift && (m_activeColumn == DxuiHexViewTypes::Column::Hex))
        {
            SetActiveColumn (DxuiHexViewTypes::Column::Text);
            return true;
        }

        if (ev.shift && (m_activeColumn == DxuiHexViewTypes::Column::Text))
        {
            SetActiveColumn (DxuiHexViewTypes::Column::Hex);
            return true;
        }

        return false;

    default:
        break;
    }

    return false;
}
