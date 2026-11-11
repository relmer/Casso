#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CollectionQuery.h"

struct DxuiPopupMenuItem;
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools
//
//  Disk list and Find duplicates on a folder, from its row's context menu
//  or the background of the open folder: each opens its results in the
//  tab, as a search does. The results' command bar has Save as CSV, which
//  writes the text `casso disk list` and `casso disk dupes` write.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerCollectionTools
{
public:
    //  The folder row selected, or the open host folder; empty for any
    //  other selection or location.
    static std::wstring  GetFolder (const ExplorerContext & context);

    static void  AddMenuRows (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items);
    static void  Open        (ExplorerContext & context, CollectionQuery::Tool tool);
    static bool  CanSaveCsv  (const ExplorerContext & context);
    static void  SaveCsv     (ExplorerContext & context);

private:
    static constexpr GUID  s_kSaveCsvGuid = { 0x8c2d4a71, 0x6b3e, 0x4d90, { 0xa5, 0x1f, 0x37, 0xe2, 0x9b, 0x04, 0x6c, 0xd8 } };
};
