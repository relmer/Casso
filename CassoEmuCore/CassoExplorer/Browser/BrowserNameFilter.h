#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/Location.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter
//
//  A picker's name filter: the rows are those whose file name contains every
//  word of the text. An empty text lists every row the location lists.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserNameFilter
{
public:
    //  Whether the location's rows are filtered by name, and how many rows the
    //  location lists with no filter.
    void  CaptureSource (const Location & location, bool hasScope, size_t rowCount);
    void  SetSource     (std::vector<CatalogRow> rows)                              { m_source = std::move (rows); }
    void  Apply         (std::vector<CatalogRow> & outRows) const;

    //  True when the text changed and the rows are filtered by name.
    bool                  SetText        (const std::wstring & text);
    const std::wstring  & GetText        () const                                   { return m_text; }
    bool                  IsFilterable   () const                                   { return m_isFilterable; }

    //  The words of a name filter, lowercase, and whether a name has them all.
    static std::vector<std::wstring>  SplitFilterWords  (const std::wstring & text);
    static bool                       IsNameFilterMatch (const std::wstring & name, const std::vector<std::wstring> & words);

private:
    std::wstring             m_text;
    std::vector<CatalogRow>  m_source;                  // a picker's rows before the name filter
    size_t                   m_sourceBase   = 0;        // the rows of m_source the location lists with no filter
    bool                     m_isFilterable = false;
};
