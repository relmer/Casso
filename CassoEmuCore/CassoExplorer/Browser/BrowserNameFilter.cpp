#include "Pch.h"

#include "CassoExplorer/Browser/BrowserNameFilter.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/TreeModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter::CaptureSource
//
//  A picker filters by name the rows of a folder, the Casso node and its
//  stock and demo disks.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNameFilter::CaptureSource (
    const Location  & location,
    bool              hasScope,
    size_t            rowCount)
{
    bool  isCasso = location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId;
    bool  isStock = location.kind == Location::Kind::Root && (location.path == TreeModel::kStockDisksId || location.path == TreeModel::kDemoDisksId);



    m_source.clear();
    m_sourceBase   = 0;
    m_isFilterable = hasScope && (location.kind == Location::Kind::HostFolder || isCasso || isStock);

    if (!m_isFilterable)
    {
        return;
    }

    m_sourceBase = rowCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BrowserNameFilter::Apply (std::vector<CatalogRow> & outRows) const
{
    std::vector<std::wstring>  words = SplitFilterWords (m_text);



    outRows.clear();

    for (size_t index = 0; index < m_source.size(); index++)
    {
        const CatalogRow &  row = m_source[index];

        if (words.empty() ? index < m_sourceBase : IsNameFilterMatch (row.name, words) || (!row.label.empty() && IsNameFilterMatch (row.label, words)))
        {
            outRows.push_back (row);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter::SetText
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNameFilter::SetText (const std::wstring & text)
{
    if (text == m_text)
    {
        return false;
    }

    m_text = text;

    return m_isFilterable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter::SplitFilterWords
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> BrowserNameFilter::SplitFilterWords (const std::wstring & text)
{
    std::vector<std::wstring>  words;
    std::wstring               word;



    for (wchar_t c : text)
    {
        if (iswspace (c))
        {
            if (!word.empty())
            {
                words.push_back (std::move (word));
                word.clear();
            }

            continue;
        }

        word += (wchar_t) towlower (c);
    }

    if (!word.empty())
    {
        words.push_back (std::move (word));
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilter::IsNameFilterMatch
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserNameFilter::IsNameFilterMatch (const std::wstring & name, const std::vector<std::wstring> & words)
{
    std::wstring  lower;



    for (wchar_t c : name)
    {
        lower += (wchar_t) towlower (c);
    }

    return std::all_of (words.begin(), words.end(), [&lower] (const std::wstring & word) { return lower.find (word) != std::wstring::npos; });
}





