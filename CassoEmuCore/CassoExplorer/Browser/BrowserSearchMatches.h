#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/BrowserTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatches
//
//  The files a search found inside disk images: each row found again in its
//  image's catalog, the rows selected, and whether the selection is such
//  files.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserSearchMatches
{
public:
    explicit BrowserSearchMatches (const BrowserParts & parts);

    bool  TryFindSearchMatch           (const CatalogRow & row, BrowserSearchMatch & outMatch, std::wstring & outError);
    void  GetSelectedSearchMatches     (std::vector<BrowserSearchMatch> & outMatches);
    bool  AreSelectedRowsSearchMatches () const;

private:
    BrowserParts  m_parts;
};
