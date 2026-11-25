#include "Pch.h"

#include "CassoExplorer/Browser/BrowserSearchMatches.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatches::BrowserSearchMatches
//
////////////////////////////////////////////////////////////////////////////////

BrowserSearchMatches::BrowserSearchMatches (const BrowserParts & parts)
    : m_parts (parts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatches::TryFindSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed. False, with why, when the image cannot be read or
//  no longer has it.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSearchMatches::TryFindSearchMatch (const CatalogRow & row, BrowserSearchMatch & outMatch, std::wstring & outError)
{
    DiskOperations::Result  result;
    VolumeListing           listing;
    VolumeKind              kind  = VolumeKind::Unknown;
    bool                    found = false;



    if (row.imagePath.empty())
    {
        return false;
    }

    //  The catalog of the volume itself is cached; a directory inside it is read.
    if (!row.imageDirectory.empty() || !m_parts.model.TryGetCachedCatalog (row.imagePath, listing, kind))
    {
        result = m_parts.operations.List (TextEncoding::WideToNarrow (row.imagePath), row.imageDirectory, listing, kind);

        if (!result.Succeeded())
        {
            outError = DiskMessageText::FormatImageError (row.imagePath, result.message);
            return false;
        }

        if (row.imageDirectory.empty())
        {
            m_parts.model.CacheCatalog (row.imagePath, listing, kind);
        }
    }

    for (const FileEntry & entry : listing.entries)
    {
        if (!found && entry.catalogIndex == row.imageCatalogIndex)
        {
            outMatch.imagePath = row.imagePath;
            outMatch.kind      = kind;
            outMatch.directory = row.imageDirectory;
            outMatch.entry     = entry;
            found              = true;
        }
    }

    if (!found)
    {
        outError = L"This file is no longer in its disk image.";
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatches::GetSelectedSearchMatches
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSearchMatches::GetSelectedSearchMatches (std::vector<BrowserSearchMatch> & outMatches)
{
    BrowserSearchMatch  match;
    std::wstring        error;



    outMatches.clear();

    for (int row : m_parts.list.selectedRows)
    {
        if (row >= 0 && (size_t) row < m_parts.list.rows.size() && TryFindSearchMatch (m_parts.list.rows[(size_t) row], match, error))
        {
            outMatches.push_back (match);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatches::AreSelectedRowsSearchMatches
//
//  Whether a selection is files a search found inside disk images, which
//  copy from their images.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSearchMatches::AreSelectedRowsSearchMatches() const
{
    bool  any = false;



    for (int row : m_parts.list.selectedRows)
    {
        any = any || (row >= 0 && (size_t) row < m_parts.list.rows.size() && !m_parts.list.rows[(size_t) row].imagePath.empty());
    }

    return any;
}
