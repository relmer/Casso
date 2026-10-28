#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserTypes.h"

#include "CassoExplorer/Model/TreeModel.h"
#include "Seams/IShellIcons.h"

class CassoExplorerBrowser;
struct CatalogRow;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells
//
//  What the list and the status bar draw: the columns, each row's cells
//  and colors, the tree's nodes and icons, and sizes, dates, selections
//  and errors as text.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserCells
{
public:
    explicit BrowserCells (CassoExplorerBrowser & browser);

    std::wstring  GetTabLabel (size_t index) const;

    static std::wstring  GetLocationLabel (const Location & location);

    //  The list widget's columns, in CatalogModel::Column order, and one
    //  row's cells.
    static std::vector<DxuiListView::Column>  GetColumns();

    static std::vector<DxuiListView::Cell>       ToCells              (const CatalogRow & row, const Location & at = Location(), IShellIcons * icons = nullptr);
    static std::shared_ptr<const DxuiIconImage>  GetRowIcon           (const CatalogRow & row, const Location & at, IShellIcons & icons);
    static IShellIcons::Kind                     GetAppleTypeIconKind (const std::wstring & typeText);
    static bool                                  IsBasicListingName   (const std::wstring & name);
    static std::shared_ptr<const DxuiIconImage>  GetLocationIcon      (const Location & at, IShellIcons & icons);

    //  The color a row's name is drawn in, or zero for the theme's.
    static uint32_t  GetNameArgb (const CatalogRow & row, const FolderOptions & options, bool dark);
    static constexpr uint32_t  kEncryptedDarkArgb   = 0xFF4EB54Eu;
    static constexpr uint32_t  kEncryptedLightArgb  = 0xFF008000u;

    //  The narrower set a disk image's catalog uses in the preview pane.
    static std::vector<DxuiListView::Column>  GetCatalogPreviewColumns ();
    static std::vector<DxuiListView::Cell>    ToCatalogPreviewCells    (const CatalogRow & row);

    static DxuiTreeNode  ToTreeNode (const TreeNode & node, IShellIcons * icons = nullptr);

    //  File Explorer's default column widths at 100%, and the two catalog
    //  columns sized for what they hold.
    static constexpr int  kNameColumnDip     = 250;
    static constexpr int  kModifiedColumnDip = 144;
    static constexpr int  kTypeColumnDip     = 120;
    static constexpr int  kSizeColumnDip     = 80;
    static constexpr int  kAddressColumnDip  = 72;
    static constexpr int  kLockedColumnDip   = 64;
    static constexpr int  kOriginalColumnDip = 230;
    static constexpr int  kDeletedColumnDip  = 144;

    //  Explorer's selection field: "1 item selected" or "4 items selected",
    //  then two spaces and the files' total size when any file is selected.
    static std::wstring  FormatSelected  (size_t selected, uint64_t bytes, bool anyFile);

    //  An image's refusal as the list shows it: the file's name, then why.
    static std::wstring  FormatImageError (const std::wstring & imagePath, const std::string & message);

    //  The list's and the preview's text for a linked Apple's drive whose
    //  listing failed: the operation's message when there is one.
    static std::wstring  FormatLinkError (HRESULT result, const std::string & message);

    //  A message with each full path in it cut to the name at its end.
    static std::wstring  ShortenPaths     (const std::wstring & message);

    static std::wstring  FormatSize      (uint64_t bytes);

    //  A Content view line: the property's name, drawn muted, then its value.
    static DxuiListView::Cell  MakeContentLine  (const std::wstring & label, const std::wstring & value);
    static std::wstring        FormatSizeColumn (uint64_t bytes);
    static std::wstring        FormatModified   (int64_t unixSeconds, bool wallClock);
    static std::wstring        FormatSelection  (size_t selected, size_t total);
    void                       UpdateStatus     ();

    //  An image's free and total space in the status, and what the volume is
    //  in its tip.
    static void  DescribeVolumeSpace (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind, BrowserStatus & outStatus);

    static std::shared_ptr<const DxuiIconImage>  GetNodeIcon (const TreeNode & node, IShellIcons & icons);

private:
    friend class CassoExplorerBrowser;

    CassoExplorerBrowser  & m_browser;
};
