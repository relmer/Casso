#include "Pch.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/Managers/ExplorerCollectionTools.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "Machines/Apple2/Common/StockBootDisks.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Seams/Win32UserClasses.h"
#include "Update/Win32InstallEnvironment.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Core/AppTimerIds.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerClipboard.h"
#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/Managers/ExplorerDeletion.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerRename.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/Managers/ExplorerCassoTarget.h"
#include "CassoExplorer/Window/Managers/ExplorerProperties.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32HostDialogs.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/WozPropertiesDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsBlockedInPicker
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsBlockedInPicker (int id) const
{
    const PickerRequest *  picker = m_context.app.picker;



    return picker != nullptr && !CassoExplorerCommands::IsAllowedInPicker (id, picker->IsSave());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::CreateDisk
//
//  Create an empty disk in the listed folder, whatever is selected, as File
//  Explorer's New does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::CreateDisk (const CassoExplorerNewDiskDialog::Outcome & newDisk)
{
    CassoExplorerActions::Outcome  outcome = m_context.actions.CreateImage (m_context.browser.GetLocation().path, newDisk.fileName, newDisk.request);



    ReportOutcome (outcome, L"New disk");
    m_context.folderSync.RefreshAfterHostChange();
    m_context.listPane.SelectRowNamed (newDisk.fileName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::CreateDiskForPicker
//
//  A drive's picker creates a disk in the folder it lists, or in the
//  request's create folder from a list that is no folder. The dialog opens
//  on the kind of disk the drive takes. True with the new image's path when
//  the drive accepts what was made.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::CreateDiskForPicker (std::wstring & outPath)
{
    static constexpr int  kContainerPo  = 2;   // indices into CassoExplorerNewDiskChoices::kContainers and kSizes
    static constexpr int  kContainerHdv = 5;
    static constexpr int  kSize800K     = 1;
    static constexpr int  kSize5Mb      = 2;
    static constexpr int  kSize10Mb     = 3;
    static constexpr int  kSize32Mb     = 5;



    const PickerRequest                  * request   = m_context.app.picker;
    Location                               location  = m_context.browser.GetLocation();
    int                                    format    = CassoExplorerNewDiskChoices::kFormatDos33;
    int                                    container = 0;
    int                                    size      = 0;
    bool                                   isListed  = false;
    CassoExplorerNewDiskDialog::Outcome    newDisk;



    outPath.clear();

    if (request == nullptr || !request->IsForDrive())
    {
        return false;
    }

    if (location.kind != Location::Kind::HostFolder || location.path.empty())
    {
        if (request->createFolder.empty())
        {
            return false;
        }

        m_context.addressBar.GoToLocation (Location::MakeHostFolder (request->createFolder.wstring()));
        location = m_context.browser.GetLocation();

        if (location.kind != Location::Kind::HostFolder)
        {
            return false;
        }
    }

    switch (request->receivingDrive->kind)
    {
        case ReceivingDriveKind::Apple35:
        case ReceivingDriveKind::UniDisk35:
            format    = CassoExplorerNewDiskChoices::kFormatProDos;
            container = kContainerPo;
            size      = kSize800K;
            break;

        case ReceivingDriveKind::HardDisk:
            format    = CassoExplorerNewDiskChoices::kFormatProDos;
            container = kContainerHdv;
            size      = (request->receivingDrive->maxBlocks >= 65535) ? kSize32Mb : (request->receivingDrive->maxBlocks >= 19456) ? kSize10Mb : kSize5Mb;
            break;

        default:
            break;
    }

    newDisk = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, false, [this, &location] (const std::wstring & fileName)
    {
        return m_context.actions.IsNameTaken (location.path, fileName);
    }, format, {}, false, container, size);

    if (!newDisk.confirmed || !TryPrepareBootDisk (newDisk.request))
    {
        return false;
    }

    CreateDisk (newDisk);

    for (const CatalogRow & row : m_context.browser.GetRows())
    {
        isListed = isListed || (!row.isDirectory && _wcsicmp (row.name.c_str(), newDisk.fileName.c_str()) == 0);
    }

    if (!isListed)
    {
        m_context.window.ShowMessage (std::format (L"{} was created, but {} doesn't take that kind of disk.", newDisk.fileName, request->driveLabel), MB_ICONINFORMATION);
        return false;
    }

    outPath = CassoExplorerBrowser::JoinPath (location.path, newDisk.fileName);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::InspectDiskImages
//
//  With no app, as in a test, no inspector is opened.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::InspectDiskImages (const std::vector<std::wstring> & paths)
{
    if (m_context.app.host != nullptr && m_context.theme != nullptr && !paths.empty())
    {
        m_context.app.host->InspectDiskImages (m_context.window.GetHwnd(), paths, *m_context.theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::RepairDiskImage
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::RepairDiskImage (const std::wstring & path)
{
    if (m_context.app.host != nullptr && m_context.theme != nullptr)
    {
        m_context.app.host->RepairDiskImage (m_context.window.GetHwnd(), path, *m_context.theme);
    }
}
