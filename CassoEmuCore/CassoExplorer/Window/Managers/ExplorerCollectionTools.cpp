#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerCollectionTools.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Model/CollectionResultsItems.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Config/IFileSystem.h"
#include "Core/UnicodeSymbols.h"
#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools::GetFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerCollectionTools::GetFolder (const ExplorerContext & context)
{
    const std::vector<int>  & selected = context.browser.GetSelectedRows();
    Location                  location;



    if (selected.size() == 1)
    {
        return (context.browser.TryGetRowLocation (selected[0], location) && location.kind == Location::Kind::HostFolder) ? location.path : std::wstring();
    }

    return (selected.empty() && context.browser.GetLocation().kind == Location::Kind::HostFolder) ? context.browser.GetLocation().path : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools::AddMenuRows
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCollectionTools::AddMenuRows (ExplorerContext & context, std::vector<DxuiPopupMenuItem> & items)
{
    if (GetFolder (context).empty())
    {
        return;
    }

    context.menus.AddMenuCommand (items, L"Dis&k list",       [&context]() { context.commandManager.Dispatch (CassoExplorerCommands::kDiskList); });
    context.menus.AddMenuCommand (items, L"Find d&uplicates", [&context]() { context.commandManager.Dispatch (CassoExplorerCommands::kFindDuplicates); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools::Open
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCollectionTools::Open (ExplorerContext & context, CollectionQuery::Tool tool)
{
    std::wstring  folder = GetFolder (context);



    if (!folder.empty())
    {
        context.browser.NavigateToLocation (Location::MakeShellFolder (CollectionQuery::MakeId (tool, folder), CollectionQuery::GetLabel (tool, folder)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools::CanSaveCsv
//
//  Once the walk has ended, so the file has every row.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCollectionTools::CanSaveCsv (const ExplorerContext & context)
{
    return CollectionQuery::IsId (context.browser.GetLocation().path) && !context.browser.IsSearchRunning() && context.app.fs != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCollectionTools::SaveCsv
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCollectionTools::SaveCsv (ExplorerContext & context)
{
    HRESULT                hr     = S_OK;
    CollectionQuery::Tool  tool   = CollectionQuery::Tool::DiskList;
    std::wstring           folder;
    FileDialogSpec         spec;
    std::filesystem::path  picked;
    bool                   chosen = false;
    bool                   parsed = CollectionQuery::TryParseId (context.browser.GetLocation().path, tool, folder);



    BAIL_OUT_IF (!parsed || !CanSaveCsv (context), S_OK);

    spec.filters.push_back (FileDialogFilter { L"CSV file (*.csv)", L"*.csv" });

    spec.clientGuid       = s_kSaveCsvGuid;
    spec.defaultFileName  = (tool == CollectionQuery::Tool::DiskList) ? L"Disk list.csv" : L"Duplicates.csv";
    spec.defaultExtension = L"csv";
    spec.title            = L"Save as CSV";

    hr = context.dialogs.PickFileToSave (context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    hr = context.app.fs->WriteAllText (picked.wstring(), CollectionResultsItems::FormatCsv (tool, context.browser.GetRows()));
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        context.window.ShowMessage (L"The CSV file could not be saved.", MB_ICONERROR);
    }
}
