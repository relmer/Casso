#include "Pch.h"

#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Browser/BrowserHostInterfaces.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Browser/BrowserSearchMatches.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::BrowserPreviewer
//
////////////////////////////////////////////////////////////////////////////////

BrowserPreviewer::BrowserPreviewer (
    const BrowserParts      & parts,
    const BrowserSelection  & selection,
    BrowserSearchMatches    & searchMatches,
    const BrowserArchives   & archives,
    IBrowserPreviewHost     & host)
    : m_parts         (parts),
      m_selection     (selection),
      m_host          (host),
      m_searchMatches (searchMatches),
      m_archives      (archives)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::UpdatePreview
//
//  One selected file in an image previews its contents; one selected disk
//  image in a host folder previews its catalog. Anything else, including a
//  multiple selection, previews nothing.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::UpdatePreview()
{
    const FileEntry         * entry     = nullptr;
    Location                  location  = GetLocation();
    std::wstring              imagePath;
    std::wstring              folder    = location.path;



    m_preview = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    if (m_selection.TryGetSelectedEntry (entry))
    {
        if (entry->isDirectory)
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"A folder. Open it to see what it contains.";
            return;
        }

        if (m_parts.list.isAppleDrive)
        {
            m_cassoLink.PreviewAppleEntry (BrowserRowPaths::GetEntryPath (GetLocation(), *entry), *entry);
        }
        else
        {
            m_diskImages.PreviewEntry (location.path, BrowserRowPaths::GetEntryPath (GetLocation(), *entry), *entry, m_parts.list.kind);
        }

        return;
    }

    //  CassoLink's own items, and a linked Apple's drive, which previews as
    //  its catalog once it is read.
    if (location.kind == Location::Kind::Root && m_parts.list.selectedRows.size() == 1 && m_parts.list.selectedRows[0] >= 0 && (size_t) m_parts.list.selectedRows[0] < m_parts.list.rows.size())
    {
        const CatalogRow &  row = m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]];

        if (row.cassoLinkItem != CatalogRow::CassoLinkItem::None)
        {
            m_cassoLink.PreviewCassoLinkItem (row);
            return;
        }

        if (m_cassoLink.PreviewAppleDrive (row))
        {
            return;
        }
    }

    //  A search's match inside a disk image previews as it does in its image.
    if (m_parts.list.selectedRows.size() == 1 && m_parts.list.selectedRows[0] >= 0 && (size_t) m_parts.list.selectedRows[0] < m_parts.list.rows.size() && !m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]].imagePath.empty())
    {
        PreviewSearchMatch (m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]]);
        return;
    }

    //  A search's result on the disk previews as it does in its own folder.
    if (!m_parts.list.isImage && m_parts.list.selectedRows.size() == 1 && SearchQuery::IsId (location.path) && !m_parts.list.rows[m_parts.list.selectedRows[0]].folderPath.empty())
    {
        folder = m_parts.list.rows[m_parts.list.selectedRows[0]].folderPath;
    }

    //  A file on Home with a file on the disk previews as it does in its folder.
    else if (!m_parts.list.isImage && m_host.IsHomeLocation() && m_parts.list.selectedRows.size() == 1 && !m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        folder = BrowserRowPaths::GetParentFolder (m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath);
    }
    else if (!m_parts.list.isImage && m_host.IsHomeLocation() && m_parts.list.selectedRows.size() == 1 && m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        //  A cloud item has no file here, and its stream is a web page.
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"No preview available.";
        return;
    }
    else if (!m_parts.list.isImage && (location.kind == Location::Kind::ShellFolder || m_host.IsHomeLocation()) && m_parts.list.selectedRows.size() == 1)
    {
        m_shellItems.PreviewShellItem (m_parts.list.rows[m_parts.list.selectedRows[0]]);
        return;
    }
    else if (m_parts.list.isPartitionList && m_parts.list.selectedRows.size() == 1 && !m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath.empty())
    {
        //  A partition of a card or a card image previews as its volume's catalog.
        m_diskImages.PreviewImageCatalog (m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath);
        return;
    }
    else if (!m_parts.list.isImage && location == Location::MakeRoot (TreeModel::kCassoRootId) && m_parts.list.selectedRows.size() == 1
             && !m_parts.list.rows[m_parts.list.selectedRows[0]].isDirectory && !m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        //  An image in the Casso node's list previews as it does in its folder.
        folder = BrowserRowPaths::GetParentFolder (m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath);
    }
    else if (m_parts.list.isImage || location.kind != Location::Kind::HostFolder || m_parts.list.selectedRows.size() != 1)
    {
        return;
    }

    //  An archive previews as what it holds, as a disk image does.
    if (m_parts.list.rows[m_parts.list.selectedRows[0]].isArchive)
    {
        m_shellItems.PreviewArchive (BrowserRowPaths::JoinPath (folder, m_parts.list.rows[m_parts.list.selectedRows[0]].name));
        return;
    }

    //  Any other file in a host folder.
    if (!m_parts.list.rows[m_parts.list.selectedRows[0]].isDiskImage)
    {
        m_hostFiles.PreviewHostFile (folder, m_parts.list.rows[m_parts.list.selectedRows[0]]);
        return;
    }

    imagePath = folder;

    if (!imagePath.empty() && imagePath.back() != L'\\')
    {
        imagePath += L'\\';
    }

    imagePath += m_parts.list.rows[m_parts.list.selectedRows[0]].name;

    m_diskImages.PreviewImageCatalog (imagePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewSearchMatch (const CatalogRow & row)
{
    BrowserSearchMatch  match;
    std::wstring        error;



    if (!m_searchMatches.TryFindSearchMatch (row, match, error))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = error;
        return;
    }

    if (match.entry.isDirectory)
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"A folder. Open it to see what it contains.";
        return;
    }

    m_diskImages.PreviewEntry (match.imagePath, match.directory.empty() ? match.entry.name : match.directory + "/" + match.entry.name, match.entry, match.kind);
}





