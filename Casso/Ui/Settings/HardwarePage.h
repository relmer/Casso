#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTreeView.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage
//
//  Settings-panel page that surfaces the hardware tree. Owns a
//  `DxuiTreeView` widget; `Rebuild` consumes the current `HardwareEntry`
//  list out of `SettingsPanelState` and re-flattens the underlying
//  `DxuiTreeNode` tree so the renderer can paint disabled checkboxes,
//  enabled-by-default rows, and platform-locked rows distinctly.
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

class HardwarePage
{
public:
    void  SetRect    (const RECT & rect, const DxuiDpiScaler & scaler);
    void  SetState   (SettingsPanelState * state);
    void  Rebuild    ();

    DxuiTreeView       & Tree ()       { return m_tree; }
    const DxuiTreeView & Tree () const { return m_tree; }

    void  OnLButtonDown (int x, int y) { (void) m_tree.OnLButtonDown (x, y); }
    void  OnLButtonUp   (int x, int y) { (void) m_tree.OnLButtonUp   (x, y); }
    void  OnMouseHover  (int x, int y) { m_tree.SetMouseHover (x, y); }
    bool  OnKey         (WPARAM vk)    { return m_tree.OnKey (vk); }

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out)
    {
        out.push_back ([this] (bool f) { m_tree.SetFocused (f); });
    }

    void  Paint         (DxuiPainter & painter, DxuiTextRenderer & text, const IDxuiTheme & theme) const;

    // Pure helper: convert one hardware-entry list into the DxuiTreeNode
    // tree the underlying DxuiTreeView consumes. Exposed for unit tests.
    static std::vector<DxuiTreeNode>  BuildNodes (const std::vector<HardwareEntry> & entries);

private:
    // Three fixed rows (Machine / CPU / Clock) + one header row
    // (Memory regions) + N dynamic rows (one per region). Cap at a
    // reasonable max so layout doesn't need vector reallocation each
    // rebuild.
    static constexpr size_t              kFixedInfoRowCount = 4;
    static constexpr size_t              kMaxMemoryRows     = 8;
    static constexpr size_t              kInfoRowCount      = kFixedInfoRowCount + kMaxMemoryRows;

    SettingsPanelState                 * m_state = nullptr;
    std::array<DxuiLabel, kInfoRowCount>     m_infoLabels;
    std::array<DxuiLabel, kInfoRowCount>     m_infoValues;
    std::array<DxuiLabel, kInfoRowCount>     m_infoExtras;
    size_t                               m_memoryRowsInUse = 0;
    int                                  m_rowHeight       = 0;
    int                                  m_sectionGap      = 0;
    RECT                                 m_baseRect        = {};
    DxuiDpiScaler                            m_scaler;
    DxuiTreeView                             m_tree;
};
