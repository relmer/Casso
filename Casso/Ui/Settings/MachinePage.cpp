#include "Pch.h"

#include "MachinePage.h"

#include "Core/UnicodeSymbols.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 140;
    constexpr int    s_kDropdownWidthDp = 200;
    constexpr int    s_kSectionGapDp    = 14;
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::MachinePage
//
//  Registers each member widget into the page's child list via Adopt so
//  they participate in the IDxuiControl tree (Bounds, Visible, focus, parent
//  pointers). The widgets remain MachinePage-owned members; Adopt is
//  non-owning. Layout positioning happens in Layout() below.
//
////////////////////////////////////////////////////////////////////////////////

MachinePage::MachinePage(std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_machineLabel);
    Adopt (m_speedLabel);

    Adopt (m_machineDropdown);
    Adopt (m_speed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetMachineList
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetMachineList (std::vector<std::string>  machineIds,
                                  std::vector<std::wstring> displayNames,
                                  int                       activeIndex)
{
    m_machines           = std::move (machineIds);
    m_activeMachineIndex = activeIndex;

    if (displayNames.size() != m_machines.size())
    {
        // Caller mismatch — fall back to ids as labels.
        displayNames.clear();
        for (const std::string & id : m_machines)
        {
            displayNames.emplace_back (id.begin(), id.end());
        }
    }

    m_machineDropdown.SetItems    (displayNames);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi          = scaler.Dpi();
    int  pad          = scaler.Px (s_kPagePadDp);
    int  rowHeight    = scaler.Px (s_kRowHeightDp);
    int  labelWidth   = scaler.Px (s_kLabelWidthDp);
    int  dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap   = scaler.Px (s_kSectionGapDp);
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;



    m_machineLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_machineLabel.SetText (L"Machine:");
    m_machineDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_speedLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_speedLabel.SetText (L"CPU speed:");
    m_speed.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_speed.SetItems ({ L"Authentic (1.023 MHz)",
                        L"2x (2.046 MHz)",
                        std::wstring (L"Maximum (full afterburner ") + s_kpszRocket + L")" });
    y += rowHeight + sectionGap;

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);

    // Mirror the page's footprint into the IDxuiControl tree so future
    // centralized walks see this page as a panel covering `rect`.
    DxuiPanel::SetBounds (rect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Rebuild
//
//  Re-sync widget state to the underlying SettingsPanelState and wire each
//  widget's OnChange callback back into the state.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Rebuild ()
{
    SettingsPanelState  * state = m_state;



    if (state == nullptr)
    {
        return;
    }

    m_speed.SetSelected     ((int) state->Prefs().speedMode);
    m_machineDropdown.SetSelected (m_activeMachineIndex);

    m_machineDropdown.SetSelect ([this] (int idx)
    {
        if (idx >= 0 && idx < (int) m_machines.size())
        {
            m_activeMachineIndex = idx;
            if (m_onMachineSelected)
            {
                m_onMachineSelected (m_machines[(size_t) idx]);
            }
        }
    });
    m_speed.SetSelect        ([state] (int idx) { state->SetSpeedMode ((SettingsSpeedMode) idx); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetPopupHost
//
//  Routes each owned dropdown's menu through the supplied host's popup pool
//  so the menu HWND escapes the page's clipping bounds. Pass nullptr to
//  revert to the in-panel PaintMenu path.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetPopupHost (DxuiHwndSource * host)
{
    m_machineDropdown.SetPopupHost (host);
    m_speed.SetPopupHost           (host);
}
