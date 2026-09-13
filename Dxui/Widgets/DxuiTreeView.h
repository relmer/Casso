#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
#include "Widgets/DxuiScrollbar.h"





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
// -----------------  --------------
//      Optional            yes                yes              normal
//      Required            yes                no  (checked)    gray checkbox
//      PlatformLocked      yes                no  (checked)    gray + tooltip on
//                                                              hover with
//                                                              lockReason text
//
//  Keyboard contract on the focused tree:
//      Up   / Down  -> move highlight to prev / next visible row
//      Right        -> expand current row (if collapsible)
//      Left         -> collapse current row, or move to its parent when it
//                      is already collapsed or has no children
//      Space / Enter -> toggle current row's checkbox (if interactive)
//
//  A NAVIGATION TREE is the same control with the checkboxes hidden: rows are
//  addressed by a stable id rather than a label, children can be supplied on
//  first expand by a provider, and moving the highlight reports a selection.
//  Every one of those is off by default, so a checklist tree built before them
//  behaves exactly as it did.
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
    std::wstring               id;                              // stable identity; unique per tree
    std::wstring               label;
    std::wstring               lockReason;
    DxuiTreeCapabilityFlag     capabilityFlag = DxuiTreeCapabilityFlag::Optional;
    bool                       checked        = false;
    bool                       expanded       = true;
    bool                       childrenLoaded = true;           // false: ask the provider on first expand
    bool                       dimmed         = false;          // drawn in the muted color
    std::vector<DxuiTreeNode>  children;

    //  Drawn before the label, as Explorer draws a folder's; none draws none.
    std::shared_ptr<const DxuiIconImage>  icon;
};


class DxuiTreeView : public IDxuiControl
{
public:
    using ToggleFn        = std::function<void (const std::wstring & label, bool checked)>;
    using ChildProviderFn = std::function<std::vector<DxuiTreeNode> (const std::wstring & id)>;
    using SelectFn        = std::function<void (const std::wstring & id)>;
    using ExpandFn        = std::function<void (const std::wstring & id, bool expanded)>;

    void  SetShowCheckboxes (bool show)          { m_showCheckboxes = show; }
    void  SetChildProvider  (ChildProviderFn fn) { m_childProvider  = std::move (fn); }
    void  SetOnSelect       (SelectFn fn)        { m_onSelect       = std::move (fn); }
    void  SetOnExpand       (ExpandFn fn)        { m_onExpand       = std::move (fn); }

    bool  IsShowingCheckboxes () const { return m_showCheckboxes; }

    //  Whether a row can open: it has children, or has not been asked yet.
    static bool  CanExpand (const DxuiTreeNode & node) { return !node.children.empty() || !node.childrenLoaded; }

    //  Opens or closes the row, fetching its children first when they have
    //  not been. Returns whether anything changed.
    bool  SetRowExpanded (int flatRow, bool expanded);

    //  Moves the highlight and reports the selection.
    void  SelectRow (int flatRow);

    int                   FindRowById      (const std::wstring & id) const;
    std::wstring          GetHighlightedId () const;
    const DxuiTreeNode *  FindNodeById     (const std::wstring & id) const;
    int                   GetParentRow     (int flatRow) const;

    DxuiTreeView() { m_focusable = true; }
    ~DxuiTreeView() override = default;

    void  SetRect      (const RECT & rect) { SetBounds (rect); }
    void  SetRowHeight (int px) { m_rowHeightPx = px; }

    //  True while a scrollbar thumb drag is in progress, even with the pointer
    //  outside the tree. The host routes mouse input to the widget that started
    //  the drag until the button is released, as Win32 capture does, so
    //  dragging into the adjacent pane keeps scrolling this tree.
    bool  IsInteracting () const { return m_vertScroll.IsDragging(); }

    //  Explorer's navigation pane, measured at 120 dpi: forty pixels a row.
    static constexpr int  s_kRowHeightDip = 32;
    static constexpr int  s_kIconDip      = 16;
    static constexpr int  s_kIconGapDip   = 6;
    void  SetNodes     (std::vector<DxuiTreeNode> nodes) { m_nodes = std::move (nodes); RebuildFlatRows(); }
    void  SetEnabled   (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused   (bool focused) { m_focused = focused; }
    void  SetOnToggle  (ToggleFn fn) { m_toggle = std::move (fn); }
    void  SetDpi       (UINT dpi)
    {
        m_scaler.SetDpi (dpi);
        m_rowHeightPx = m_scaler.ToPx (s_kRowHeightDip);
        m_indentPx    = m_scaler.ToPx (18);
        m_checkboxPx  = m_scaler.ToPx (16);
        m_twistyPx    = m_scaler.ToPx (16);
    }

    const std::vector<DxuiTreeNode> & GetNodes        () const { return m_nodes; }
    int                           GetHighlight    () const { return m_highlight; }
    int                           GetRowHeight    () const { return m_rowHeightPx; }
    int                           GetVisibleCount () const { return (int) m_flatRows.size(); }
    int                           GetHoverRow     () const { return m_hoverRow; }
    bool                          IsEnabled       () const { return m_enabled; }
    bool                          IsFocused       () const { return m_focused; }

    // Returns the path-stack indices used to address the highlight
    // node within the nested tree. Empty if highlight is invalid.
    std::vector<int>  GetPath   (int flatIndex) const;
    bool              IsInteractive (int flatIndex) const;
    const DxuiTreeNode *  GetNodeAt (int flatIndex) const;
    DxuiTreeNode       *  GetNodeAtMutable (int flatIndex);

    // Hit-test maps a (x, y) to either the row's checkbox or the
    // row's twisty toggle. Returns -1 when nothing was hit.
    int   HitTestRow      (int x, int y) const;
    bool  HitTestTwisty   (int x, int y, int flatRow) const;
    bool  HitTestCheckbox (int x, int y, int flatRow) const;

    void  SetMouseHover   (int x, int y);
    bool  OnLButtonDown   (int x, int y);
    bool  OnLButtonUp     (int x, int y);
    bool  OnKey           (WPARAM vk);

    //  Scrolling. The tree scrolls by whole rows, so the first row drawn is
    //  the unit the scrollbar and the wheel both move.
    int   GetTopRow        () const  { return m_topRow; }
    void  SetTopRow        (int row);
    void  ScrollRows       (int delta)  { SetTopRow (m_topRow + delta); }
    void  EnsureRowVisible (int flatRow);
    int   GetRowCap        () const;
    int   GetMaxTopRow     () const;
    bool  IsScrollbarVisible () const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { SetFocused (focused); }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::TreeView; }

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

    static const DxuiTreeNode *  FindNodeRecursive (const std::vector<DxuiTreeNode> & nodes, const std::wstring & id);

    //  Where the label starts, which moves left when checkboxes are hidden.
    int   GetCheckboxWidthPx () const { return m_showCheckboxes ? m_checkboxPx : 0; }

    //  The rows' own width: the widget's, less the scrollbar when it shows.
    int   GetContentWidthPx   () const;
    int   GetScrollbarWidthPx () const { return m_scaler.ToPx (s_kScrollbarWidthDip); }

    //  Hands the scrollbar the range, page and position it draws from.
    void  SyncVertScroll () const;

    static constexpr int  s_kScrollbarWidthDip = 10;
    static constexpr int  s_kWheelRows         = 3;

    bool                       m_showCheckboxes = true;
    ChildProviderFn            m_childProvider;
    SelectFn                   m_onSelect;
    ExpandFn                   m_onExpand;
    int                        m_rowHeightPx    = 22;
    int                        m_indentPx       = 18;
    int                        m_checkboxPx     = 16;
    int                        m_twistyPx       = 16;
    int                        m_highlight      = -1;
    int                        m_hoverRow       = -1;
    int                        m_pressedRow     = -1;
    bool                       m_enabled        = true;
    bool                       m_focused        = false;
    std::vector<DxuiTreeNode>  m_nodes;
    std::vector<FlatRow>       m_flatRows;
    ToggleFn                   m_toggle;
    DxuiDpiScaler              m_scaler;
    int                        m_topRow         = 0;
    mutable DxuiScrollbar      m_vertScroll;
};
