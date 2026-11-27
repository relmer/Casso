#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/RowGrouping.h"

class BrowserColumnFilters;
class BrowserModel;
class IBrowserListing;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrder
//
//  The order of the list's rows: the active tab's sort and Group by, the
//  groups the list draws headers for, the shell property columns a sort can
//  use, and Home's sections and tabs.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserRowOrder
{
public:
    //  The function that returns a row's key for sorting by a shell property
    //  column after the fixed ones.
    using PropertyKeyFn = std::function<CatalogModel::PropertyKey (size_t row, size_t property)>;

    BrowserRowOrder (BrowserModel            & model,
                     std::vector<CatalogRow> & rows,
                     std::vector<int>        & selectedRows,
                     BrowserColumnFilters    & columnFilters,
                     IBrowserListing         & listing);

    //  Orders the rows by the active tab's sort and grouping.
    void  SortRows ();

    //  The same column again reverses the order; a new column starts
    //  ascending. The selection follows its rows through the reorder.
    void  SortByColumn    (int column);
    void  SetGroupBy      (RowGrouping::Field field, bool descending);
    void  SetSortAndGroup (CatalogModel::Column column, bool descending, RowGrouping::Field group, bool groupDescending);

    std::vector<DxuiListView::Group>  GetListGroups () const;

    void  SetPropertyColumns (size_t count, PropertyKeyFn keyFn, PropertyKeyFn peekFn = nullptr);
    bool  IsPropertyColumn   (int column) const;

    //  Home, listed in its sections: Quick access, then one of its tabs.
    bool  IsHomeSectioned () const;
    int   GetHomeTab      () const                                        { return m_homeTab; }
    void  SetHomeTab      (int tab)                                       { m_homeTab = tab; }

    //  All of Home's items, from which its rows are listed.
    void  SetHomeRows     (const std::vector<CatalogRow> & rows)          { m_homeRows = rows; }
    bool  HasHomeRows     () const                                        { return !m_homeRows.empty(); }
    void  RemoveHomeRow   (const std::wstring & shellId);

private:
    void  GroupRows              ();
    void  ResortKeepingSelection ();

    BrowserModel                     & m_model;
    std::vector<CatalogRow>          & m_rows;
    std::vector<int>                 & m_selectedRows;
    BrowserColumnFilters             & m_columnFilters;
    IBrowserListing                  & m_listing;
    std::vector<RowGrouping::Group>    m_rowGroups;            // each row's group, when grouped
    size_t                             m_propertyCount   = 0;
    PropertyKeyFn                      m_propertyKey;
    PropertyKeyFn                      m_propertyPeek;         // a value already read, or none, without waiting for the file
    std::vector<CatalogRow>            m_homeRows;             // all of Home's items; the rows are Quick access and one tab's
    int                                m_homeTab         = 0;
    int                                m_homeFirstTabRow = 0;
};
