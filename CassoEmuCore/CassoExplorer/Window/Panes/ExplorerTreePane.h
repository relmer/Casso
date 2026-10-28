#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"

class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane
//
//  The navigation pane: the tree of Casso's places, This PC, the shell's
//  roots and the CassoLink connections; the location revealed in it; its
//  menus and the pane options they set; a node's tip, and a cut-off name
//  drawn whole over its row; and dragging a node. The window routes pointer
//  and key events over the tree here.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTreePane
{
public:
    explicit ExplorerTreePane (CassoExplorerWindow & window);

    //  How far each level sits in from its parent: File Explorer's, measured
    //  at 125% as 10 pixels.
    static constexpr int     kTreeIndentDip   = 8;

    //  A row's icon starts 18 dip past the middle of its twisty and its label
    //  3.33 dip past the icon: measured at 150%.
    static constexpr float   kTreeIconLeadDip = 10.0f;
    static constexpr float   kTreeIconGapDip  = 3.33f;
    static constexpr float   kTreeLeftPadDip  = 8.0f;    // the top level from the pane's left edge

    //  The tips of Casso's folders node and of a folder on a removable drive
    //  that is not connected.
    static constexpr const wchar_t *  kCassoNodeTip   = L"Folders Casso has opened disk images from, most recently used first";
    static constexpr const wchar_t *  kMissingNodeTip = L"This folder is on a removable drive which is not currently connected.";

    //  A rename box runs to this far in from the tree's right edge.
    static constexpr int     kRenameEdgeDip   = 8;

    //  The tree, as a child of the window, and its settings, handlers and
    //  roots once the window has its HWND.
    void  Create    ();
    void  Configure ();
    void  Layout    (const RECT & rect);

    //  Pointer events. The label tip follows every move; a drag armed on a
    //  node or one under way, and the hover's end, are ahead of the list's
    //  hit test; a press over the tree is after it.
    void  UpdateLabelTip  (const DxuiMouseEvent & ev);
    void  HideLabelTip    () { m_labelTip.Hide(); }
    bool  HandleCapturedMouse (const DxuiMouseEvent & ev);
    bool  HandleMouse         (const DxuiMouseEvent & ev);
    void  UpdateTip       (const DxuiMouseEvent & ev, POINT point);

    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu (POINT point) const;
    bool                            UpdateScrollbarHover (POINT at) { return m_tree->SetScrollbarHover (at); }
    bool                            TickScrollbars       (int64_t nowMs) { return m_tree->TickScrollbars (nowMs); }

    //  Keys, and focus.
    bool            TryShortcutKey (const DxuiKeyEvent & ev, bool & outHandled);
    bool            HandleKey          (const DxuiKeyEvent & ev) { return m_tree->OnKey (ev); }
    void            HandleFocusChanged (bool focused) { m_tree->OnFocusChanged (focused); }
    IDxuiControl *  GetControl     () const { return m_tree; }
    RECT            GetBounds      () const { return m_tree->GetBounds(); }
    bool            IsInteracting  () const { return m_tree->IsInteracting(); }

    //  The location's node revealed: opened to, highlighted and scrolled into
    //  view. RefreshTree re-reads the tree, keeping what was open,
    //  highlighted and on screen; ReloadChildren re-reads one node's.
    void  RevealLocationInTree ();
    void  RefreshTree          ();
    void  ReloadChildren       (const std::wstring & id);

    //  The pane options, from the preferences.
    void  ApplyNavPaneOptions ();
    bool  IsNavOptionOn       (CassoExplorerPrefs::NavOption option) const;

    //  Rows, for a drop and for the folders on display.
    int                   HitTestRow       (POINT client) const { return m_tree->HitTestRow (client.x, client.y); }
    const DxuiTreeNode *  GetNodeAt        (int row) const { return m_tree->GetNodeAt (row); }
    int                   GetVisibleCount  () const { return m_tree->GetVisibleCount(); }
    bool                  HasNode          (const std::wstring & id) const { return m_tree->FindRowById (id) >= 0; }
    void                  ExpandRowForDrop (int row);
    void                  SetDropRow       (int row) { m_tree->SetDropRow (row); }
    void                  ClearDropTarget  ();
    void                  ReleasePress     ();
    bool                  TryGetRenameRect (const std::wstring & id, RECT & outRect);

private:
    //  A press on a drive's, a folder's or an image's node, which drags it
    //  once the pointer moves far enough.
    struct TreeDrag
    {
        bool      armed    = false;
        POINT     start    = {};
        Location  location;
    };

    void  ScrollTreeToRow     (int row);
    int   WalkTreeLabels      (int row, const std::wstring & path);
    void  ShowLabelTipAt      (POINT point);
    void  ShowTreeContextMenu (int x, int y, const std::wstring & id);
    void  AddTreeIconRow      (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder, const std::wstring & id);

    //  The menu below the tree's last node, and the pane options it sets.
    void  ShowTreeEmptyMenu   (int x, int y);
    void  AddNavPaneToggle    (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, CassoExplorerPrefs::NavOption option);

    CassoExplorerWindow  & m_window;

    DxuiTreeView         * m_tree          = nullptr;
    DxuiInPlaceTip         m_labelTip;               // a name the splitter cuts off, shown whole over its row
    bool                   m_revealPending = false;  // a node to scroll into view once the tree has a height
    TreeDrag               m_drag;
    std::wstring           m_treeTipId;              // the broken image whose reason is in m_treeTipText
    std::wstring           m_treeTipText;
};