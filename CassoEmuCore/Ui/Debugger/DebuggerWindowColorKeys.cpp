#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateColorKeys
//
//  An info button for every pane whose colors mean something, in the band
//  its toolbar sits in; the call stack's and a device panel's are bands of
//  their own. Made after the panes' frames, which hold the bands.
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

    AddColorKey (DebuggerLayout::kCallStack,   m_callStackKeySlot.get(), ColorLegend::Pane::CallStack);
    AddColorKey (DebuggerLayout::kHeatMap,     m_heatMapBarSlot.get(),   ColorLegend::Pane::HeatMap);
    AddColorKey (DebuggerLayout::kBreakpoints, m_breakpointSlot.get(),   ColorLegend::Pane::Breakpoints);

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        AddColorKey (DebuggerLayout::GetMemoryPaneId (pane->GetId()), m_memoryBars[(size_t) (pane->GetId() - 1)].get(), ColorLegend::Pane::Memory);
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        DiagnosticsPane  * each = pane.get();

        AddColorKey (DebuggerLayout::GetDiagnosticsPaneId (each->GetId()), each->GetKeySlot(), ColorLegend::Pane::MemoryMap,
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
//  Each button takes the trailing end of its band, where its pane shows it
//  and has colors to explain, and goes with the pane into a floating window.
//  A key whose button has gone closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceColorKeys()
{
    for (PaneColorKey & key : m_colorKeys)
    {
        std::optional<ColorLegend::Pane>  legend = key.legend ? key.legend() : std::optional<ColorLegend::Pane> (key.button->GetLegend());
        bool                              shown  = key.slot != nullptr && key.slot->IsVisible() && legend.has_value();
        RECT                              slot   = {};
        RECT                              place  = {};
        DxuiWindow                      * host   = nullptr;

        key.button->SetVisible (shown);
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

        slot  = key.slot->GetBounds();
        place = { std::max (slot.left, slot.right - (long) m_scaler.ToPx (ColorKeyButton::kWidthDip)), slot.top, slot.right, slot.bottom };
        host  = GetPaneHost (key.button->GetPane());

        key.button->Layout (place, m_scaler);
        host->SetChildClip (key.button, place);
    }
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
