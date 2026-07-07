#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView
//
//  Two-level expandable list of nodes. Each `Node` carries a label,
//  an optional `capabilityFlag` (drives the checkbox interaction
//  model), a checked state, an explicit enabled flag, and an
//  optional `lockReason` string that the surrounding tooltip surface
//  renders on hover for platform-locked rows.
//
//  CapabilityFlag rendering rules (FR-004 .. FR-008):
//
//      capabilityFlag      checkbox visible?  interactive?     visual
//      --------------      -----------------  --------------   ------------------
//      Optional            yes                yes              normal
//      Required            yes                no  (checked)    grey checkbox
//      PlatformLocked      yes                no  (checked)    grey + tooltip on
//                                                              hover with
//                                                              lockReason text
//
//  Keyboard contract on the focused tree:
//      Up   / Down  -> move highlight to prev / next visible row
//      Right        -> expand current row (if collapsible)
//      Left         -> collapse current row
//      Space / Enter -> toggle current row's checkbox (if interactive)
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiTreeCapabilityFlag
{
    Optional,        // user-toggleable
    Required,        // always on, checkbox shown disabled-checked
    PlatformLocked,  // always on + locked, hovering surfaces lockReason
};


struct DxuiTreeNode
{
    std::wstring                  label;
    std::wstring                  lockReason;
    DxuiTreeCapabilityFlag            capabilityFlag = DxuiTreeCapabilityFlag::Optional;
    bool                          checked        = false;
    bool                          expanded       = true;
    std::vector<DxuiTreeNode>         children;
};


class DxuiTreeView : public IDxuiControl
{
public:
    using ToggleFn = std::function<void (const std::wstring & label, bool checked)>;

    DxuiTreeView() { m_focusable = true; }
    ~DxuiTreeView() override = default;

    void  SetRect      (const RECT & rect) { SetBounds (rect); }
    void  SetRowHeight (int px) { m_rowHeightPx = px; }
    void  SetNodes     (std::vector<DxuiTreeNode> nodes) { m_nodes = std::move (nodes); RebuildFlatRows(); }
    void  SetEnabled   (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused   (bool focused) { m_focused = focused; }
    void  SetOnToggle  (ToggleFn fn) { m_toggle = std::move (fn); }
    void  SetDpi       (UINT dpi)
    {
        m_scaler.SetDpi (dpi);
        m_rowHeightPx = m_scaler.Px (22);
        m_indentPx    = m_scaler.Px (18);
        m_checkboxPx  = m_scaler.Px (16);
        m_twistyPx    = m_scaler.Px (16);
    }

    const std::vector<DxuiTreeNode> & Nodes        () const { return m_nodes; }
    int                           Highlight    () const { return m_highlight; }
    int                           RowHeight    () const { return m_rowHeightPx; }
    int                           VisibleCount () const { return (int) m_flatRows.size(); }
    int                           HoverRow     () const { return m_hoverRow; }
    bool                          Enabled      () const { return m_enabled; }
    bool                          Focused      () const { return m_focused; }

    // Returns the path-stack indices used to address the highlight
    // node within the nested tree. Empty if highlight is invalid.
    std::vector<int>  PathFor   (int flatIndex) const;
    bool              IsInteractive (int flatIndex) const;
    const DxuiTreeNode *  NodeAt    (int flatIndex) const;
    DxuiTreeNode       *  NodeAtMutable (int flatIndex);

    // Hit-test maps a (x, y) to either the row's checkbox or the
    // row's twisty toggle. Returns -1 when nothing was hit.
    int   HitTestRow      (int x, int y) const;
    bool  HitTestTwisty   (int x, int y, int flatRow) const;
    bool  HitTestCheckbox (int x, int y, int flatRow) const;

    void  SetMouseHover   (int x, int y);
    bool  OnLButtonDown   (int x, int y);
    bool  OnLButtonUp     (int x, int y);
    bool  OnKey           (WPARAM vk);

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::TreeView; }

    // Force the flat-rows cache to repopulate. Public so tests can
    // mutate node state via NodeAtMutable then re-flatten.
    void  RebuildFlatRows ();

private:
    struct FlatRow
    {
        std::vector<int>  pathStack;   // indices into m_nodes children
        int               depth = 0;
    };


    void  FlattenRecursive (const DxuiTreeNode & node, std::vector<int> & path, int depth);
    void  ToggleRow        (int flatRow);
    int                   m_rowHeightPx = 22;
    int                   m_indentPx    = 18;
    int                   m_checkboxPx  = 16;
    int                   m_twistyPx    = 16;
    int                   m_highlight   = -1;
    int                   m_hoverRow    = -1;
    int                   m_pressedRow  = -1;
    bool                  m_enabled     = true;
    bool                  m_focused     = false;
    std::vector<DxuiTreeNode> m_nodes;
    std::vector<FlatRow>  m_flatRows;
    ToggleFn              m_toggle;
    DxuiDpiScaler             m_scaler;
};
