#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView::OnChar
//
//  A typed byte moves the caret to the next byte, scrolled into view.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexView::OnChar (wchar_t ch)
{
    uint64_t  caret     = GetCaret();
    bool      isHex     = GetActiveColumn() != Column::Text;
    bool      isHigh    = false;
    bool      isChanged = false;



    if (!m_onType || GetSource() == nullptr || caret >= GetSource()->GetByteCount())
    {
        return false;
    }

    if (m_isLowNibble && m_nibbleAt != caret)
    {
        m_isLowNibble = false;
    }

    isHigh    = isHex && !m_isLowNibble;
    isChanged = m_onType (caret, isHex, isHigh, ch);

    if (!isChanged)
    {
        return false;
    }

    if (isHigh)
    {
        m_isLowNibble = true;
        m_nibbleAt    = caret;
        return true;
    }

    m_isLowNibble = false;

    if (caret + 1 < GetSource()->GetByteCount())
    {
        GoToOffset (caret + 1);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexView::OnKey (const DxuiKeyEvent & ev)
{
    bool  isNavigation = ev.kind == DxuiKeyEventKind::Down
                      && (ev.vk == VK_PRIOR || ev.vk == VK_NEXT || (ev.alt && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT)));



    if (isNavigation && m_onNavigate && m_onNavigate (ev))
    {
        m_isLowNibble = false;
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        m_isLowNibble = false;
    }

    return DxuiHexView::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexView::OnMouse (const DxuiMouseEvent & ev)
{
    HitResult  hit       = HitTestPoint (ev.positionDip);
    bool       isHandled = DxuiHexView::OnMouse (ev);



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hit.hit)
    {
        m_isLowNibble = false;

        if (m_onClick && ev.ctrl)
        {
            m_onClick (hit.offset);
        }
    }

    return isHandled;
}
