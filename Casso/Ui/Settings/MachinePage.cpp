#include "Pch.h"

#include "MachinePage.h"

#include "../../UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 140;
    constexpr int    s_kCheckWidthDp    = 140;
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
//  Registers each member widget into the panel's child list via
//  Adopt so they participate in the IDxuiControl tree (Bounds,
//  Visible, focus, parent pointers). The widgets remain MachinePage-
//  owned members; Adopt is non-owning. Layout positioning still
//  happens in Layout() below via the legacy SetRect calls because
//  the existing layout code does things DxuiFormLayout cannot model
//  (per-row indentation for the mechanism sub-row, two checkboxes
//  on a single row for the write-protect pair). SettingsPanel still
//  drives input/paint through the bespoke shims below; collapsing
//  the duality is deferred to the SettingsPanel atomic conversion.
//
////////////////////////////////////////////////////////////////////////////////

MachinePage::MachinePage()
{
    size_t  i = 0;


    Adopt (m_machineLabel);
    Adopt (m_speedLabel);
    Adopt (m_wpLabel);
    Adopt (m_writeModeLabel);
    Adopt (m_audioLabel);
    Adopt (m_mechLabel);

    Adopt (m_machineDropdown);
    Adopt (m_speed);
    Adopt (m_writeMode);
    Adopt (m_mechanism);
    Adopt (m_driveAudio);
    for (i = 0; i < m_writeProtect.size(); ++i)
    {
        Adopt (m_writeProtect[i]);
    }
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
    int  checkWidth   = scaler.Px (s_kCheckWidthDp);
    int  dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap   = scaler.Px (s_kSectionGapDp);
    int  childIndent  = scaler.Px (18);          // matches DxuiTreeView indent
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

    m_wpLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_wpLabel.SetText (L"Write protect:");
    m_writeProtect[0].SetRect (MakeRect (controlsX,                y, checkWidth, rowHeight));
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetRect (MakeRect (controlsX + checkWidth,   y, checkWidth, rowHeight));
    m_writeProtect[1].SetLabel (L"Drive 2");
    y += rowHeight + sectionGap;

    m_writeModeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_writeModeLabel.SetText (L"Write mode:");
    m_writeMode.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_writeMode.SetItems ({ L"Buffer and flush", L"Copy on write" });
    y += rowHeight + sectionGap;

    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_audioLabel.SetText (L"Drive audio:");
    m_driveAudio.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Mechanism is a child of Drive audio: indent the label by the
    // same childIndent used elsewhere (matches DxuiTreeView's 18 dp).
    m_mechLabel.SetRect  (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_mechLabel.SetText  (L"Mechanism:");
    m_mechanism.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_mechanism.SetItems ({ L"Shugart", L"Alps" });

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_wpLabel.SetDpi         (dpi);
    m_writeModeLabel.SetDpi  (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);
    m_writeMode.SetDpi       (dpi);
    m_mechanism.SetDpi       (dpi);
    m_driveAudio.SetDpi      (dpi);
    m_writeProtect[0].SetDpi (dpi);
    m_writeProtect[1].SetDpi (dpi);

    // Mirror the page's footprint into the IDxuiControl tree so future
    // centralized walks see this page as a panel covering `rect`.
    // Adopted children already have their bounds written via the
    // SetRect calls above (SetRect mirrors through SetBounds).
    DxuiPanel::SetBounds (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Rebuild
//
//  Re-sync widget visible state to the underlying SettingsPanelState
//  and wire each widget's OnChange callback back into the state.
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
    m_writeMode.SetSelected ((int) state->Prefs().writeMode);
    m_mechanism.SetSelected (state->Prefs().floppyMechanism == "alps" ? 1 : 0);
    m_driveAudio.SetChecked (state->Prefs().floppySoundEnabled);
    m_writeProtect[0].SetChecked (state->Prefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->Prefs().writeProtect[1]);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
    ApplyMechanismEnabled (state->Prefs().floppySoundEnabled);

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
    m_writeMode.SetSelect    ([state] (int idx) { state->SetWriteMode ((SettingsWriteMode) idx); });
    m_mechanism.SetSelect    ([state] (int idx) { state->SetMechanism (idx == 1 ? "alps" : "shugart"); });
    m_driveAudio.SetOnChange ([this, state] (bool checked)
    {
        state->SetFloppySound (checked);
        ApplyMechanismEnabled (checked);
    });
    m_writeProtect[0].SetOnChange ([state] (bool checked) { state->SetWriteProtect (0, checked); });
    m_writeProtect[1].SetOnChange ([state] (bool checked) { state->SetWriteProtect (1, checked); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetPopupHost
//
//  Routes each owned dropdown's menu through the supplied host's
//  popup pool so the menu HWND escapes the page's clipping bounds.
//  Pass nullptr to revert to the in-panel PaintMenu path.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetPopupHost (DxuiHostWindow * host)
{
    m_machineDropdown.SetPopupHost (host);
    m_speed.SetPopupHost           (host);
    m_writeMode.SetPopupHost       (host);
    m_mechanism.SetPopupHost       (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ApplyMechanismEnabled
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ApplyMechanismEnabled (bool enabled)
{
    constexpr uint32_t  s_kLabelEnabledArgb  = 0xFFE8EEF4;
    constexpr uint32_t  s_kLabelDisabledArgb = 0xFF6A7585;

    m_mechanism.SetEnabled (enabled);
    m_mechLabel.SetColorArgb (enabled ? s_kLabelEnabledArgb : s_kLabelDisabledArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bespoke input + paint shims (OnLButtonDown / OnLButtonUp /
//  OnMouseHover / OnKey / Paint(painter,text,theme) / CollectFocusables /
//  AnyDropdownOpen) used to live here. SettingsPanel now dispatches
//  via IDxuiControl::OnMouse / OnKey through DxuiPanel auto fan-out
//  (input) and queries individual widgets directly (focus, dropdowns).
//  Paint runs through the inherited DxuiPanel::Paint walk over the
//  Adopt'd child list. Out-of-panel popup hosting (Sub-step B)
//  collapses the legacy PaintBase-then-PaintMenu z-slot, so the auto
//  walk preserves the correct stacking.
//
////////////////////////////////////////////////////////////////////////////////
