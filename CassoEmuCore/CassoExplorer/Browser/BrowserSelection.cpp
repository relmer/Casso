#include "Pch.h"

#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::BrowserSelection
//
////////////////////////////////////////////////////////////////////////////////

BrowserSelection::BrowserSelection (
    BrowserModel             & model,
    std::vector<CatalogRow>  & rows,
    std::vector<int>         & selectedRows,
    IBrowserSelectionSource  & source)
    : m_model        (model),
      m_rows         (rows),
      m_selectedRows (selectedRows),
      m_source       (source)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::SetSelectedRows
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::SetSelectedRows (const std::vector<int> & rows)
{
    std::vector<std::wstring>  names;
    std::set<std::wstring>     items;



    m_selectedRows.clear();

    //  An item that several groups list is acted on once, whichever of its
    //  rows are selected.
    for (int row : rows)
    {
        if (row >= 0 && row < (int) m_rows.size())
        {
            if (!items.insert (m_rows[(size_t) row].shellId + L"|" + m_rows[(size_t) row].hostPath + L"|" + m_rows[(size_t) row].folderPath + L"|" + m_rows[(size_t) row].name).second)
            {
                continue;
            }

            m_selectedRows.push_back (row);
            names.push_back (GetSelectionKey ((size_t) row));
        }
    }

    if (m_model.HasTabs())
    {
        m_model.SetSelection (names);
    }

    m_source.UpdatePreviewAndStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::SelectRowsByKeys
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::SelectRowsByKeys (const std::vector<std::wstring> & keys)
{
    std::unordered_set<std::wstring>  wanted (keys.begin(), keys.end());
    std::vector<std::wstring>         rowKeys;
    std::vector<int>                  rows;
    size_t                            row = 0;



    GetRowKeys (rowKeys);

    for (row = 0; row < rowKeys.size(); row++)
    {
        if (wanted.find (rowKeys[row]) != wanted.end())
        {
            rows.push_back ((int) row);
        }
    }

    SetSelectedRows (rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::GetRowKeys
//
//  ONE PASS, NOT ONE PER ROW. GetSelectionKey counts a name's earlier
//  occurrences by walking every row, so asking it for each row in turn walked
//  the listing once per file, and a folder of a few thousand cost millions of
//  comparisons. The occurrences are counted as the rows go by instead.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::GetRowKeys (std::vector<std::wstring> & outKeys) const
{
    std::unordered_map<std::wstring, size_t>  seen;



    outKeys.clear();
    outKeys.reserve (m_rows.size());

    for (const CatalogRow & row : m_rows)
    {
        if (row.hostPath.starts_with (TreeModel::kLinkRootTag))
        {
            outKeys.push_back (row.hostPath);
            continue;
        }

        size_t  occurrence = seen[row.name]++;

        outKeys.push_back (std::to_wstring (occurrence) + L":" + row.name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::GetSelectionKey
//
//  The occurrence counts rows of the same name that come before this one in
//  the listing rather than in the sorted view, so the key stays the same
//  across a re-sort and a reload. GetRowKeys must build the same keys.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserSelection::GetSelectionKey (size_t row) const
{
    size_t  occurrence = 0;



    //  A linked machine's or drive's row is renamed once its disk is read,
    //  so it is found again by its id.
    if (m_rows[row].hostPath.starts_with (TreeModel::kLinkRootTag))
    {
        return m_rows[row].hostPath;
    }

    for (const CatalogRow & other : m_rows)
    {
        occurrence += (other.name == m_rows[row].name && other.sourceIndex < m_rows[row].sourceIndex) ? 1 : 0;
    }

    return std::to_wstring (occurrence) + L":" + m_rows[row].name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::SelectRowAt
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::SelectRowAt (int row)
{
    if (row >= 0 && !m_rows.empty())
    {
        SetSelectedRows ({ (std::min) (row, (int) m_rows.size() - 1) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::TryGetSelectedEntry
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::TryGetSelectedEntry (const FileEntry *& outEntry) const
{
    size_t  source = 0;



    outEntry = nullptr;

    if (!m_source.IsVolumeLocation() || m_selectedRows.size() != 1)
    {
        return false;
    }

    source = m_rows[m_selectedRows[0]].sourceIndex;

    if (source >= m_source.GetListing().entries.size())
    {
        return false;
    }

    outEntry = &m_source.GetListing().entries[source];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::GetSelectedEntries
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::GetSelectedEntries (std::vector<FileEntry> & outEntries) const
{
    outEntries.clear();

    if (!m_source.IsVolumeLocation())
    {
        return;
    }

    for (int row : m_selectedRows)
    {
        size_t  source = m_rows[row].sourceIndex;

        if (source < m_source.GetListing().entries.size())
        {
            outEntries.push_back (m_source.GetListing().entries[source]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::IsCassoNodeLocation
//
//  The Casso node, or its stock or demo disks.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::IsCassoNodeLocation (const Location & location)
{
    return location.kind == Location::Kind::Root
        && (location.path == TreeModel::kCassoRootId || location.path == TreeModel::kStockDisksId || location.path == TreeModel::kDemoDisksId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::GetSelectedHostPaths
//
////////////////////////////////////////////////////////////////////////////////

void BrowserSelection::GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const
{
    Location  location = m_source.GetLocation();



    outPaths.clear();

    //  Home's, a search's and the Casso node's items are anywhere on the
    //  disk, and a cloud item isn't on it. A stock disk's path is where it is
    //  downloaded to, so it is downloaded before the path is used.
    if (m_source.IsHomeLocation() || AreSelectedRowsSearchFiles() || IsCassoNodeLocation (location))
    {
        for (int row : m_selectedRows)
        {
            if (!m_rows[row].hostPath.empty())
            {
                outPaths.push_back (m_rows[row].hostPath);
            }
        }

        return;
    }

    if (m_source.IsImageLocation() || location.kind != Location::Kind::HostFolder)
    {
        return;
    }

    for (int row : m_selectedRows)
    {
        outPaths.push_back (BrowserRowPaths::JoinPath (location.path, m_rows[row].name));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::TryGetSelectedShellIds
//
//  The selection by the shell's names for it, when every selected row is a
//  host file or an item the shell lists; false when any is inside a disk
//  image.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::TryGetSelectedShellIds (std::vector<std::wstring> & outIds) const
{
    const std::vector<CatalogRow> &  rows = m_rows;



    outIds.clear();

    if (m_source.GetLocation().kind == Location::Kind::HostFolder || IsCassoNodeLocation (m_source.GetLocation()))
    {
        GetSelectedHostPaths (outIds);
        return !outIds.empty();
    }

    //  CassoLink's items drag as the files they are written to.
    if (m_source.TryGetSelectedCassoLinkPaths (outIds))
    {
        return true;
    }

    if (!HasShellRows (m_source.GetLocation().kind, m_source.IsHomeLocation()))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (row < 0 || (size_t) row >= rows.size() || !rows[(size_t) row].imagePath.empty())
        {
            outIds.clear();
            return false;
        }

        outIds.push_back (rows[(size_t) row].hostPath.empty() ? rows[(size_t) row].shellId : rows[(size_t) row].hostPath);
    }

    return !outIds.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::AreSelectedRowsCassoFiles
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::AreSelectedRowsCassoFiles() const
{
    if (m_selectedRows.empty() || !IsCassoNodeLocation (m_source.GetLocation()))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (row < 0 || (size_t) row >= m_rows.size() || m_rows[(size_t) row].hostPath.empty() || !m_rows[(size_t) row].imagePath.empty() || m_rows[(size_t) row].isStockDisk)
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::AreSelectedRowsSearchFiles
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::AreSelectedRowsSearchFiles() const
{
    if (m_selectedRows.empty() || !SearchQuery::IsId (m_source.GetLocation().path))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (row < 0 || (size_t) row >= m_rows.size() || m_rows[(size_t) row].hostPath.empty() || !m_rows[(size_t) row].imagePath.empty())
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection::AreSelectedRowsImages
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserSelection::AreSelectedRowsImages() const
{
    bool  search = AreSelectedRowsSearchFiles();



    if (m_source.IsImageLocation() || m_selectedRows.empty() || (m_source.GetLocation().kind != Location::Kind::HostFolder && !search))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (!m_rows[row].isDiskImage || (search && !m_rows[row].containerPath.empty()))
        {
            return false;
        }
    }

    return true;
}





