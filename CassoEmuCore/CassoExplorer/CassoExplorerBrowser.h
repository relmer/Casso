#pragma once

#include "Pch.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Browser/BrowserCells.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/TypedPathHistory.h"

class CassoLinkConnections;
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

class CassoExplorerBrowser
{
public:
    using Status = BrowserStatus;

    CassoExplorerBrowser (IFileSystem & fs, IDiskFileIo & fileIo);

    TreeModel        & GetTreeModel    ()       { return m_tree;  }
    const TreeModel  & GetTreeModel    () const { return m_tree;  }
    BrowserModel     & GetBrowserModel ()       { return m_model; }
    DiskOperations   & GetOperations   ()       { return m_operations; }

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
    bool  IsKnownFolder      (const std::wstring & path) const { return m_tree.IsKnownFolder (path); }
    bool  TryGetNodePath     (const std::wstring & id, std::wstring & outPath) const;

    //  Where a tree node or a list row leads, without going there: a folder, a
    //  disk image, or a directory inside one. False for anything else.
    bool  TryGetNodeLocation (const std::wstring & id, Location & outLocation) const;
    bool  TryGetRowLocation  (int row, Location & outLocation);

    //  The path a row stands for, as Copy as path gives it: a host item's full
    //  path, or an entry inside an image after the image's address.
    bool  TryGetRowPath      (int row, std::wstring & outPath) const;

    //  Tabs. A new tab opens where the active one is. The last tab does not
    //  close, since the window always shows somewhere. Switching restores
    //  the tab's selection by name. Restoring opens one tab per location and
    //  makes the first active; an empty list leaves the tabs as they are.
    size_t        NewTab      ();
    bool          CloseTab    (size_t index);
    bool          MoveTab     (size_t from, size_t to) { return m_model.MoveTab (from, to); }
    bool          SwitchTab   (size_t index);
    void          RestoreTabs (const std::vector<Location> & locations);
    void          RestoreTabs (std::vector<BrowserModel::Tab> tabs, size_t active);

    //  A tab arriving from another window goes in at `index`, history and view
    //  included, and becomes the active tab.
    size_t                           InsertTab              (size_t index, BrowserModel::Tab tab);
    std::vector<DxuiListView::Cell>  GetArchivePreviewCells (const CatalogRow & row) const { return m_previewer.GetArchivePreviewCells (row); }

private:

public:
    std::wstring  GetTabLabel (size_t index) const { return m_cells.GetTabLabel (index); }

    //  Opens a location in a new tab and switches to it. The tab menu's other
    //  commands: a copy of a tab where it is, and closing every tab but one or
    //  every tab after one. The last tab still never closes.
    size_t        OpenInNewTab     (const Location & location);
    size_t        DuplicateTab     (size_t index);
    bool          CloseOtherTabs   (size_t index);
    bool          CloseTabsToRight (size_t index);

    //  Opens a folder or disk image row in place, as a double-click does.
    //  False, with nothing changed, for a row that is neither.
    bool  OpenRow (int row);

    bool  CanGoUp () const;

    //  Where Up goes from the active tab, for its tooltip and for GoUp.
    bool  TryGetUpTarget (Location & outTarget) const;
    bool  GoBack         ();
    bool  GoForward      ();

    //  Several steps at once, as a pick from the Back or Forward list takes.
    bool  GoBackBy    (size_t steps);
    bool  GoForwardBy (size_t steps);
    bool  GoUp        ();

    //  Navigates the active tab to a location, as a click on an address bar
    //  segment does, or to a typed path. False, with nothing changed, for a
    //  path that is not a folder, an image or a directory inside one.
    void  NavigateToLocation (const Location & location);

    //  When the folder or image the active tab shows is gone from the disk,
    //  moves to the nearest folder above it that is still there, as Explorer
    //  does when the folder it shows is deleted. False, with nothing
    //  changed, when the location still exists or is not on the host.
    bool  LeaveMissingLocation () { return m_loader.LeaveMissingLocation(); }
    bool  NavigateToAddress    (const std::wstring & text);

    //  The paths typed into the address bar. Held here because this is where a
    //  typed path is told apart from a click, and where its navigation is known
    //  to have succeeded. Seeded from the stored copy at startup.
    TypedPathHistory        & GetTypedPaths ()       { return m_typedPaths; }
    const TypedPathHistory  & GetTypedPaths () const { return m_typedPaths; }

    //  What an address bar separator lists: the folders and images in a host
    //  folder.
    void  GetFolderChildren  (const Location & location, std::vector<BrowserModel::AddressSegment> & outChildren) { BrowserModel::GetFolderChildren (m_fs, location, outChildren); }

    //  The folder holding a host path, or empty at a drive root.
    static std::wstring  GetParentFolder (const std::wstring & path);
    static std::wstring  JoinPath        (const std::wstring & folder, const std::wstring & name);

    void  SortByColumn     (int column);

    //  Explorer's Group by: the rows ordered by group, then by the sort
    //  within each, and the groups for the list to draw headers for.
    void                              SetGroupBy    (RowGrouping::Field field, bool descending);

    //  File Explorer's column filters: the groups a column's rows fall in, as
    //  Group by groups them, and the rows shown only in the groups checked.
    //  The choices are the folder's before filtering; a filter ends when the
    //  folder shown changes.
    static RowGrouping::Field                 GetFilterField     (size_t column);
    const std::vector<RowGrouping::Group>   & GetFilterChoices   (RowGrouping::Field field) const;
    bool                                      IsFiltering        (RowGrouping::Field field) const { return m_filters.contains (field); }
    bool                                      IsFilterChecked    (RowGrouping::Field field, const std::wstring & label) const;
    void                                      ToggleFilter       (RowGrouping::Field field, const std::wstring & label);
    void                                      ClearFilter        (RowGrouping::Field field);

    //  A folder's own sort and grouping, set together as it opens.
    void                              SetSortAndGroup (CatalogModel::Column column, bool descending, RowGrouping::Field group, bool groupDescending);
    std::vector<DxuiListView::Group>  GetListGroups   () const;
    void                              SetSelectedRows (const std::vector<int> & rows);

    //  The preview made again for the selection, as when a slow read arrives.
    void  RefreshPreview   () { m_previewer.UpdatePreview(); }

    //  A shell preview that failed or took too long is replaced by the file's
    //  bytes.
    void  ShowShellPreviewBytes () { m_previewer.ShowShellPreviewBytes(); }
    void  SetDisassemble        (bool disassemble) { m_previewer.SetDisassemble (disassemble); }

    const std::vector<CatalogRow>  & GetRows    () const { return m_rows;      }
    const PreviewContent           & GetPreview () const { return m_previewer.m_preview;   }

    //  The monitor a picture preview is drawn for, with the custom monitor's
    //  color; a change draws the current picture again.
    void                     SetPreviewMonitor (PicturePreview::Monitor monitor, uint32_t customArgb = PicturePreview::kDefaultCustomArgb) { m_previewer.SetPreviewMonitor (monitor, customArgb); }
    PicturePreview::Monitor  GetPreviewMonitor () const { return m_previewer.m_previewMonitor; }

    //  The monitor the current picture is drawn on: the chosen one, except
    //  for a double hi-res picture, whose monitor follows its form. Its color
    //  form is drawn on the color monitor, and its monochrome form on the
    //  chosen monochrome monitor, or the white one when the color monitor is
    //  chosen.
    PicturePreview::Monitor  GetPictureMonitor () const { return m_previewer.GetPictureMonitor(); }

    //  A double hi-res picture drawn in color or as its bits, whichever the
    //  automatic choice was not; again to go back. False for any other preview.
    bool                      TogglePictureColor    () { return m_previewer.TogglePictureColor(); }
    bool                      CanTogglePictureColor () const { return m_previewer.CanTogglePictureColor(); }
    const std::wstring      & GetListError          () const { return m_loader.m_listError; }
    const Status            & GetStatus             () const { return m_status;    }
    const std::vector<int>  & GetSelectedRows       () const { return m_selectedRows; }
    Location                  GetLocation           () const;
    VolumeKind                GetVolumeKind         () const { return m_kind; }
    bool                      IsAppleDriveLocation  () const { return m_isAppleDrive; }
    bool                      IsListingKept         () const { return m_listingKept; }

    //  An Apple drive is open, whether or not its catalog could be read, as
    //  for a disk to format.
    bool                             IsAppleDriveOpen () const { return GetLocation().kind == Location::Kind::AppleDrive; }

    //  An image or an Apple drive: a volume whose catalog is in the list.
    bool  IsVolumeLocation () const { return m_isImage || m_isAppleDrive; }
    bool  IsImageLocation  () const { return m_isImage; }

    //  The root being listed, or empty when the list is another location's.
    std::wstring                     GetRootId () const;

    //  Whether the image shown is write-protected, read once when it loads:
    //  the commands that would write to it ask on every repaint, which is too
    //  often to hit the file system.
    bool                   IsWriteProtected () const { return m_loader.m_writeProtected; }
    const VolumeListing  & GetListing       () const { return m_listing; }

    //  The catalog entries behind the selected rows, in row order, when the
    //  location is an image; empty otherwise.
    void  GetSelectedEntries (std::vector<FileEntry> & outEntries) const;

    //  An entry's path as the volume layer addresses it: its name in the volume
    //  directory, or the listed subdirectory's path and then its name.
    std::string  GetEntryPath (const FileEntry & entry) const;

    //  The host paths behind the selected rows, when the location is a host
    //  folder; empty otherwise.
    void  GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const;

    //  The selected CassoLink items written to their files, so they open and
    //  drag as files; false when any could not be written.
    bool  PrepareCassoLinkItems () { return m_loader.PrepareCassoLinkItems(); }

    //  The paths of the selection when every row is a CassoLink item.
    bool  TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const { return m_loader.TryGetSelectedCassoLinkPaths (outPaths); }

    //  The next list of a linked machine or drive reads its disks again, as
    //  a refresh does.
    void  ForgetLinkVisit () { m_loader.m_linkVisit = Location(); m_loader.m_linkReadAt.clear(); }

    //  The archive Extract all applies to, selected or open; empty for none.
    std::wstring  GetExtractTarget () const { return m_previewer.GetExtractTarget(); }

    //  The files the selected rows are when they are a search's matches inside
    //  disk images, each with its image, found again in that image's catalog.
    using SearchMatch = BrowserSearchMatch;

    void  GetSelectedSearchMatches     (std::vector<SearchMatch> & outMatches) { m_previewer.GetSelectedSearchMatches (outMatches); }
    bool  AreSelectedRowsSearchMatches () const { return m_previewer.AreSelectedRowsSearchMatches(); }
    bool  TryFindSearchMatch           (const CatalogRow & row, SearchMatch & outMatch, std::wstring & outError) { return m_previewer.TryFindSearchMatch (row, outMatch, outError); }

    //  Whether the selected rows are all disk images in a host folder.
    bool  AreSelectedRowsImages() const;

    //  Every row's key, in row order, as a selection stores them: a name and
    //  its occurrence among rows with that name. What a refresh matches rows
    //  by, since an index moves whenever a row is added or removed above it.
    void  GetRowKeys (std::vector<std::wstring> & outKeys) const;
    //  The shell's icons for the tree and the list. None set, none drawn.
    void  SetShellIcons (IShellIcons * icons) { m_shellIcons = icons; }

    //  The CassoLink connections an Apple drive is listed and read through. Without
    //  them, an Apple drive lists nothing.
    void                    SetLinks (CassoLinkConnections * links) { m_links = links; }
    CassoLinkConnections *  GetLinks () const                       { return m_links; }

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
    CassoExplorerHostListings &  GetHostListings  () { return m_loader.m_hostListings; }


    //  Quick access as the shell keeps it, read again after a pin changes.
    bool  IsPinnedToQuickAccess (const std::wstring & folder) const { return m_tree.IsPinnedFolder (folder); }
    void  RefreshShellRoots     ()                                  { m_tree.RefreshShellRoots(); }

    //  The shell's ids for the Recycle Bin items selected.
    void  GetSelectedRecycledIds (std::vector<std::wstring> & outIds) const { m_loader.GetSelectedRecycledIds (outIds); }
    void  GetAllRecycledIds      (std::vector<std::wstring> & outIds) const { m_loader.GetAllRecycledIds (outIds); }

    //  The catalog entry a row inside an image stands for.
    bool  TryGetRowEntry (int row, FileEntry & outEntry) const { return m_loader.TryGetRowEntry (row, outEntry); }

    //  Which hidden items the tree and the list show, and whether compressed
    //  items are colored. The next listing follows them.
    void                   SetFolderOptions (const FolderOptions & options) { m_loader.m_folderOptions = options; m_tree.SetFolderOptions (options); }
    const FolderOptions &  GetFolderOptions () const                        { return m_loader.m_folderOptions; }

    static constexpr uint32_t  kCompressedDarkArgb  = 0xFF3A99E8u;
    static constexpr uint32_t  kCompressedLightArgb = 0xFF0000FFu;

    //  Why an image cannot be read as a disk at all, as the list's message
    //  says it; empty for one that can, a whole disk with no file system to
    //  list included, since that is a disk and not a broken file.
    std::wstring  GetImageProblem (const std::wstring & imagePath) { return m_loader.GetImageProblem (imagePath); }

    //  A typed path with a root and no drive, put on the system drive.
    static std::wstring  RootOnSystemDrive (const std::wstring & text);

private:

    CassoExplorerShellListings  m_shellListings;
    void  SortRows               ();
    void  GroupRows              ();
    void  FilterRows             ();
    void  ResortKeepingSelection ();
    bool  TryGetSelectedEntry    (const FileEntry *& outEntry) const;

    //  The key a selection stores for a row: its name and its occurrence among
    //  rows with that name, since a DOS 3.3 catalog can repeat a name.
    std::wstring  GetSelectionKey (size_t row) const;

    //  Selects the rows whose keys are in `names`, in one pass over the rows.
    void  SelectRowsByKeys (const std::vector<std::wstring> & names);

    IFileSystem                       & m_fs;
    TreeModel                           m_tree;
    BrowserModel                        m_model;
    DiskOperations                      m_operations;
    TypedPathHistory                    m_typedPaths;
    std::vector<CatalogRow>             m_rows;
    std::vector<RowGrouping::Group>     m_rowGroups;       // each row's group, when grouped
    std::map<RowGrouping::Field, std::set<std::wstring>>             m_filters;          // the checked groups of each filtered column
    std::map<RowGrouping::Field, std::vector<RowGrouping::Group>>    m_filterChoices;    // each column's groups before filtering
    Location                                                          m_filterLocation;   // the folder the filters are for
    bool                                                              m_rowsUnfiltered = false;   // loaded since the last filter
    VolumeListing                       m_listing;
    VolumeKind                          m_kind           = VolumeKind::Unknown;
    bool                                m_isImage        = false;
    bool                                m_isAppleDrive   = false;
    bool                                m_listingKept    = false;   // an Apple drive's listing as last read, while it is read again
    CassoLinkConnections              * m_links          = nullptr;
    std::vector<int>                    m_selectedRows;
    Status                              m_status;
    IShellIcons                       * m_shellIcons     = nullptr;
    IShellItemVerbs                   * m_shellVerbs     = nullptr;

    //  The browser's parts, each with the state it uses.
    friend class      BrowserLoader;
    friend class      BrowserPreviewer;
    friend class      BrowserCells;
    BrowserLoader     m_loader         { *this };
    BrowserPreviewer  m_previewer      { *this };
    BrowserCells      m_cells          { *this };
};

