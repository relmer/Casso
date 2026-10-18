#include "Pch.h"

#include "Ui/Debugger/PaneColorKeys.h"

#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Create
//
//  The button is in the pane's title bar, ahead of the menu button; a pane
//  in a document group, which has none, has it at the end of its toolbar's
//  band instead.
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Create (const std::vector<std::unique_ptr<IDebuggerPane>> & panes)
{
    for (const std::unique_ptr<IDebuggerPane> & pane : panes)
    {
        std::optional<PaneColorKeySpec>  key = pane->GetColorKey();



        if (key.has_value())
        {
            Add (pane->GetId(), *key);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Add
//
//  Space or Enter on the button opens the key or closes it, and a pinned
//  key closes when the focus moves off the button.
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Add (const std::wstring & pane, const PaneColorKeySpec & spec)
{
    Key               key;
    ColorKeyButton  * button = m_host.CreateColorKeyButton (pane, spec.legend);



    button->SetVisible    (false);
    button->SetOnActivate ([this, button] { Toggle (button); });
    button->SetOnFocus    ([this, button] (bool focused)
    {
        if (!focused && m_owner == button && m_popup.IsHeld())
        {
            Hide();
        }
    });

    key.button = button;
    key.slot   = spec.slot;
    key.legend = spec.dynamic;

    m_keys.push_back (std::move (key));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Find
//
////////////////////////////////////////////////////////////////////////////////

ColorKeyButton * PaneColorKeys::Find (const std::wstring & pane) const
{
    for (const Key & key : m_keys)
    {
        if (key.button->GetPane() == pane)
        {
            return key.button;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Place
//
//  Each button is in its pane's title bar while the pane has a color key
//  and its title bar is drawn, or else at the trailing end of its
//  toolbar's band, and moves with the pane into a floating window. A key
//  whose button is hidden closes.
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Place()
{
    const DxuiDpiScaler  & scaler = m_host.GetPaneScaler();



    for (Key & key : m_keys)
    {
        std::optional<ColorLegend::Pane>  legend  = key.legend ? key.legend() : std::optional<ColorLegend::Pane> (key.button->GetLegend());
        bool                              inTitle = legend.has_value() && TryGetTitleRect (key.button->GetPane(), true);
        bool                              inSlot  = !inTitle && legend.has_value() && key.slot != nullptr && key.slot->IsVisible();
        bool                              shown   = inTitle || inSlot;
        RECT                              slot    = {};
        RECT                              place   = {};
        DxuiWindow                      * window  = nullptr;

        if (!legend.has_value())
        {
            (void) TryGetTitleRect (key.button->GetPane(), false);
        }

        key.button->SetVisible    (shown);
        key.button->SetInTitleBar (inTitle);
        key.button->SetPressed    (shown && m_owner == key.button && m_popup.IsShown());

        if (!shown)
        {
            if (m_owner == key.button)
            {
                Hide();
            }

            continue;
        }

        key.button->SetLegend (*legend);

        if (inTitle)
        {
            (void) TryGetTitleRect (key.button->GetPane(), true, &place);
        }
        else
        {
            slot  = key.slot->GetBounds();
            place = { std::max (slot.left, slot.right - (long) scaler.ToPx (ColorKeyButton::kWidthDip)), slot.top, slot.right, slot.bottom };
        }

        window = m_host.GetPaneWindow (key.button->GetPane());

        key.button->Layout (place, scaler);
        window->SetChildClip (key.button, place);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::TryGetTitleRect
//
//  Reserve room for the pane's info button in its title bar, or none when
//  `reserve` is false, in the dock site the pane is in. Returns whether the
//  title bar is drawn for the pane, and so the button, and where.
//
////////////////////////////////////////////////////////////////////////////////

bool PaneColorKeys::TryGetTitleRect (const std::wstring & pane, bool reserve, RECT * rect)
{
    DxuiDockSite  * site  = m_host.FindDockSiteOf (pane);
    RECT            place = {};
    bool            isIn  = false;



    if (site == nullptr)
    {
        return false;
    }

    site->SetTitleExtra (pane, reserve ? DxuiTabGroup::kTitleButtonDip : 0);

    isIn = reserve && site->TryGetTitleExtraRect (pane, place);

    if (rect != nullptr)
    {
        *rect = place;
    }

    return isIn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::GetBarStrip
//
//  A pane's toolbar runs the width of its band, less the trailing end where
//  the pane's info button is when its title bar has no room for it.
//
////////////////////////////////////////////////////////////////////////////////

RECT PaneColorKeys::GetBarStrip (const std::wstring & pane, const RECT & slot)
{
    if (Find (pane) == nullptr || TryGetTitleRect (pane, true))
    {
        return slot;
    }

    return ColorKeyButton::GetStripBeside (slot, m_host.GetPaneScaler());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::RouteMouse
//
//  When the pointer moves onto a button its key opens, and when the pointer
//  moves off an unpinned key closes. A press on a button pins its key open
//  or closes it; a press anywhere else closes a pinned key and is routed on
//  to whatever is there.
//
////////////////////////////////////////////////////////////////////////////////

bool PaneColorKeys::RouteMouse (const DxuiMouseEvent & ev)
{
    ColorKeyButton  * over = nullptr;



    for (const Key & key : m_keys)
    {
        if (key.button == nullptr)
        {
            continue;
        }

        if (over == nullptr && m_host.IsBarRouted (key.button->GetPane()) && key.button->Contains (ev.positionDip))
        {
            over = key.button;
        }
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        if (m_hover != over)
        {
            if (m_hover != nullptr)
            {
                m_hover->SetHovered (false);
            }

            m_hover = over;

            if (over != nullptr)
            {
                over->SetHovered (true);
            }

            m_host.Repaint();
        }

        if (over != nullptr && (m_owner != over || !m_popup.IsShown()))
        {
            Show (over, false);
        }
        else if (over == nullptr && m_popup.IsShown() && !m_popup.IsHeld())
        {
            Hide();
        }

        return over != nullptr;
    }

    if (ev.kind == DxuiMouseEventKind::Down && over == nullptr && m_popup.IsShown())
    {
        Hide();
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && over != nullptr)
    {
        Toggle (over);
        return true;
    }

    return over != nullptr && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::TryGetPinnedRect
//
////////////////////////////////////////////////////////////////////////////////

bool PaneColorKeys::TryGetPinnedRect (RECT & rect) const
{
    if (m_owner == nullptr || !m_popup.IsShown() || !m_popup.IsHeld() || !m_host.IsBarRouted (m_owner->GetPane()))
    {
        return false;
    }

    rect = m_popup.GetRect();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Show
//
//  The key of the button's pane, under the button, in the window the pane is
//  in and in the theme in force. A tip that was up is hidden, so the two do
//  not overlap.
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Show (ColorKeyButton * button, bool pin)
{
    DxuiWindow  * window = m_host.GetPaneWindow (button->GetPane());



    if (m_host.GetTheme() == nullptr || window == nullptr)
    {
        return;
    }

    m_host.HideRoutedTip();

    m_owner = button;
    m_popup.Show (window->GetPopupHost(), button->GetBounds(), button->GetLegend(), m_host.GetColorPalette(), *m_host.GetTheme(), pin);

    button->SetPressed (m_popup.IsShown());
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Toggle
//
//  A key pinned open by this button closes; otherwise the button's key opens
//  pinned.
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Toggle (ColorKeyButton * button)
{
    if (m_owner == button && m_popup.IsShown() && m_popup.IsHeld())
    {
        Hide();
        return;
    }

    Show (button, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeys::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PaneColorKeys::Hide()
{
    if (m_owner != nullptr)
    {
        m_owner->SetPressed (false);
    }

    m_popup.Hide();
    m_owner = nullptr;
    m_host.Repaint();
}
