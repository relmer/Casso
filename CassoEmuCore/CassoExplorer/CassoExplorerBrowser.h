#pragma once

#include "Pch.h"
#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Browser/BrowserHostInterfaces.h"
#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/BrowserColumnFilters.h"
#include "CassoExplorer/Browser/BrowserNameFilter.h"
#include "CassoExplorer/Browser/BrowserNavigator.h"
#include "CassoExplorer/Browser/BrowserRowOrder.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Browser/BrowserSearchMatches.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"
#include "CassoExplorer/Browser/IBrowserListing.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/StockDisksListing.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/TypedPathHistory.h"
#include "CassoExplorer/Model/IImageRecords.h"
#include "CassoExplorer/Model/ImageVolumeFacts.h"
#include "Devices/Disk/ImageClassCache.h"
#include "Shell/DiskMru.h"

class CassoLinkConnections;
class FilesystemChoices;
class PhysicalCards;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser
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

class CassoExplorerBrowser : private IBrowserListing,
                             private IBrowserSelectionSource,
                             private IBrowserLoaderHost,
                             private IBrowserPreviewHost,
                             private IBrowserCellsHost
{
public:
    using Status = BrowserStatus;

    CassoExplorerBrowser (IFileSystem & fs, IDiskFileIo & fileIo);

    TreeModel        & GetTreeModel    ()       { return m_tree;  }
    const TreeModel  & GetTreeModel    () const { return m_tree;  }
    HostFolderReader & GetHostFolders  ()       { return m_hostFolders; }
    BrowserModel     & GetBrowserModel ()       { return m_model; }
    DiskOperations   & GetOperations   ()       { return m_operations; }
    ImageClassCache  & GetImageClasses ()       { return m_imageClasses; }

    //  The tree's roots, collapsed with children unloaded, and one node's
    //  children for the tree widget's provider.
    void                       GetTreeRoots    (std::vector<DxuiTreeNode> & outNodes);
    std::vector<DxuiTreeNode>  GetTreeChildren (const std::wstring & id);

    //  Navigates the active tab to the node's location, opening a first tab
    //  when there is none. A root has no location and leaves the list empty.
    HRESULT  SelectTreeNode (const std::wstring & id);

    //  Loads the active tab's location into rows, applying its sort.
    HRESULT  Refresh() override;

    //  Rereads the location, dropping every cached catalog, and reselects
    //  the rows whose names were selected, for a return to the window after
    //  another program may have changed what it shows.
    HRESULT  Reload (bool keepSelection);

    //  What the tree's context menu can do with a node: add a host folder to
    //  Casso's known folders, or remove a known folder from them.
    bool  CanAddToCasso      (const std::wstring & id) const;
    bool  CanRemoveFromCasso (const std::wstring & id) const;
    bool  IsKnownFolder      (const std::wstring & path) const { return m_tree.IsKnownFolder (path); }
    bool  TryGetNodePath     (const std::wstring & id, std::wstring & outPath) const;

    //  Where a tree node or a list row leads, without going there: a folder, a
    //  disk image, or a directory inside one. False for anything else.
    bool  TryGetNodeLocation (const std::wstring & id, Location & outLocation) const;
    bool  TryGetTreeNode     (const std::wstring & id, DxuiTreeNode & outNode) const;

    //  The listing BrowserRowPaths reads a row's location and path from.
    BrowserRowPaths::Listing  GetRowListing () override;

    //  The tabs, and Back, Forward, Up and the address bar.
    BrowserNavigator        & GetNavigator ()       { return m_navigator; }
    const BrowserNavigator  & GetNavigator () const { return m_navigator; }

    //  The list's loading, the preview of the selection, and the cells and
    //  status text.
    BrowserLoader               & GetLoader        ()       { return m_loader; }
    const BrowserLoader         & GetLoader        () const { return m_loader; }
    BrowserPreviewer            & GetPreviewer     ()       { return m_previewer; }
    const BrowserPreviewer      & GetPreviewer     () const { return m_previewer; }
    BrowserSearchMatches        & GetSearchMatches ()       { return m_searchMatches; }
    const BrowserSearchMatches  & GetSearchMatches () const { return m_searchMatches; }
    BrowserArchives             & GetArchives      ()       { return m_archives; }
    const BrowserArchives       & GetArchives      () const { return m_archives; }
    BrowserCells                & GetCells         ()       { return m_cells; }
    const BrowserCells          & GetCells         () const { return m_cells; }

    //  Opens a folder or disk image row in place, as a double-click does.
    //  False, with nothing changed, for a row that is neither.
    bool  OpenRow (int row);

    //  What an address bar separator lists: the folders and images in a host
    //  folder.
    void  GetFolderChildren  (const Location & location, std::vector<BrowserModel::AddressSegment> & outChildren) { BrowserModel::GetFolderChildren (m_fs, location, outChildren); }

    //  The rows' sort, Group by and Home sections.
    BrowserRowOrder        & GetRowOrder      ()       { return m_rowOrder; }
    const BrowserRowOrder  & GetRowOrder      () const { return m_rowOrder; }
    BrowserColumnFilters   & GetColumnFilters ()       { return m_columnFilters; }

    bool                              IsHomeLocation  () const override;
    void                              SetHomeTab      (int tab);
    void                              RemoveHomeRow   (const std::wstring & shellId);
    void                              RequestHomeRead () override;
    void                              SetTabPreview   (bool open) { if (m_model.HasTabs()) { m_model.GetActiveTab().previewOpen = open; } }

    //  Whether the list is on a search that is being read, and stopping that
    //  read, so the list has the results found before the stop.
    bool                              IsSearchRunning       ();
    void                              StopSearch            ();
    bool                              IsTabPreview          () { return m_model.HasTabs() && m_model.GetActiveTab().previewOpen; }
    void                              SetTabPreviewExpanded (bool expanded) { if (m_model.HasTabs()) { m_model.GetActiveTab().previewExpanded = expanded; } }
    bool                              IsTabPreviewExpanded  () const { return m_model.HasTabs() && m_model.GetActiveTab().previewExpanded; }
    void                              SetTabTopRow          (int row) { if (m_model.HasTabs()) { m_model.GetActiveTab().listTopRow = row; } }
    int                               GetTabTopRow          () { return m_model.HasTabs() ? m_model.GetActiveTab().listTopRow : 0; }
    void                              SetTabTreeRow         (int row) { if (m_model.HasTabs()) { m_model.GetActiveTab().treeTopRow = row; } }
    int                               GetTabTreeRow         () { return m_model.HasTabs() ? m_model.GetActiveTab().treeTopRow : -1; }

    //  The selected rows, and selecting them.
    BrowserSelection        & GetSelection ()       { return m_selection; }
    const BrowserSelection  & GetSelection () const { return m_selection; }

    const std::vector<CatalogRow>  & GetRows () const { return m_list.rows; }

    const std::wstring      & GetListError          () const { return m_list.listError; }
    bool                      IsListLoading         () const { return m_list.listError == s_kWorkingText; }
    const Status            & GetStatus             () const { return m_list.status;    }
    const std::vector<int>  & GetSelectedRows       () const { return m_list.selectedRows; }
    Location                  GetLocation           () const override;
    VolumeKind                GetVolumeKind         () const { return m_list.kind; }
    bool                      IsAppleDriveLocation  () const { return m_list.isAppleDrive; }
    bool                      IsListingKept         () const { return m_list.listingKept; }

    //  An Apple drive is open, whether or not its catalog could be read, as
    //  for a disk to format.
    bool                             IsAppleDriveOpen () const { return GetLocation().kind == Location::Kind::AppleDrive; }

    //  An image or an Apple drive: a volume whose catalog is in the list.
    bool  IsVolumeLocation () const override { return m_list.isImage || m_list.isAppleDrive; }
    bool  IsImageLocation  () const override { return m_list.isImage; }

    //  An image with no catalog, whose partitions are the rows.
    bool  IsPartitionListLocation () const { return m_list.isPartitionList; }

    //  The root being listed, or empty when the list is another location's.
    std::wstring                     GetRootId () const;

    //  Whether the image shown is write-protected, read once when it loads:
    //  the commands that would write to it ask on every repaint, which is too
    //  often to hit the file system.
    bool                   IsWriteProtected () const { return m_list.writeProtected; }
    const VolumeListing  & GetListing       () const override { return m_list.listing; }

    //  The files the selected rows are when they are a search's matches inside
    //  disk images, each with its image, found again in that image's catalog.
    using SearchMatch = BrowserSearchMatch;

    //  A search's row given the name a rename gave its file, without searching
    //  again: the index takes a while to have the new name. A name the query no
    //  longer matches removes the row, and false is returned.
    bool  RenameSearchRow (int row, const std::wstring & newName);

    //  The shell's icons for the tree and the list. None set, none drawn.
    void  SetShellIcons (IShellIcons * icons) { m_sources.shellIcons = icons; }

    //  The CassoLink connections an Apple drive is listed and read through. Without
    //  them, an Apple drive lists nothing.
    void                    SetLinks (CassoLinkConnections * links) { m_sources.links = links; }

    //  The file systems the user chose, which outlive this browser.
    void                    SetFilesystemChoices (FilesystemChoices * choices) { m_filesystemChoices = choices; m_operations.SetFilesystemChoices (choices); }
    FilesystemChoices     * GetFilesystemChoices () const { return m_filesystemChoices; }
    CassoLinkConnections  * GetLinks             () const                       { return m_sources.links; }

    //  The cards open through removable drives. Without them, a path on a card
    //  lists nothing.
    void                    SetPhysicalCards     (PhysicalCards * cards)         { m_sources.cards = cards; }

    //  The recent list's images and their load times, read each time the
    //  Casso node is listed, for its Last used column.
    using RecentLoadsFn = std::function<std::vector<DiskMru::Entry> ()>;
    void                    SetRecentLoads       (RecentLoadsFn recentLoads)     { m_sources.recentLoads = std::move (recentLoads); }

    //  Called on a worker's thread when an image's file system and volume
    //  name are read; ApplyVolumeFacts, on the window's thread, puts them in
    //  the rows and returns whether any row changed.
    //  A picker's scope: the host rows it lists. The rows a listing drops for
    //  it are counted, for the status bar.
    using RowScopeFn = std::function<bool (const CatalogRow & row)>;
    void                    SetRowScope          (RowScopeFn scope)              { m_rowScope = std::move (scope); }

    //  The images inserted in the running machine's drives, by path, each
    //  with its drive. Their rows are marked with the drive.
    void                    SetMountedImages     (const std::map<std::wstring, std::wstring> & mounted);

    //  The rows of a picker's "Stock disks" node.
    void                    SetStockDisks        (std::vector<StockDiskRow> disks)  { m_sources.stockDisks = std::move (disks); }
    void                    SetDemoDisks         (std::vector<StockDiskRow> disks)  { m_sources.demoDisks  = std::move (disks); }
    IDiskFileIo           & GetFileIo            ()                              { return m_fileIo; }
    size_t                  GetScopedOutCount    () const                        { return m_scopedOut; }

    //  A picker's name filter: the rows are those whose file name contains
    //  every word of the text. On the Casso node, the stock and demo disks
    //  that match are listed too. An empty text lists every row in the
    //  picker's scope.
    void                       SetNameFilter        (const std::wstring & text);
    const BrowserNameFilter  & GetNameFilter        () const                     { return m_nameFilter; }

    void                    SetVolumeFactsNotify (std::function<void ()> notify) { m_onVolumeFacts = std::move (notify); }
    bool                    ApplyVolumeFacts     ();

    //  Whether an image has a catalog to open: the volume facts worker's
    //  answer when it has read the image, so painting never decodes one.
    bool                    CanOpenImage         (const std::wstring & path);

    //  Until set, a card that is not open is not opened, so the tabs restored
    //  at launch raise no UAC prompt.
    void                    SetCardOpenAllowed   (bool allowed)                 { m_sources.cardOpenAllowed = allowed; }
    PhysicalCards         * GetPhysicalCards     () const                       { return m_sources.cards; }

    //  The stored records of what images contain; none set, every image is
    //  read each time.
    void  SetImageRecords (IImageRecords * records) { m_records = records; m_catalog.SetRecords (records); }

    //  What lists the Recycle Bin. None set, it shows empty.
    void  SetShellVerbs (IShellItemVerbs * verbs);

    //  The keys a shell folder's listing is read under, for the list and the
    //  tree apart.
    static constexpr const wchar_t *  s_kListKey = L"list:";

    //  An archive's preview, read on a thread of its own when on a network.
    static constexpr const wchar_t *  s_kPreviewKey = L"preview:";

    //  The list or preview message while a slow read is under way.
    static constexpr const wchar_t *  s_kWorkingText = L"Working on it...";
    static constexpr const wchar_t *  s_kTreeKey = L"tree:";

    //  Shell folders read on threads of their own; the window sets where the
    //  message goes when one arrives.
    CassoExplorerShellListings &  GetShellListings () { return m_shellListings; }

    //  Host folders on a network path, read on threads of their own the same
    //  way, so an unreachable share never stops the window. Without a lister
    //  set, every host folder is read through the file system at once.
    static constexpr const wchar_t *  s_kHostListKey = L"hostlist:";
    static constexpr const wchar_t *  s_kHostTreeKey = L"hosttree:";

    //  Quick access as the shell keeps it, read again after a pin changes.
    bool  IsPinnedToQuickAccess (const std::wstring & folder) const { return m_tree.IsPinnedFolder (folder); }
    void  RefreshShellRoots     ()                                  { m_tree.RefreshShellRoots(); }

    //  Which hidden items the tree and the list show, and whether compressed
    //  items are colored. The next listing follows them.
    void                   SetFolderOptions (const FolderOptions & options) { m_hostFolders.SetFolderOptions (options); }
    const FolderOptions &  GetFolderOptions () const                        { return m_hostFolders.GetFolderOptions(); }

    static constexpr uint32_t  kCompressedDarkArgb  = 0xFF3A99E8u;
    static constexpr uint32_t  kCompressedLightArgb = 0xFF0000FFu;

private:
    //  IBrowserSelectionSource, for the selection.
    bool      TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const override { return m_loader.TryGetSelectedCassoLinkPaths (outPaths); }
    void      UpdatePreviewAndStatus       () override                                         { m_previewer.UpdatePreview(); m_cells.UpdateStatus(); }

    //  IBrowserListing, for the navigator.
    void      ReloadAfterNavigation () override                                          { m_loader.ReloadAfterNavigation(); }
    void      ReturnToTab           (const std::vector<std::wstring> & rowKeys) override;
    bool      IsOnMissingDrive      (const Location & location) const override           { return m_loader.IsOnMissingDrive (location); }
    Location  GetShellParent        () const override                                    { return m_list.shellParent; }

    //  An image's facts for the facts worker, from its stored record when
    //  it has one. Without one, the image is read through the catalog so the
    //  record is stored for the next read.
    VolumeFacts  ReadVolumeFacts (const std::string & path);

    //  IBrowserLoaderHost, for the loader.
    void  QueueVolumeFacts   (const std::wstring & folder) override;
    void  NavigateToLocation (const Location & location) override  { m_navigator.NavigateToLocation (location); }

    CassoExplorerShellListings              m_shellListings;
    Location                                m_homeParent;
    bool                                    m_returningToTab    = false;
    IFileSystem                           & m_fs;
    IDiskFileIo                           & m_fileIo;
    ImageClassCache                         m_imageClasses;
    std::function<void ()>                  m_onVolumeFacts;
    std::atomic<IImageRecords *>            m_records           { nullptr };
    ImageVolumeFacts                        m_volumeFacts       { m_fileIo, [this] () { if (m_onVolumeFacts) { m_onVolumeFacts(); } }, [this] (const std::string & path) { return ReadVolumeFacts (path); } };
    HostFolderReader                        m_hostFolders;
    TreeModel                               m_tree              { m_fs, m_hostFolders };
    BrowserModel                            m_model;
    DiskOperations                          m_operations;
    DiskImageCatalog                        m_catalog           { m_operations };
    BrowserColumnFilters                    m_columnFilters;
    RowScopeFn                              m_rowScope;
    std::map<std::wstring, std::wstring>    m_mountedImages;   // by CassoImagesListing::MakePathKey
    size_t                                  m_scopedOut         = 0;
    BrowserNameFilter                       m_nameFilter;
    FilesystemChoices                     * m_filesystemChoices = nullptr;
    BrowserListState                        m_list;
    BrowserSources                          m_sources;

    //  The browser's components, each with the parts it uses.
    BrowserParts          m_parts          { m_fs, m_fileIo, m_imageClasses, m_catalog, m_tree, m_hostFolders, m_model, m_operations, m_shellListings, m_list, m_sources };
    BrowserSelection      m_selection      { m_model, m_list.rows, m_list.selectedRows, *this };
    BrowserArchives       m_archives       { m_parts, m_selection };
    BrowserLoader         m_loader         { m_parts, m_archives, *this };
    BrowserSearchMatches  m_searchMatches  { m_parts };
    BrowserPreviewer      m_previewer      { m_parts, m_selection, m_searchMatches, m_archives, *this };
    BrowserCells          m_cells          { m_parts, *this };
    BrowserNavigator      m_navigator      { m_model, m_fs, m_hostFolders, *this };
    BrowserRowOrder       m_rowOrder       { m_model, m_list.rows, m_list.selectedRows, m_columnFilters, *this };
};
