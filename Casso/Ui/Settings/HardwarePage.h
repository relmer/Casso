#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiTreeView.h"


class IDxuiTheme;
class DxuiHwndSource;




////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage
//
//  Hosts the settings sheet's "Machine" tab (GH #84 merged the old standalone
//  Machine tab in here; the class name is retained). Top to bottom:
//
//      * Machine selector (DxuiDropdown) -- the outermost control; switching
//        machines reloads the rest of the sheet.
//      * CPU speed        (DxuiDropdown: authentic / 2x / max)
//      * Hardware spec    (read-only CPU / clock / memory-region rows)
//      * Device tree      (DxuiTreeView of internal devices + slots)
//
//  The tree owns a `DxuiTreeView`; `Rebuild` consumes the current
//  `HardwareEntry` list out of `SettingsPanelState` and re-flattens the
//  underlying `DxuiTreeNode` tree so the renderer can paint disabled
//  checkboxes, enabled-by-default rows, and platform-locked rows distinctly.
//
//  Mapping rules (FR-004 .. FR-008):
//      * `HardwareEntryKind::InternalDevice` -> a row at depth 0 with
//        `Internal devices` parent (collapsible).
//      * `HardwareEntryKind::Slot`           -> a row at depth 0 with
//        `Slots` parent (collapsible). The display name already
//        includes the slot number.
//      * `CapabilityFlag::Optional`          -> `DxuiTreeCapabilityFlag::Optional`.
//      * `CapabilityFlag::Required`          -> `DxuiTreeCapabilityFlag::Required`.
//      * `CapabilityFlag::PlatformLocked`    -> `DxuiTreeCapabilityFlag::PlatformLocked`
//        + lockReason copied through so the tooltip surface can render
//        the explanatory text on hover.
//
////////////////////////////////////////////////////////////////////////////////

class HardwarePage : public DxuiPropertyPage
{
public:
    explicit HardwarePage (std::wstring title = L"Machine");

    using MachineSelectFn = std::function<void (const std::string & machineName)>;

    // DxuiPropertyPage/DxuiPanel layout hook.
    void  Layout     (const RECT & rect, const DxuiDpiScaler & scaler) override;

    void  SetRect    (const RECT & rect, const DxuiDpiScaler & scaler);
    void  SetState   (SettingsPanelState * state);
    void  SetMachineList (std::vector<std::string>  machineIds,
                          std::vector<std::wstring> displayNames,
                          int                       activeIndex);
    void  SetOnMachineSelected (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }

    // Routes the machine + speed dropdown menus through the supplied host's
    // popup pool so the menu HWND escapes the page's clipping bounds.
    void  SetPopupHost (DxuiHwndSource * host);

    void  Rebuild    ();

    DxuiDropdown       & MachineDropdown ()       { return m_machineDropdown; }
    DxuiDropdown       & SpeedDropdown   ()       { return m_speed; }
    const DxuiDropdown & SpeedDropdown   () const { return m_speed; }
    DxuiTreeView       & Tree ()       { return m_tree; }
    const DxuiTreeView & Tree () const { return m_tree; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int  ActiveMachineIndex () const { return m_activeMachineIndex; }

    // Friendly display name of the machine the dropdown currently shows
    // (e.g. "Apple ][") for the FR-131 restart notice. Empty if none.
    std::wstring SelectedMachineDisplayName () const
    {
        int  idx = m_machineDropdown.SelectedIndex();
        const std::vector<std::wstring> & items = m_machineDropdown.Items();
        return (idx >= 0 && idx < (int) items.size()) ? items[(size_t) idx] : std::wstring();
    }

    // Pure helper: convert one hardware-entry list into the DxuiTreeNode
    // tree the underlying DxuiTreeView consumes. Exposed for unit tests.
    static std::vector<DxuiTreeNode>  BuildNodes (const std::vector<HardwareEntry> & entries);

private:
    // The spec block is CPU / Clock / Memory-header (3 fixed rows) + N dynamic
    // memory-region rows (one per region). The machine identity is shown by the
    // dropdown above, so no read-only "Machine:" spec row. Cap the memory rows
    // so layout doesn't reallocate each rebuild.
    static constexpr size_t              kFixedInfoRowCount = 3;
    static constexpr size_t              kMaxMemoryRows     = 8;
    static constexpr size_t              kInfoRowCount      = kFixedInfoRowCount + kMaxMemoryRows;

    SettingsPanelState                 * m_state              = nullptr;
    std::vector<std::string>             m_machines;
    int                                  m_activeMachineIndex = -1;
    MachineSelectFn                      m_onMachineSelected;

    DxuiLabel                            m_machineLabel;
    DxuiLabel                            m_speedLabel;
    DxuiDropdown                         m_machineDropdown;
    DxuiDropdown                         m_speed;

    std::array<DxuiLabel, kInfoRowCount>     m_infoLabels;
    std::array<DxuiLabel, kInfoRowCount>     m_infoValues;
    std::array<DxuiLabel, kInfoRowCount>     m_infoExtras;
    size_t                               m_memoryRowsInUse = 0;
    int                                  m_infoTop         = 0;
    int                                  m_rowHeight       = 0;
    int                                  m_sectionGap      = 0;
    RECT                                 m_baseRect        = {};
    DxuiDpiScaler                            m_scaler;
    DxuiTreeView                             m_tree;
};
