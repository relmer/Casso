#include "Pch.h"

#include "Ui/DiskInspector/Components/WalkthroughPart.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::WalkthroughPart
//
////////////////////////////////////////////////////////////////////////////////

WalkthroughPart::WalkthroughPart (const InspectorSession & session) :
    m_session (session)
{
    m_next.SetVariant (DxuiButton::Variant::Primary);

    m_close.SetOnClick ([this] () { m_walkthrough.Close(); });
    m_back.SetOnClick  ([this] () { m_walkthrough.Back();  });
    m_next.SetOnClick  ([this] () { m_walkthrough.Next();  });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart::OnWalkthroughMouse
//
//  While the walkthrough is open, mouse events go to its buttons and to no
//  other control.
//
////////////////////////////////////////////////////////////////////////////////

bool WalkthroughPart::OnWalkthroughMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = m_walkthrough.IsOpen();



    if (isHandled)
    {
        for (DxuiButton * button : { &m_close, &m_back, &m_next })
        {
            button->OnMouse (ev);
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
//  The card, then the layer flushed so the buttons cover it.
//
////////////////////////////////////////////////////////////////////////////////

void WalkthroughPart::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & boundsPx)
{
    if (m_walkthrough.IsOpen())
    {
        m_walkthrough.SetHasDrives (m_session.host->GetDriveCount() > 0);
        m_walkthrough.Paint (text, theme, m_session.scaler, boundsPx, GetWalkthroughArea (m_walkthrough.GetArea()));

        m_next.SetLabel   (m_walkthrough.IsLastStep() ? L"Done" : L"Next");
        m_back.SetEnabled (m_walkthrough.GetStep() > 0);

        m_close.Layout (m_walkthrough.GetButtonRect (WalkthroughButton::Close), m_session.scaler);
        m_back.Layout  (m_walkthrough.GetButtonRect (WalkthroughButton::Back),  m_session.scaler);
        m_next.Layout  (m_walkthrough.GetButtonRect (WalkthroughButton::Next),  m_session.scaler);

        painter.FlushLayer();

        for (DxuiButton * button : { &m_close, &m_back, &m_next })
        {
            button->Paint (painter, text, theme);
        }
    }
}
