#pragma once

#include "Pch.h"

#include "Seams/IShellIcons.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator
//
//  Block mode's address bar, search box, tree and grouped list, as Casso
//  Explorer's: the tree has the card, its partitions and each volume's
//  directories, and the list has whatever is selected in the tree, or a
//  file's blocks once the file is opened. The controls are children of the
//  panel that lays them out.
//
//  Selecting a row highlights its blocks on the map. Opening a partition, a
//  directory or a file lists what is in it; opening a structure, a finding
//  or a run of blocks, or a click on the address bar's block, opens the
//  block in the hex view, which the panel draws in the list's place.
//
////////////////////////////////////////////////////////////////////////////////

class BlockNavigator
{
public:
    using BlockFn   = std::function<void (uint32_t block)>;
    using ChangedFn = std::function<void ()>;

    void  Create (DxuiPanel & host, BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text);

    //  The tree's and the list's icons, as Casso Explorer's; with none, the
    //  rows have no icons.
    void  SetIcons (IShellIcons * icons) { m_icons = icons; }

    //  The host for the address bar's menus; with none, a click on a
    //  separator does nothing.
    void  SetPopupHost (DxuiHwndSource * host) { m_popupHost = host; }

    //  Casso Explorer's address bar menu at a point: Copy Address, Copy
    //  Address as Text and Edit Address. `focus` moves the keyboard focus to
    //  the address bar for the edit.
    void  ShowAddressMenu (int x, int y, std::function<void ()> focus);

    //  The place changed, and a block is to be opened, for the panel.
    void  SetOnPlace     (ChangedFn fn) { m_onPlace     = std::move (fn); }
    void  SetOnOpenBlock (BlockFn fn)   { m_onOpenBlock = std::move (fn); }
    void  SetOnHighlight (ChangedFn fn) { m_onHighlight = std::move (fn); }

    //  After the model's maps are read: the start for an image just opened,
    //  or the tree and the list built again from the new maps.
    void  Sync ();

    void  GoTo         (const BlockPlace & place);
    void  ShowBlock    (uint32_t block);
    void  ReplaceBlock (uint32_t block);
    bool  GoBack       ();
    bool  GoForward    ();

    //  A click on the map: the row of the list that covers the block is
    //  selected, or the block's file is selected in its directory.
    void  SelectForBlock (uint32_t block);

    const BlockPlace     & GetPlace     () const { return m_browser.GetPlace(); }
    const BlockBrowser   & GetBrowser   () const { return m_browser; }
    const BlockListing   & GetListing   () const { return m_listing; }
    bool                   IsBlockPlace () const { return GetPlace().kind == BlockPlace::Kind::Block; }

    //  The selected row's name or path, or the place's, for the status line.
    std::wstring  GetSelectionLine () const;

    DxuiAddressBar  & GetCrumbs () { return *m_crumbs; }
    DxuiTextInput   & GetSearch () { return *m_search; }

    //  Whether a key goes to text being typed: the search box has the
    //  keyboard, or the address bar is open for a path.
    bool            IsTyping (const IDxuiControl * focused) const { return focused == m_search || m_crumbs->IsEditing(); }
    DxuiTreeView  & GetTree  () { return *m_tree; }
    DxuiListView  & GetList  () { return *m_list; }

    //  The row at an index in the order of the listing's groups, or the
    //  selected row, or null.
    const BlockListRow  * GetRow         (int index) const;
    const BlockListRow  * GetSelectedRow () const { return GetRow (m_selected); }

private:
    void          Show             ();
    void          FillList         ();
    void          SortRows         ();
    void          SelectTreeNode   (const BlockPlace & container);
    BlockPlace    GetRowContainer  (const BlockListRow & row) const;
    bool          HasCardPlace     () const;
    void          OnRowSelected    (int row);
    void          OnRowActivated   (int row);
    std::wstring  GetCardLabel     () const;
    BlockPlace    GetListPlace     () const;
    void          SetTreeIcons     (vector<DxuiTreeNode> & nodes);
    void          SubmitPath       (const std::wstring & text);
    void          ShowChildMenu    (int index, const RECT & anchor);
    void          ShowOverflowMenu (const RECT & anchor);
    void          AddMenuCommand   (vector<DxuiPopupMenuItem> & items, const std::wstring & label, std::function<void ()> dispatch);

    std::shared_ptr<const DxuiIconImage>  GetRowIcon (const BlockListRow & row);

    BlockInspectorModel                   * m_model        = nullptr;
    IShellIcons                           * m_icons        = nullptr;
    DxuiHwndSource                        * m_popupHost    = nullptr;
    HWND                                    m_hwnd         = nullptr;
    vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
    BlockBrowser                            m_browser;
    BlockListing                            m_listing;
    DxuiAddressBar                        * m_crumbs       = nullptr;
    DxuiTextInput                         * m_search       = nullptr;
    DxuiTreeView                          * m_tree         = nullptr;
    DxuiListView                          * m_list         = nullptr;
    ChangedFn                               m_onPlace;
    BlockFn                                 m_onOpenBlock;
    ChangedFn                               m_onHighlight;
    vector<BlockPlace>                      m_targets;
    std::wstring                            m_query;
    uint32_t                                m_generation   = 0;
    int                                     m_selected     = -1;
    int                                     m_sortColumn   = 0;
    bool                                    m_isDescending = false;
    bool                                    m_isFilling    = false;
};
