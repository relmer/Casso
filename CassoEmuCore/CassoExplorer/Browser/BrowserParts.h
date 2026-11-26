#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserTypes.h"
#include "CassoExplorer/CassoExplorerShellListings.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/Location.h"
#include "CassoExplorer/Model/StockDisksListing.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Config/IFileSystem.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "Shell/DiskMru.h"

class BrowserModel;
class CassoLinkConnections;
class DiskImageCatalog;
class DiskOperations;
class HostFolderReader;
class IDiskFileIo;
class IShellIcons;
class IShellItemVerbs;
class ImageClassCache;
class PhysicalCards;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserListState
//
//  The list for the active tab's location: its rows and selection, the
//  volume listed, the kind of location, the error in place of rows, and the
//  status bar's text.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserListState
{
    std::vector<CatalogRow>       rows;
    std::vector<int>              selectedRows;
    VolumeListing                 listing;
    VolumeKind                    kind            = VolumeKind::Unknown;
    bool                          isImage         = false;
    bool                          isAppleDrive    = false;
    bool                          isPartitionList = false;
    bool                          listingKept     = false;   // an Apple drive's listing as last read, while it is read again
    bool                          writeProtected  = false;
    std::wstring                  listError;
    Location                      shellParent;               // where Up goes from the shell folder listed; none at the top
    std::vector<FileSystemEntry>  hostEntries;
    std::vector<TreeNode>         rootChildren;
    BrowserStatus                 status;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSources
//
//  What the window sets for the browser to list from: CassoLink, the cards,
//  the shell, the recent list, and a picker's stock and demo disks.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserSources
{
    CassoLinkConnections                           * links           = nullptr;
    PhysicalCards                                  * cards           = nullptr;
    IShellIcons                                    * shellIcons      = nullptr;
    IShellItemVerbs                                * shellVerbs      = nullptr;
    std::function<std::vector<DiskMru::Entry> ()>    recentLoads;
    std::vector<StockDiskRow>                        stockDisks;
    std::vector<StockDiskRow>                        demoDisks;
    bool                                             cardOpenAllowed = false;   // until set, a card that is not open is not opened
};





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserParts
//
//  The browser's models, services and state that its components use.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserParts
{
    IFileSystem                 & fs;
    IDiskFileIo                 & fileIo;
    ImageClassCache             & imageClasses;
    DiskImageCatalog            & catalog;
    TreeModel                   & tree;
    HostFolderReader            & hostFolders;
    BrowserModel                & model;
    DiskOperations              & operations;
    CassoExplorerShellListings  & shellListings;
    BrowserListState            & list;
    const BrowserSources        & sources;
};
