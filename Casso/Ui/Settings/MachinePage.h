#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"


class IDxuiTheme;
class DxuiHwndSource;




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage
//
//  Top page of the settings sheet. Hosts the machine identity + CPU
//  knobs; the Disk ][ drive controls moved to DiskPage (GH #84):
//
//      * Machine selector (DxuiDropdown) -- the outermost control;
//        switching machines reloads the rest of the sheet.
//      * Speed mode    (DxuiDropdown: authentic / 2x / max)
//
//  All change events route into the supplied `SettingsPanelState`
//  through the small setter API; widgets read their initial state
//  on `Rebuild`.
//
////////////////////////////////////////////////////////////////////////////////

class MachinePage : public DxuiPropertyPage
{
public:
    explicit MachinePage (std::wstring title = L"Machine");

    using MachineSelectFn = std::function<void (const std::string & machineName)>;

    void  SetState              (SettingsPanelState * state);
    void  SetMachineList        (std::vector<std::string>  machineIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnMachineSelected  (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }

    // Routes every owned dropdown's popup menu through the supplied
    // DxuiHwndSource's popup-host pool so the menu HWND escapes the
    // page's clipping bounds. Pass nullptr to revert to the legacy
    // in-panel PaintMenu path.
    void  SetPopupHost          (DxuiHwndSource * host);

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild               ();

    // Test accessors. Also surface mutable widget refs so the sheet
    // can inline focus-setter lambdas and dropdown-open queries.
    DxuiDropdown          & MachineDropdown      () { return m_machineDropdown; }
    DxuiDropdown          & SpeedDropdown        () { return m_speed; }

    const DxuiDropdown    & SpeedDropdown        () const { return m_speed; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int               ActiveMachineIndex     () const { return m_activeMachineIndex; }
    const DxuiDropdown  & MachineDropdown    () const { return m_machineDropdown; }

    // Friendly display name of the machine the dropdown currently shows
    // (e.g. "Apple ][") for the FR-131 restart notice. Empty if none.
    std::wstring SelectedMachineDisplayName () const
    {
        int  idx = m_machineDropdown.SelectedIndex();
        const std::vector<std::wstring> & items = m_machineDropdown.Items();
        return (idx >= 0 && idx < (int) items.size()) ? items[(size_t) idx] : std::wstring();
    }

private:
    SettingsPanelState         * m_state              = nullptr;
    std::vector<std::string>     m_machines;
    int                          m_activeMachineIndex = -1;
    MachineSelectFn              m_onMachineSelected;

    DxuiLabel                        m_machineLabel;
    DxuiLabel                        m_speedLabel;

    DxuiDropdown                     m_machineDropdown;
    DxuiDropdown                     m_speed;
};
