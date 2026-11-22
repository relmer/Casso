#include "Pch.h"

#include "CassoExplorer/Browser/BrowserRowOrder.h"
#include "CassoExplorer/Browser/BrowserColumnFilters.h"
#include "CassoExplorer/Browser/IBrowserListing.h"
#include "CassoExplorer/Model/BrowserModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::BrowserRowOrder
//
////////////////////////////////////////////////////////////////////////////////

BrowserRowOrder::BrowserRowOrder (
    BrowserModel             & model,
    std::vector<CatalogRow>  & rows,
    std::vector<int>         & selectedRows,
    BrowserColumnFilters     & columnFilters,
    IBrowserListing          & listing)
    : m_model         (model),
      m_rows          (rows),
      m_selectedRows  (selectedRows),
      m_columnFilters (columnFilters),
      m_listing       (listing)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::SortRows
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::SortRows()
{
    if (!m_model.HasTabs())
    {
        return;
    }

    //  Grouped, Home is a list of its items, each once, as File Explorer's is.
    if (m_listing.IsHomeLocation() && !IsHomeSectioned())
    {
        std::set<std::wstring>  seen;

        m_rows.clear();

        for (const CatalogRow & row : m_homeRows)
        {
            if (seen.insert (row.shellId.empty() ? row.hostPath : row.shellId).second)
            {
                m_rows.push_back (row);
                m_rows.back().homeInList = true;
            }
        }

        CatalogModel::Sort (m_rows, m_model.GetActiveTab().sortColumn, m_model.GetActiveTab().sortDescending);
        GroupRows();

        return;
    }

    //  Home keeps the shell's order: Quick access, then the tab's items.
    if (m_listing.IsHomeLocation())
    {
        static constexpr int  s_kTabGroups[] = { 3, 2, 4 };

        m_rows.clear();
        m_rowGroups.clear();

        for (const CatalogRow & row : m_homeRows)
        {
            if (row.homeGroup == 1)
            {
                m_rows.push_back (row);
            }
        }

        //  The pinned folders keep their order and the frequent ones follow by
        //  name; Shared is by name; the others are as the shell has them.
        std::stable_sort (m_rows.begin(), m_rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
        {
            return (a.homePinned != b.homePinned) ? a.homePinned : (!a.homePinned && _wcsicmp (a.name.c_str(), b.name.c_str()) < 0);
        });

        m_homeFirstTabRow = (int) m_rows.size();

        for (const CatalogRow & row : m_homeRows)
        {
            if (row.homeGroup == s_kTabGroups[m_homeTab])
            {
                m_rows.push_back (row);
            }
        }

        if (s_kTabGroups[m_homeTab] == 4)
        {
            std::stable_sort (m_rows.begin() + m_homeFirstTabRow, m_rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
            {
                return _wcsicmp (a.name.c_str(), b.name.c_str()) < 0;
            });
        }

        return;
    }

    m_columnFilters.FilterRows (m_rows, m_model.GetActiveTab().location);

    if (m_model.GetActiveTab().sortColumn != CatalogModel::kRelevance && (size_t) m_model.GetActiveTab().sortColumn >= CatalogModel::kColumnCount && m_propertyKey)
    {
        std::vector<CatalogModel::PropertyKey>  keys;

        for (size_t row = 0; row < m_rows.size(); row++)
        {
            keys.push_back ((m_propertyPeek ? m_propertyPeek : m_propertyKey) (row, (size_t) m_model.GetActiveTab().sortColumn - CatalogModel::kColumnCount));
        }

        CatalogModel::SortByProperty (m_rows, keys, m_model.GetActiveTab().sortDescending);
    }
    else
    {
        CatalogModel::Sort (m_rows, m_model.GetActiveTab().sortColumn, m_model.GetActiveTab().sortDescending);
    }

    GroupRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::GroupRows
//
//  Grouping orders the groups and keeps the sort within each: a stable sort
//  by group over rows already sorted by the column.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::GroupRows()
{
    struct Entry
    {
        size_t              row = 0;
        RowGrouping::Group  group;
    };

    RowGrouping::Field               field      = m_model.GetActiveTab().groupBy;
    bool                             descending = m_model.GetActiveTab().groupDescending;
    RowGrouping::Today               today      = RowGrouping::GetToday();
    bool                             property   = RowGrouping::IsPropertyField (field);
    std::vector<Entry>               entries;
    std::vector<CatalogRow>          sorted;
    std::vector<RowGrouping::Group>  sortedGroups;
    std::set<size_t>                 listed;



    m_rowGroups.clear();
    std::erase_if (m_rows, [] (const CatalogRow & row) { return row.isGroupCopy; });

    if (field == RowGrouping::Field::None || (property && (RowGrouping::GetPropertyIndex (field) >= m_propertyCount || !m_propertyKey)))
    {
        return;
    }

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        if (!property)
        {
            entries.push_back ({ i, RowGrouping::GetGroup (m_rows[i], field, today) });
            continue;
        }

        for (RowGrouping::Group & group : RowGrouping::GetPropertyGroups ((m_propertyPeek ? m_propertyPeek : m_propertyKey) (i, RowGrouping::GetPropertyIndex (field)), today))
        {
            entries.push_back ({ i, std::move (group) });
        }
    }

    std::stable_sort (entries.begin(), entries.end(), [descending] (const Entry & a, const Entry & b)
    {
        return RowGrouping::IsBefore (a.group, b.group, descending);
    });

    for (Entry & entry : entries)
    {
        sorted.push_back (m_rows[entry.row]);
        sorted.back().isGroupCopy = !listed.insert (entry.row).second;
        sortedGroups.push_back (std::move (entry.group));
    }

    m_rows      = std::move (sorted);
    m_rowGroups = std::move (sortedGroups);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::IsHomeSectioned
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserRowOrder::IsHomeSectioned() const
{
    return m_listing.IsHomeLocation() && (!m_model.HasTabs() || m_model.GetActiveTab().groupBy == RowGrouping::Field::None);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::RemoveHomeRow
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::RemoveHomeRow (const std::wstring & shellId)
{
    std::erase_if (m_homeRows, [&shellId] (const CatalogRow & row) { return row.shellId == shellId || row.hostPath == shellId; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::GetListGroups
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Group> BrowserRowOrder::GetListGroups() const
{
    std::vector<DxuiListView::Group>  out;



    if (IsHomeSectioned())
    {
        out.push_back ({ L"Quick access", 0 });
        out.push_back ({ L"Home tabs", m_homeFirstTabRow });

        return out;
    }

    for (size_t i = 0; i < m_rowGroups.size(); i++)
    {
        if (i == 0 || !(m_rowGroups[i] == m_rowGroups[i - 1]))
        {
            out.push_back ({ m_rowGroups[i].label, (int) i });
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::SetPropertyColumns
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::SetPropertyColumns (size_t count, PropertyKeyFn keyFn, PropertyKeyFn peekFn)
{
    m_propertyCount = count;
    m_propertyKey   = std::move (keyFn);
    m_propertyPeek  = std::move (peekFn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::IsPropertyColumn
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserRowOrder::IsPropertyColumn (int column) const
{
    return column >= (int) CatalogModel::kColumnCount && (size_t) column < CatalogModel::kColumnCount + m_propertyCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::SetGroupBy
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::SetGroupBy (RowGrouping::Field field, bool descending)
{
    if (!m_model.HasTabs())
    {
        return;
    }

    m_model.SetGroup (field, descending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::SetSortAndGroup
//
//  A folder's sort and grouping, set together as it opens.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::SetSortAndGroup (CatalogModel::Column column, bool descending, RowGrouping::Field group, bool groupDescending)
{
    const BrowserModel::Tab *  tab = m_model.HasTabs() ? &m_model.GetActiveTab() : nullptr;



    if (tab == nullptr ||
        (tab->sortColumn == column && tab->sortDescending == descending && tab->groupBy == group && tab->groupDescending == groupDescending))
    {
        return;
    }

    m_model.SetSort  (column, descending);
    m_model.SetGroup (group, groupDescending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::SortByColumn
//
//  The same column again reverses the order; a new column starts ascending.
//  The selection follows its rows through the reorder.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::SortByColumn (int column)
{
    CatalogModel::Column  requested       = (CatalogModel::Column) column;
    bool                  descending      = false;



    if (!m_model.HasTabs() || column < 0 || (column >= (int) CatalogModel::kColumnCount && requested != CatalogModel::kRelevance && !IsPropertyColumn (column)))
    {
        return;
    }

    descending = m_model.GetActiveTab().sortColumn == requested && !m_model.GetActiveTab().sortDescending;

    m_model.SetSort (requested, descending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder::ResortKeepingSelection
//
//  The selection follows its rows through the reorder.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserRowOrder::ResortKeepingSelection()
{
    std::vector<size_t>   selectedSources;



    for (int row : m_selectedRows)
    {
        selectedSources.push_back (m_rows[row].sourceIndex);
    }

    SortRows();

    m_selectedRows.clear();

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        if (std::find (selectedSources.begin(), selectedSources.end(), m_rows[row].sourceIndex) != selectedSources.end())
        {
            m_selectedRows.push_back ((int) row);
        }
    }
}





