#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateColorKeys
//
//  An info button for every pane whose colors mean something. It sits in
//  the pane's title bar, ahead of the menu button; a pane in a document
//  group, which has none, has it at the end of its toolbar's band instead.
//  Made after the panes' frames, which hold the bands.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateColorKeys()
{
    const std::array<std::pair<size_t, ColorLegend::Pane>, kUndoBarCount>  undo =
    { {
        { kRegisterUndoBar, ColorLegend::Pane::Registers },
        { kWatchUndoBar,    ColorLegend::Pane::Watch     },
        { kStackUndoBar,    ColorLegend::Pane::Stack     },
    } };



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        AddColorKey (DebuggerLayout::GetCodePaneId (view), m_codeBarSlots[(size_t) view].get(), ColorLegend::Pane::Disassembly);
    }

    for (int slot = 0; slot < (int) m_sourceDocs.size(); slot++)
    {
        AddColorKey (DebuggerLayout::GetSourcePaneId (slot), m_sourceDocs[(size_t) slot].barSlot.get(), ColorLegend::Pane::Source);
    }

    for (const auto & [index, legend] : undo)
    {
        AddColorKey (m_undoBars[index].pane, m_undoBars[index].slot.get(), legend);
    }

    AddColorKey (DebuggerLayout::kHeatMap,     m_heatMapBarSlot.get(),   ColorLegend::Pane::HeatMap);
    AddColorKey (DebuggerLayout::kBreakpoints, m_breakpointSlot.get(),   ColorLegend::Pane::Breakpoints);

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        AddColorKey (DebuggerLayout::GetMemoryPaneId (pane->GetId()), m_memoryBars[(size_t) (pane->GetId() - 1)].get(), ColorLegend::Pane::Memory);
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        DiagnosticsPane  * each = pane.get();

        AddColorKey (DebuggerLayout::GetDiagnosticsPaneId (each->GetId()), nullptr, ColorLegend::Pane::MemoryMap,
                     [each] { return each->GetColorKey(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddColorKey
//
//  Space or Enter on the button opens the key or closes it, and the key
//  held open goes when the focus leaves the button.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddColorKey (
    const std::wstring                                   & pane,
    DebuggerPaneFrame                                    * slot,
    ColorLegend::Pane                                      legend,
    std::function<std::optional<ColorLegend::Pane> ()>     dynamic)
{
    PaneColorKey      key;
    ColorKeyButton  * button = CreateChild<ColorKeyButton> (pane, legend);



    button->SetVisible  (false);
    button->SetOnActivate ([this, button] { ToggleColorKey (button); });
    button->SetOnFocus  ([this, button] (bool focused)
    {
        if (!focused && m_colorKeyOwner == button && m_colorKeyPopup.IsHeld())
        {
            HideColorKey();
        }
    });

    key.button = button;
    key.slot   = slot;
    key.legend = std::move (dynamic);

    m_colorKeys.push_back (std::move (key));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

ColorKeyButton * DebuggerWindow::GetColorKey (const std::wstring & pane) const
{
    for (const PaneColorKey & key : m_colorKeys)
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
//  DebuggerWindow::PlaceColorKeys
//
//  Each button sits in its pane's title bar while the pane has colors to
//  explain and its title bar shows it, or else at the trailing end of its
//  toolbar's band, and goes with the pane into a floating window. A key
//  whose button has gone closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceColorKeys()
{
    for (PaneColorKey & key : m_colorKeys)
    {
        std::optional<ColorLegend::Pane>  legend  = key.legend ? key.legend() : std::optional<ColorLegend::Pane> (key.button->GetLegend());
        bool                              inTitle = legend.has_value() && TryGetColorKeyTitleRect (key.button->GetPane(), true);
        bool                              inSlot  = !inTitle && legend.has_value() && key.slot != nullptr && key.slot->IsVisible();
        bool                              shown   = inTitle || inSlot;
        RECT                              slot    = {};
        RECT                              place   = {};
        DxuiWindow                      * host    = nullptr;

        if (!legend.has_value())
        {
            (void) TryGetColorKeyTitleRect (key.button->GetPane(), false);
        }

        key.button->SetVisible    (shown);
        key.button->SetInTitleBar (inTitle);
        key.button->SetPressed (shown && m_colorKeyOwner == key.button && m_colorKeyPopup.IsShown());

        if (!shown)
        {
            if (m_colorKeyOwner == key.button)
            {
                HideColorKey();
            }

            continue;
        }

        key.button->SetLegend (*legend);

        if (inTitle)
        {
            (void) TryGetColorKeyTitleRect (key.button->GetPane(), true, &place);
        }
        else
        {
            slot  = key.slot->GetBounds();
            place = { std::max (slot.left, slot.right - (long) m_scaler.ToPx (ColorKeyButton::kWidthDip)), slot.top, slot.right, slot.bottom };
        }

        host = GetPaneHost (key.button->GetPane());

        key.button->Layout (place, m_scaler);
        host->SetChildClip (key.button, place);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetColorKeyTitleRect
//
//  Keeps room for the pane's info button in its title bar, or none when
//  `keep` is false, in the dock site the pane is in, and says whether the
//  title bar shows the pane, and so the button, and where.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetColorKeyTitleRect (const std::wstring & pane, bool keep, RECT * rect)
{
    auto            found = m_floats.find (pane);
    DxuiDockSite  * site  = (found != m_floats.end() && found->second != nullptr) ? &found->second->GetSite() : m_dockSite;
    RECT            place = {};
    bool            isIn  = false;



    if (site == nullptr)
    {
        return false;
    }

    site->SetTitleExtra (pane, keep ? DxuiTabGroup::kTitleButtonDip : 0);

    isIn = keep && site->TryGetTitleExtraRect (pane, place);

    if (rect != nullptr)
    {
        *rect = place;
    }

    return isIn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBarStrip
//
//  A pane's toolbar runs the width of its band, less the trailing end where
//  the pane's info button sits when its title bar cannot hold it.
//
////////////////////////////////////////////////////////////////////////////////

RECT DebuggerWindow::GetBarStrip (const std::wstring & pane, const RECT & slot)
{
    if (GetColorKey (pane) == nullptr || TryGetColorKeyTitleRect (pane, true))
    {
        return slot;
    }

    return ColorKeyButton::GetStripBeside (slot, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteColorKeyMouse
//
//  The pointer resting on a button shows its key, and leaving it takes a key
//  that was not held away. A press on a button holds its key open or closes
//  it; a press anywhere else closes a held key and goes on to whatever is
//  there.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteColorKeyMouse (const DxuiMouseEvent & ev)
{
    ColorKeyButton  * over = nullptr;



    for (const PaneColorKey & key : m_colorKeys)
    {
        if (key.button == nullptr)
        {
            continue;
        }

        if (over == nullptr && m_routingPane == GetBarRoutingPane (key.button->GetPane()) && key.button->Contains (ev.positionDip))
        {
            over = key.button;
        }
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        if (m_colorKeyHover != over)
        {
            if (m_colorKeyHover != nullptr)
            {
                m_colorKeyHover->SetHovered (false);
            }

            m_colorKeyHover = over;

            if (over != nullptr)
            {
                over->SetHovered (true);
            }

            Invalidate();
        }

        if (over != nullptr && (m_colorKeyOwner != over || !m_colorKeyPopup.IsShown()))
        {
            ShowColorKey (over, false);
        }
        else if (over == nullptr && m_colorKeyPopup.IsShown() && !m_colorKeyPopup.IsHeld())
        {
            HideColorKey();
        }

        return over != nullptr;
    }

    if (ev.kind == DxuiMouseEventKind::Down && over == nullptr && m_colorKeyPopup.IsShown())
    {
        HideColorKey();
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && over != nullptr)
    {
        ToggleColorKey (over);
        return true;
    }

    return over != nullptr && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowColorKey
//
//  The key of the button's pane, under the button, in the window the pane is
//  in and in the theme in force. A tip that was up goes, so the two do not
//  sit one over the other.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowColorKey (ColorKeyButton * button, bool hold)
{
    DxuiWindow  * host = GetPaneHost (button->GetPane());



    if (m_theme == nullptr || host == nullptr)
    {
        return;
    }

    GetRoutedTooltip().HideImmediate();

    m_colorKeyOwner = button;
    m_colorKeyPopup.Show (host->GetPopupHost(), button->GetBounds(), button->GetLegend(), GetColorPalette(), *m_theme, hold);

    button->SetPressed (m_colorKeyPopup.IsShown());
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleColorKey
//
//  A key held open by this button closes; otherwise the button's key opens
//  and stays.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleColorKey (ColorKeyButton * button)
{
    if (m_colorKeyOwner == button && m_colorKeyPopup.IsShown() && m_colorKeyPopup.IsHeld())
    {
        HideColorKey();
        return;
    }

    ShowColorKey (button, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HideColorKey
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::HideColorKey()
{
    if (m_colorKeyOwner != nullptr)
    {
        m_colorKeyOwner->SetPressed (false);
    }

    m_colorKeyPopup.Hide();
    m_colorKeyOwner = nullptr;
    Invalidate();
}
