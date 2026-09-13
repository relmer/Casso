#pragma once

#include "Pch.h"

#include "Cassque/Model/BrowserModel.h"
#include "Cassque/Model/CatalogModel.h"
#include "Cassque/Model/DiskOperations.h"
#include "Cassque/Model/PreviewDecoder.h"
#include "Cassque/Model/TreeModel.h"
#include "Config/IFileSystem.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IShellIcons.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTreeView.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser
//
//  What the browser window shows, with no window: the tree's nodes, the rows
//  of the list for the active tab's location, the preview of the selection,
//  and the status bar's text.
//
//  THE WINDOW ONLY FORWARDS. A tree click calls SelectTreeNode, a list click
//  SetSelectedRows, a header click SortByColumn; each leaves the rows, the
//  preview and the status recomputed, and the window copies them into its
//  widgets. Every decision is here or in a model beneath, so every decision
//  is reachable from a test with an in-memory host.
//
//  A host folder's rows come from the host file system; a disk image's from
//  its catalog, read once per image and kept in the browser model's cache
//  until something invalidates it.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueBrowser
{
public:
    struct Status
    {
        std::wstring  selection;
        std::wstring  detail;
        std::wstring  freeSpace;
    };

    CassqueBrowser (IFileSystem & fs, IDiskFileIo & fileIo);

    TreeModel &       GetTreeModel    ()       { return m_tree;  }
    BrowserModel &    GetBrowserModel ()       { return m_model; }
    DiskOperations &  GetOperations   ()       { return m_operations; }

    //  The tree's roots, collapsed with children unloaded, and one node's
    //  children for the tree widget's provider.
    void                       GetTreeRoots    (std::vector<DxuiTreeNode> & outNodes);
    std::vector<DxuiTreeNode>  GetTreeChildren (const std::wstring & id);

    //  Navigates the active tab to the node's location, opening a first tab
    //  when there is none. A root has no location and leaves the list empty.
    HRESULT  SelectTreeNode (const std::wstring & id);

    //  Loads the active tab's location into rows, applying its sort.
    HRESULT  Refresh();

    //  Rereads the location, dropping every cached catalog, and reselects
    //  the rows whose names were selected, for a return to the window after
    //  another program may have changed what it shows.
    HRESULT  Reload (bool keepSelection);

    //  What the tree's context menu can do with a node: add a host folder to
    //  Casso's known folders, or remove a known folder from them.
    bool  CanAddToCasso      (const std::wstring & id) const;
    bool  CanRemoveFromCasso (const std::wstring & id) const;
    bool  TryGetNodePath     (const std::wstring & id, std::wstring & outPath) const;

    //  Tabs. A new tab opens where the active one is. The last tab does not
    //  close, since the window always shows somewhere. Switching restores
    //  the tab's selection by name. Restoring opens one tab per location and
    //  makes the first active; an empty list leaves the tabs as they are.
    size_t        NewTab      ();
    bool          CloseTab    (size_t index);
    bool          MoveTab     (size_t from, size_t to) { return m_model.MoveTab (from, to); }
    bool          SwitchTab   (size_t index);
    void          RestoreTabs (const std::vector<Location> & locations);
    std::wstring  GetTabLabel (size_t index) const;

    static std::wstring  GetLocationLabel (const Location & location);

    //  Opens a folder or disk image row in place, as a double-click does.
    //  False, with nothing changed, for a row that is neither.
    bool  OpenRow (int row);

    bool  CanGoUp () const;
    bool  GoBack    ();
    bool  GoForward ();
    bool  GoUp      ();

    //  The folder holding a host path, or empty at a drive root.
    static std::wstring  GetParentFolder (const std::wstring & path);
    static std::wstring  JoinPath        (const std::wstring & folder, const std::wstring & name);

    void  SortByColumn     (int column);
    void  SetSelectedRows  (const std::vector<int> & rows);
    void  SetDisassemble   (bool disassemble);

    const std::vector<CatalogRow> &  GetRows       () const { return m_rows;      }
    const PreviewContent &           GetPreview    () const { return m_preview;   }
    const std::wstring &             GetListError  () const { return m_listError; }
    const Status &                   GetStatus     () const { return m_status;    }
    const std::vector<int> &         GetSelectedRows () const { return m_selectedRows; }
    Location                         GetLocation   () const;
    VolumeKind                       GetVolumeKind () const { return m_kind; }
    bool                             IsImageLocation () const { return m_isImage; }
    const VolumeListing &            GetListing    () const { return m_listing; }

    //  The catalog entries behind the selected rows, in row order, when the
    //  location is an image; empty otherwise.
    void  GetSelectedEntries (std::vector<FileEntry> & outEntries) const;

    //  The host paths behind the selected rows, when the location is a host
    //  folder; empty otherwise.
    void  GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const;

    //  Whether the selected rows are all disk images in a host folder.
    bool  AreSelectedRowsImages() const;

    //  The list widget's columns, in CatalogModel::Column order, and one
    //  row's cells.
    static std::vector<DxuiListView::Column>  GetColumns();
    //  The shell's icons for the tree and the list. None set, none drawn.
    void  SetShellIcons (IShellIcons * icons) { m_shellIcons = icons; }

    static std::vector<DxuiListView::Cell>    ToCells (const CatalogRow & row, const Location & at = Location(), IShellIcons * icons = nullptr);

    //  The narrower set a disk image's catalog uses in the preview pane.
    static std::vector<DxuiListView::Column>  GetCatalogPreviewColumns();
    static std::vector<DxuiListView::Cell>    ToCatalogPreviewCells (const CatalogRow & row);

    static DxuiTreeNode  ToTreeNode (const TreeNode & node, IShellIcons * icons = nullptr);

    static std::wstring  FormatSize      (uint64_t bytes);
    static std::wstring  FormatModified  (int64_t unixSeconds, bool wallClock);
    static std::wstring  FormatSelection (size_t selected, size_t total);

private:
    //  Refresh for a navigation, whose failure is already in the list error.
    void     ReloadAfterNavigation();
    HRESULT  LoadHostFolder (const std::wstring & path);
    HRESULT  LoadRoot       (const std::wstring & id);
    HRESULT  LoadImage      (const std::wstring & path);
    void     SortRows();
    void     UpdatePreview();
    void     UpdateStatus();
    bool     TryGetSelectedEntry (const FileEntry *& outEntry) const;

    static std::shared_ptr<const DxuiIconImage>  GetRowIcon  (const CatalogRow & row, const Location & at, IShellIcons & icons);
    static std::shared_ptr<const DxuiIconImage>  GetNodeIcon (const TreeNode & node, IShellIcons & icons);

    IFileSystem                       & m_fs;
    TreeModel                           m_tree;
    BrowserModel                        m_model;
    DiskOperations                      m_operations;
    MemoryBus                           m_bus;
    std::map<std::wstring, TreeNode>    m_nodes;
    std::vector<CatalogRow>             m_rows;
    std::vector<FileSystemEntry>        m_hostEntries;
    std::vector<TreeNode>               m_rootChildren;
    std::wstring                        m_rootId;
    VolumeListing                       m_listing;
    VolumeKind                          m_kind         = VolumeKind::Unknown;
    bool                                m_isImage      = false;
    std::vector<int>                    m_selectedRows;
    PreviewContent                      m_preview;
    std::wstring                        m_listError;
    Status                              m_status;
    IShellIcons                       * m_shellIcons   = nullptr;
};
