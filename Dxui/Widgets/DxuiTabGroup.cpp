#include "Pch.h"

#include "Widgets/DxuiTabGroup.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::AddTab
//
//  The first tab added becomes the active one; later ones start hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::AddTab (const std::wstring & title, IDxuiControl * content)
{
    Tab  tab;



    if (content == nullptr || IndexOf (content) >= 0)
    {
        return;
    }

    tab.title   = title;
    tab.content = content;
    m_tabs.push_back (tab);

    if (m_active < 0)
    {
        m_active = 0;
    }

    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::RemoveTab
//
//  Removing the active tab shows the one that took its place, or the new
//  last one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::RemoveTab (IDxuiControl * content)
{
    int  index = IndexOf (content);



    if (index < 0)
    {
        return false;
    }

    m_tabs.erase (m_tabs.begin() + index);

    if (m_tabs.empty())
    {
        m_active = -1;
    }
    else if (index < m_active || m_active >= (int) m_tabs.size())
    {
        m_active = std::max (0, m_active - 1);
    }

    LayoutContent();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetTitle (IDxuiControl * content, const std::wstring & title)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].title = title;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetIndicator
//
//  The active tab's content is on screen, so it never carries one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetIndicator (IDxuiControl * content, bool on)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].indicator = on && index != m_active;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetLeadingDot
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetLeadingDot (IDxuiControl * content, uint32_t argb)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].leadDot = argb;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiTabGroup::GetContent (int index) const
{
    return (index >= 0 && index < (int) m_tabs.size()) ? m_tabs[(size_t) index].content : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::HasIndicator
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::HasIndicator (int index) const
{
    return index >= 0 && index < (int) m_tabs.size() && m_tabs[(size_t) index].indicator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::IndexOf
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::IndexOf (IDxuiControl * content) const
{
    for (size_t i = 0; i < m_tabs.size(); i++)
    {
        if (m_tabs[i].content == content)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetActive
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetActive (int index)
{
    if (index < 0 || index >= (int) m_tabs.size() || index == m_active)
    {
        return;
    }

    m_active                              = index;
    m_tabs[(size_t) index].indicator      = false;

    LayoutContent();

    if (m_onActivated)
    {
        m_onActivated (index);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTabWidthDip
//
//  From the title's length at an average character width, so tabs can be
//  hit-tested before the first paint has measured anything.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::GetTabWidthDip (int index) const
{
    const Tab &  tab = m_tabs[(size_t) index];



    return m_scaler.ToPx (2 * kTabPadDip + (int) tab.title.size() * kCharDip + (tab.indicator ? kIndicatorDip : 0) + (tab.leadDot != 0 ? kIndicatorDip : 0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTabRect (int index) const
{
    long  x = m_boundsDip.left;



    if (index < 0 || index >= (int) m_tabs.size())
    {
        return RECT {};
    }

    for (int i = 0; i < index; i++)
    {
        x += GetTabWidthDip (i);
    }

    return RECT { x, m_boundsDip.top, x + GetTabWidthDip (index), m_boundsDip.top + m_scaler.ToPx (kStripDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetNewTabRect
//
//  Past the last tab, or empty when the group shows no +.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetNewTabRect() const
{
    long  x = m_boundsDip.left;



    if (!m_newTab || !m_newTabShown || !m_newTabShown (*this))
    {
        return RECT {};
    }

    if (!m_tabs.empty())
    {
        x = GetTabRect ((int) m_tabs.size() - 1).right;
    }

    return RECT { x, m_boundsDip.top, std::min ((long) m_boundsDip.right, x + (long) m_scaler.ToPx (kNewTabDip)), m_boundsDip.top + m_scaler.ToPx (kStripDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetBodyRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetBodyRect() const
{
    return RECT { m_boundsDip.left, std::min (m_boundsDip.bottom, m_boundsDip.top + (long) m_scaler.ToPx (kStripDip)),
                  m_boundsDip.right, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::HitTestTab
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::HitTestTab (POINT pointDip) const
{
    for (int i = 0; i < (int) m_tabs.size(); i++)
    {
        RECT  tab = GetTabRect (i);

        if (pointDip.x >= tab.left && pointDip.x < tab.right && pointDip.y >= tab.top && pointDip.y < tab.bottom &&
            pointDip.x < m_boundsDip.right)
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler = scaler;
    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::LayoutContent
//
//  The active control fills the body; the others are hidden where they are.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::LayoutContent()
{
    for (int i = 0; i < (int) m_tabs.size(); i++)
    {
        IDxuiControl  * content = m_tabs[(size_t) i].content;
        bool            shown   = (i == m_active) && IsVisible();

        content->SetVisible (shown);

        if (shown)
        {
            content->Layout (GetBodyRect(), m_scaler);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::Paint
//
//  The strip only: each tab's title, the active one on the content's own
//  background so it reads as part of the pane below it, the rest on the
//  panel's, with a hairline under them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           strip = m_scaler.ToPxf ((float) kStripDip);
    float           pad   = m_scaler.ToPxf ((float) kTabPadDip);
    float           line  = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));
    float           left  = (float) m_boundsDip.left;
    float           top   = (float) m_boundsDip.top;
    float           width = (float) (m_boundsDip.right - m_boundsDip.left);
    HRESULT         hr    = S_OK;



    painter.FillRect (left, top, width, strip, theme.Background());

    for (int i = 0; i < (int) m_tabs.size(); i++)
    {
        RECT      tab    = GetTabRect (i);
        bool      active = (i == m_active);
        uint32_t  color  = active ? theme.Foreground() : theme.ForegroundMuted();

        if (tab.left >= m_boundsDip.right)
        {
            break;
        }

        tab.right = std::min (tab.right, m_boundsDip.right);

        if (active)
        {
            painter.FillRect ((float) tab.left, (float) tab.top, (float) (tab.right - tab.left), strip, theme.ContentBackground());
            painter.FillRect ((float) tab.left, (float) tab.top, (float) (tab.right - tab.left), line * 2, theme.Accent());
        }

        float  lead = 0.0f;

        if (m_tabs[(size_t) i].leadDot != 0)
        {
            lead = m_scaler.ToPxf ((float) kIndicatorDip);
            painter.FillCircleApprox ((float) tab.left + pad + lead * 0.35f, (float) tab.top + strip / 2, m_scaler.ToPxf (3.5f), m_tabs[(size_t) i].leadDot);
        }

        hr = text.DrawString (m_tabs[(size_t) i].title.c_str(),
                              (float) tab.left + pad + lead, (float) tab.top,
                              (float) (tab.right - tab.left) - pad - lead, strip,
                              color, m_scaler.ToPxf (font.sizeDip), font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (m_tabs[(size_t) i].indicator)
        {
            painter.FillCircleApprox ((float) tab.right - pad - m_scaler.ToPxf ((float) kIndicatorDip / 2 - 2),
                                      (float) tab.top + strip / 2,
                                      m_scaler.ToPxf (3.0f), theme.Accent());
        }
    }

    if (RECT plus = GetNewTabRect(); plus.right > plus.left)
    {
        if (m_hoverNewTab)
        {
            painter.FillRect ((float) plus.left, (float) plus.top, (float) (plus.right - plus.left), strip, theme.HoverBackground());
        }

        hr = text.DrawString (L"+", (float) plus.left, (float) plus.top, (float) (plus.right - plus.left), strip,
                              m_hoverNewTab ? theme.Foreground() : theme.ForegroundMuted(),
                              m_scaler.ToPxf (font.sizeDip * 1.3f), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    painter.FillRect (left, top + strip - line, width, line, theme.Divider());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::OnMouse
//
//  A press on a tab activates it and arms a drag; moving past the drag
//  distance with the button still down reports the drag once.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::OnMouse (const DxuiMouseEvent & ev)
{
    int  hit = HitTestTab (ev.positionDip);



    RECT  plus    = GetNewTabRect();
    bool  onPlus  = ev.positionDip.x >= plus.left && ev.positionDip.x < plus.right && ev.positionDip.y >= plus.top && ev.positionDip.y < plus.bottom;



    m_hoverNewTab = onPlus && ev.kind != DxuiMouseEventKind::Leave;

    if (onPlus && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_newTab (*this);
        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:
        if (hit < 0 || ev.button != DxuiMouseButton::Left)
        {
            return false;
        }

        SetActive (hit);
        m_pressedTab = hit;
        m_pressedAt  = ev.positionDip;
        m_dragging   = false;
        return true;

    case DxuiMouseEventKind::Move:
        if (m_pressedTab < 0 || m_dragging)
        {
            return m_pressedTab >= 0;
        }

        if (std::abs (ev.positionDip.x - m_pressedAt.x) > m_scaler.ToPx (kDragDip) || std::abs (ev.positionDip.y - m_pressedAt.y) > m_scaler.ToPx (kDragDip))
        {
            m_dragging = true;

            if (m_onDragStart)
            {
                m_onDragStart (m_pressedTab, ev.positionDip);
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (m_pressedTab < 0)
        {
            return false;
        }

        m_pressedTab = -1;
        m_dragging   = false;
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::OnKey (const DxuiKeyEvent & ev)
{
    int  count = (int) m_tabs.size();



    if (ev.kind != DxuiKeyEventKind::Down || ev.vk != VK_TAB || !ev.ctrl || count < 2)
    {
        return false;
    }

    SetActive ((m_active + (ev.shift ? count - 1 : 1)) % count);
    return true;
}
