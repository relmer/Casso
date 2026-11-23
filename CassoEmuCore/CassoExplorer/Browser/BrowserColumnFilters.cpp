#include "Pch.h"

#include "CassoExplorer/Browser/BrowserColumnFilters.h"
#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::GetFilterField
//
//  The columns File Explorer filters by its groups: name, date, type and
//  size.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Field BrowserColumnFilters::GetFilterField (size_t column)
{
    switch ((CatalogModel::Column) column)
    {
        case CatalogModel::Column::Name:     return RowGrouping::Field::Name;
        case CatalogModel::Column::Modified: return RowGrouping::Field::DateModified;
        case CatalogModel::Column::Type:     return RowGrouping::Field::Type;
        case CatalogModel::Column::Size:     return RowGrouping::Field::Size;
        default:                             return RowGrouping::Field::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::GetFilterChoices
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<RowGrouping::Group> & BrowserColumnFilters::GetFilterChoices (RowGrouping::Field field) const
{
    static const std::vector<RowGrouping::Group>  s_kNone;
    auto                                          found  = m_filterChoices.find (field);



    return (found != m_filterChoices.end()) ? found->second : s_kNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::IsFilterChecked
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserColumnFilters::IsFilterChecked (RowGrouping::Field field, const std::wstring & label) const
{
    auto  found = m_filters.find (field);



    return found != m_filters.end() && found->second.contains (label);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::ToggleFilter
//
//  The last group unchecked ends the column's filter.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserColumnFilters::ToggleFilter (RowGrouping::Field field, const std::wstring & label)
{
    std::set<std::wstring> &  checked = m_filters[field];



    if (!checked.erase (label))
    {
        checked.insert (label);
    }

    if (checked.empty())
    {
        m_filters.erase (field);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::ClearFilter
//
////////////////////////////////////////////////////////////////////////////////

void BrowserColumnFilters::ClearFilter (RowGrouping::Field field)
{
    m_filters.erase (field);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters::FilterRows
//
//  The choices come from every row the folder has; the rows kept are those
//  in a checked group of every filtered column. Rows are filtered once each
//  load, so a sort afterward sorts the rows the filter left.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserColumnFilters::FilterRows (
    std::vector<CatalogRow>  & rows,
    const Location           & location)
{
    RowGrouping::Today  today = RowGrouping::GetToday();



    if (!m_rowsUnfiltered)
    {
        return;
    }

    m_rowsUnfiltered = false;

    if (!(location == m_filterLocation))
    {
        m_filters.clear();
        m_filterLocation = location;
    }

    m_filterChoices.clear();

    for (RowGrouping::Field field : { RowGrouping::Field::Name, RowGrouping::Field::DateModified, RowGrouping::Field::Type, RowGrouping::Field::Size })
    {
        std::vector<RowGrouping::Group> &  choices = m_filterChoices[field];

        for (const CatalogRow & row : rows)
        {
            RowGrouping::Group  group = RowGrouping::GetGroup (row, field, today);

            if (std::find (choices.begin(), choices.end(), group) == choices.end())
            {
                choices.push_back (group);
            }
        }

        //  File Explorer's filter lists a group of rows with no value, such
        //  as a folder's Unspecified size, after the others.
        std::stable_sort (choices.begin(), choices.end(), [] (const RowGrouping::Group & a, const RowGrouping::Group & b)
        {
            if ((a.rank < 0) != (b.rank < 0))
            {
                return b.rank < 0;
            }

            return RowGrouping::IsBefore (a, b, false);
        });
    }

    if (m_filters.empty())
    {
        return;
    }

    std::erase_if (rows, [this, &today] (const CatalogRow & row)
    {
        for (const auto & [field, checked] : m_filters)
        {
            if (!checked.contains (RowGrouping::GetGroup (row, field, today).label))
            {
                return true;
            }
        }

        return false;
    });
}






