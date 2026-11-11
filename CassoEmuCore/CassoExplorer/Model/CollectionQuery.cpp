#include "Pch.h"

#include "CassoExplorer/Model/CollectionQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionQuery::MakeId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CollectionQuery::MakeId (Tool tool, const std::wstring & folder)
{
    return std::wstring (kPrefix) + (tool == Tool::DiskList ? s_kDiskListWord : s_kDuplicatesWord) + L"|" + folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionQuery::TryParseId
//
////////////////////////////////////////////////////////////////////////////////

bool CollectionQuery::TryParseId (const std::wstring & id, Tool & outTool, std::wstring & outFolder)
{
    size_t        bar  = id.find (L'|');
    std::wstring  word;



    outTool = Tool::DiskList;
    outFolder.clear();

    if (!IsId (id) || bar == std::wstring::npos)
    {
        return false;
    }

    word      = id.substr (wcslen (kPrefix), bar - wcslen (kPrefix));
    outFolder = id.substr (bar + 1);

    if (word == s_kDuplicatesWord)
    {
        outTool = Tool::Duplicates;
    }
    else if (word != s_kDiskListWord)
    {
        return false;
    }

    return !outFolder.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionQuery::IsDuplicatesId
//
////////////////////////////////////////////////////////////////////////////////

bool CollectionQuery::IsDuplicatesId (const std::wstring & id)
{
    Tool          tool = Tool::DiskList;
    std::wstring  folder;



    return TryParseId (id, tool, folder) && tool == Tool::Duplicates;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionQuery::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CollectionQuery::GetLabel (Tool tool, const std::wstring & folder)
{
    std::wstring  trimmed = folder;
    size_t        slash   = 0;



    while (trimmed.size() > 3 && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
    {
        trimmed.pop_back();
    }

    slash   = trimmed.find_last_of (L"\\/");
    trimmed = (slash == std::wstring::npos || slash + 1 == trimmed.size()) ? trimmed : trimmed.substr (slash + 1);

    return (tool == Tool::DiskList ? L"Disk list of " : L"Duplicates in ") + trimmed;
}
