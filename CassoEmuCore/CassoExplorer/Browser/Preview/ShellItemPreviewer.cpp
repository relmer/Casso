#include "Pch.h"

#include "CassoExplorer/Browser/Preview/ShellItemPreviewer.h"
#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/HostTextPreview.h"
#include "Core/PathResolver.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::ShellItemPreviewer
//
////////////////////////////////////////////////////////////////////////////////

ShellItemPreviewer::ShellItemPreviewer (
    const BrowserParts      & parts,
    const PreviewParts      & previewParts,
    const BrowserArchives   & archives,
    IImagePreviews          & images)
    : m_parts        (parts),
      m_previewParts (previewParts),
      m_archives     (archives),
      m_images       (images)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::ShowShellPreviewBytes
//
////////////////////////////////////////////////////////////////////////////////

void ShellItemPreviewer::ShowShellPreviewBytes()
{
    if (m_previewParts.preview.kind == PreviewContent::Kind::Shell)
    {
        m_previewParts.preview.kind = PreviewContent::Kind::Hex;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::PreviewArchive
//
//  What an archive holds, folders first, in the columns a catalog uses.
//
////////////////////////////////////////////////////////////////////////////////

void ShellItemPreviewer::PreviewArchive (const std::wstring & path)
{
    HRESULT                                        hr = S_OK;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;



    if (m_parts.sources.shellVerbs == nullptr)
    {
        return;
    }

    //  One on a network share is read on a thread of its own, with a working
    //  message meanwhile, so an unreachable share never blocks the window.
    if (!PathResolver::IsNetworkPath (path))
    {
        hr = m_parts.sources.shellVerbs->ListShellFolder (path, items);
    }
    else if (!m_parts.shellListings.TryTake (CassoExplorerBrowser::s_kPreviewKey + path, path, items, hr))
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = CassoExplorerBrowser::s_kWorkingText;
        return;
    }

    if (FAILED (hr))
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = PathResolver::IsNetworkPath (path) ? path.substr (path.find_last_of (L"\\/") + 1) + L" is unavailable." : m_archives.DescribeEmptyArchive (path);
        m_previewParts.preview.message = m_previewParts.preview.message.empty() ? L"The archive could not be read" : m_previewParts.preview.message;
        return;
    }

    if (items.empty() && !m_archives.DescribeEmptyArchive (path).empty())
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = m_archives.DescribeEmptyArchive (path);
        return;
    }

    m_previewParts.preview.kind    = PreviewContent::Kind::Catalog;
    m_previewParts.preview.archive = true;

    for (const IShellItemVerbs::ShellFolderItem & item : items)
    {
        CatalogRow  row;

        row.name         = item.name;
        row.typeText     = item.typeText;
        row.sizeBytes    = item.sizeBytes;
        row.modifiedUnix = item.modifiedUnix;
        row.hasModified  = item.hasModified;
        row.isDirectory  = item.isFolder;
        row.shellId      = item.id;

        m_previewParts.preview.rows.push_back (std::move (row));
    }

    std::stable_sort (m_previewParts.preview.rows.begin(), m_previewParts.preview.rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
    {
        return (a.isDirectory != b.isDirectory) ? a.isDirectory : StrCmpLogicalW (a.name.c_str(), b.name.c_str()) < 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::GetArchivePreviewColumns
//
//  The file list's first columns in their default order: Name, Date
//  modified, Type and Size.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> ShellItemPreviewer::GetArchivePreviewColumns()
{
    std::vector<DxuiListView::Column>  columns = BrowserCells::GetColumns();



    columns.resize ((std::min) (columns.size(), kArchivePreviewColumns));

    //  Fitted to what the archive holds, so the narrow pane shows them all.
    for (DxuiListView::Column & column : columns)
    {
        column.widthDip = 0;
        column.stretch  = false;
    }

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::GetArchivePreviewCells
//
//  A row as the file list draws it inside the archive, icon and all.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> ShellItemPreviewer::GetArchivePreviewCells (const CatalogRow & row) const
{
    std::vector<DxuiListView::Cell>  cells = BrowserCells::ToCells (row, Location::MakeShellFolder (row.shellId, row.name), m_parts.sources.shellIcons);



    cells.resize ((std::min) (cells.size(), kArchivePreviewColumns));

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer::PreviewShellItem
//
//  A file the shell lists shows its bytes: one on a disk from the file, as in
//  its own folder, and one with no file of its own, such as one inside a zip,
//  read through the shell up to a limit.
//
////////////////////////////////////////////////////////////////////////////////

void ShellItemPreviewer::PreviewShellItem (const CatalogRow & row)
{
    HRESULT  hr = S_OK;



    if (row.isDirectory)
    {
        return;
    }

    //  A disk image previews as its catalog, as it does in its folder, one in
    //  a zip by its path through the zip.
    if (row.isDiskImage)
    {
        m_images.PreviewImageCatalog (BrowserLoader::GetImagePath (row));
        return;
    }

    if (!row.hostPath.empty())
    {
        m_previewParts.preview.kind     = PreviewContent::Kind::Hex;
        m_previewParts.preview.hostPath = row.hostPath;
        return;
    }

    if (m_parts.sources.shellVerbs == nullptr || row.shellId.empty())
    {
        return;
    }

    hr = m_parts.sources.shellVerbs->ReadItemBytes (row.shellId, kShellPreviewMaxBytes, m_previewParts.preview.bytes);

    if (FAILED (hr))
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = L"The file could not be read";
        return;
    }

    m_previewParts.preview.kind        = PreviewContent::Kind::Hex;
    m_previewParts.preview.hostCharset = HostTextPreview::GetHostCharset (std::span<const Byte> (m_previewParts.preview.bytes.data(), (std::min) (m_previewParts.preview.bytes.size(), HostTextPreview::kTextSampleBytes)));
    m_previewParts.preview.textFile    = m_previewParts.preview.hostCharset != HostCharset::None && m_previewParts.preview.hostCharset != HostCharset::Utf8;

    HostTextPreview::TryPreviewText (row.name, m_previewParts.preview.bytes, m_previewParts.preview);
}
