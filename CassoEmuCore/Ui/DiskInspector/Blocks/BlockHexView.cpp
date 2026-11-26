#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView::OnKey
//
//  A reported navigation key starts the next byte at its high nibble.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexView::OnKey (const DxuiKeyEvent & ev)
{
    bool  isNavigation = ev.kind == DxuiKeyEventKind::Down
                      && (ev.vk == VK_PRIOR || ev.vk == VK_NEXT || (ev.alt && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT)));



    if (isNavigation && m_onNavigate && m_onNavigate (ev))
    {
        GetHostEdit().ResetNibble();
        return true;
    }

    return DxuiHexView::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView::OnMouse
//
//  A second press on the same byte within the double-click time is a
//  double-click.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexView::OnMouse (const DxuiMouseEvent & ev)
{
    HitResult  hit       = HitTestPoint (ev.positionDip);
    bool       isHandled = DxuiHexView::OnMouse (ev);
    int64_t    nowMs     = (int64_t) GetTickCount64();
    bool       isDouble  = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hit.hit)
    {
        isDouble  = m_pressMs != 0 && hit.offset == m_pressAt && nowMs - m_pressMs <= (int64_t) GetDoubleClickTime();
        m_pressAt = hit.offset;
        m_pressMs = isDouble ? 0 : nowMs;

        if (m_onClick && ev.ctrl)
        {
            m_onClick (hit.offset);
        }
        else if (m_onOpen && isDouble)
        {
            m_onOpen (hit.offset);
        }
    }

    return isHandled;
}
