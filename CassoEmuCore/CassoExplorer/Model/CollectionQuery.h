#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionQuery
//
//  Disk list or Find duplicates over a folder, as a location: the id of a
//  shell folder, as a search's is, so the list, the tabs and the history
//  treat the results as they treat any folder's.
//
//  As text, the id is kPrefix, the tool's word, a '|' and the folder:
//  `casso-results:disks|C:\Disks`.
//
////////////////////////////////////////////////////////////////////////////////

class CollectionQuery
{
public:
    enum class Tool { DiskList, Duplicates };

    static constexpr const wchar_t *  kPrefix = L"casso-results:";

    static std::wstring  MakeId         (Tool tool, const std::wstring & folder);
    static bool          TryParseId     (const std::wstring & id, Tool & outTool, std::wstring & outFolder);
    static bool          IsId           (const std::wstring & id) { return id.starts_with (kPrefix); }
    static bool          IsDuplicatesId (const std::wstring & id);

    //  The text of the address bar and the tab.
    static std::wstring  GetLabel   (Tool tool, const std::wstring & folder);

private:
    static constexpr const wchar_t *  s_kDiskListWord   = L"disks";
    static constexpr const wchar_t *  s_kDuplicatesWord = L"dupes";
};
