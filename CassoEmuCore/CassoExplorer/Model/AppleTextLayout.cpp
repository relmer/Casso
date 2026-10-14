#include "Pch.h"

#include "CassoExplorer/Model/AppleTextLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextLayout::Wrap
//
//  Fewer than one column is taken as one, so every character has a row.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> AppleTextLayout::Wrap (const std::vector<std::wstring> & lines, int columns)
{
    std::vector<std::wstring>  rows;
    size_t                     width = (size_t) (std::max) (columns, 1);



    for (const std::wstring & line : lines)
    {
        if (line.empty())
        {
            rows.emplace_back();
        }

        for (size_t at = 0; at < line.size(); at += width)
        {
            rows.push_back (line.substr (at, width));
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextLayout::MakeScreenRows
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> AppleTextLayout::MakeScreenRows (const std::vector<std::wstring> & lines, int columns)
{
    std::vector<std::wstring>  rows (1);



    for (const std::wstring & row : Wrap (lines, columns - 2))
    {
        rows.push_back (L" " + row);
    }

    rows.emplace_back();

    return rows;
}