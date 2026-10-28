#include "Pch.h"

#include "Ui/DiskInspector/Components/WalkthroughPart.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::OnWalkthroughMouse
//
//  While the walkthrough is open, a press on its buttons steps or closes it,
//  and every other event goes nowhere.
//
////////////////////////////////////////////////////////////////////////////////

bool WalkthroughPart::OnWalkthroughMouse (const DxuiMouseEvent & ev)
{
    bool  isPress   = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left;
    bool  isHandled = m_walkthrough.IsOpen();



    if (isHandled && isPress)
    {
        switch (m_walkthrough.HitTest (ev.positionDip))
        {
            case WalkthroughButton::Back:  m_walkthrough.Back();  break;
            case WalkthroughButton::Next:  m_walkthrough.Next();  break;
            case WalkthroughButton::Close: m_walkthrough.Close(); break;
            case WalkthroughButton::None:                         break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::OnWalkthroughKey
//
//  While the walkthrough is open: Escape closes it, Left steps back, and
//  Right or Enter steps on. It takes every key.
//
////////////////////////////////////////////////////////////////////////////////

bool WalkthroughPart::OnWalkthroughKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = m_walkthrough.IsOpen();



    if (isHandled && ev.kind == DxuiKeyEventKind::Down)
    {
        switch (ev.vk)
        {
            case VK_ESCAPE: m_walkthrough.Close(); break;
            case VK_LEFT:   m_walkthrough.Back();  break;
            case VK_RIGHT:
            case VK_RETURN: m_walkthrough.Next();  break;
            default:                               break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::GetWalkthroughArea
//
////////////////////////////////////////////////////////////////////////////////

RECT WalkthroughPart::GetWalkthroughArea (WalkthroughArea area) const
{
    RECT  rect   = {};
    RECT  header = m_session.views.header->GetBounds();
    RECT  row    = m_session.views.sectorRow->GetBounds();



    switch (area)
    {
        case WalkthroughArea::Platter: rect = m_session.layout.platterArea;                                       break;
        case WalkthroughArea::Toolbar: rect = m_session.layout.toolbar;                                           break;
        case WalkthroughArea::Track:   UnionRect (&rect, &header, &row);                             break;
        case WalkthroughArea::Tabs:    rect = { m_session.layout.trackArea.left, m_session.layout.trackArea.bottom, m_session.layout.trackArea.right, m_session.layout.diskContent.bottom }; break;
        case WalkthroughArea::Strip:   rect = m_session.layout.stripArea;                                         break;
        case WalkthroughArea::None:                                                                  break;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::Paint
//
////////////////////////////////////////////////////////////////////////////////

void WalkthroughPart::Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & boundsPx)
{
    if (m_walkthrough.IsOpen())
    {
        m_walkthrough.Paint (text, theme, m_session.scaler, boundsPx, GetWalkthroughArea (m_walkthrough.GetArea()));
    }
}
