#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"
#include "CassoExplorer/Model/RowGrouping.h"

struct CatalogRow;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFilters
//
//  File Explorer's column filters: the groups a column's rows fall in, as
//  Group by groups them, and the rows left in the groups checked. The
//  choices are the folder's before filtering; a filter ends when the folder
//  changes.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserColumnFilters
{
public:
    static RowGrouping::Field                 GetFilterField   (size_t column);
    const std::vector<RowGrouping::Group>   & GetFilterChoices (RowGrouping::Field field) const;
    bool                                      IsFiltering      (RowGrouping::Field field) const { return m_filters.contains (field); }
    bool                                      IsFilterChecked  (RowGrouping::Field field, const std::wstring & label) const;
    void                                      ToggleFilter     (RowGrouping::Field field, const std::wstring & label);
    void                                      ClearFilter      (RowGrouping::Field field);

    //  The rows are loaded again, so the next FilterRows reads their choices.
    void  MarkRowsLoaded ()                                                           { m_rowsUnfiltered = true; }
    void  FilterRows     (std::vector<CatalogRow> & rows, const Location & location);

private:
    std::map<RowGrouping::Field, std::set<std::wstring>>           m_filters;                  // the checked groups of each filtered column
    std::map<RowGrouping::Field, std::vector<RowGrouping::Group>>  m_filterChoices;            // each column's groups before filtering
    Location                                                        m_filterLocation;           // the folder the filters are for
    bool                                                            m_rowsUnfiltered = false;   // loaded since the last filter
};
