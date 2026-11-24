#include "Pch.h"
#include "CassoExplorer/Window/Managers/ExplorerImageTools.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
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
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Machines/Apple2/Common/DiskImageConverter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ExplorerImageTools
//
////////////////////////////////////////////////////////////////////////////////

ExplorerImageTools::ExplorerImageTools (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::CompareSelectedDiskImages
//
//  A is the focused image, which is the one the menu was opened on, and B is
//  the other selected image (FR-117).
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::CompareSelectedDiskImages()
{
    const std::vector<int>     & rows  = m_context.browser.GetSelectedRows();
    std::vector<std::wstring>    paths;
    size_t                       a     = 0;



    m_context.browser.GetSelection().GetSelectedHostPaths (paths);

    if (m_context.app.host != nullptr && m_context.theme != nullptr && paths.size() == 2 && rows.size() == 2)
    {
        a = (rows[1] == m_context.listPane.GetFocusedRow()) ? 1 : 0;

        m_context.app.host->CompareDiskImages (m_context.window.GetHwnd(), paths[a], paths[1 - a], *m_context.theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::CompareToPick
//
//  The image picked by Select for compare is A and this one is B. Once the
//  comparison opens, the pick is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::CompareToPick (const std::wstring & image)
{
    std::wstring  pick = m_context.actions.GetComparePick();



    if (m_context.app.host == nullptr || m_context.theme == nullptr || !m_context.actions.CanCompareToPick (image))
    {
        return;
    }

    m_context.app.host->CompareDiskImages (m_context.window.GetHwnd(), pick, image, *m_context.theme);
    m_context.actions.ClearComparePick();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ExtractImageFiles
//
//  The image's files go into a folder beside it, which is then selected in
//  the list when the list is on that folder's parent.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::ExtractImageFiles (const std::wstring & imagePath)
{
    std::wstring                   folder;
    CassoExplorerActions::Outcome  outcome;



    if (imagePath.empty())
    {
        return;
    }

    outcome = m_context.actions.ExtractImage (imagePath, m_context.verbs.GetNamingStyle(), folder);

    m_context.verbs.ReportOutcome (outcome, L"Extract all files");
    m_context.folderSync.RefreshAfterHostChange();

    if (!folder.empty() && m_context.browser.GetLocation().kind == Location::Kind::HostFolder
        && std::filesystem::path (m_context.browser.GetLocation().path) == std::filesystem::path (folder).parent_path())
    {
        m_context.listPane.SelectRowNamed (std::filesystem::path (folder).filename().wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::MakeArchive
//
//  The new archive is selected once the folder is listed again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::MakeArchive (bool shrinkItDisk)
{
    std::vector<std::wstring>      hostPaths;
    std::wstring                   created;
    CassoExplorerActions::Outcome  outcome;



    m_context.browser.GetSelection().GetSelectedHostPaths (hostPaths);

    if (hostPaths.empty() || (shrinkItDisk && hostPaths.size() != 1))
    {
        return;
    }

    outcome = shrinkItDisk ? m_context.actions.MakeShrinkItDisk (hostPaths[0], created) : m_context.actions.MakeArchive (hostPaths, created);

    m_context.verbs.ReportOutcome (outcome, shrinkItDisk ? L"Make a ShrinkIt disk" : L"Add to ShrinkIt archive");
    m_context.folderSync.RefreshAfterHostChange();

    if (!created.empty())
    {
        m_context.listPane.SelectRowNamed (std::filesystem::path (created).filename().wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ConvertImage
//
//  The new image is selected once the folder is listed again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::ConvertImage (ActionsTypes::Verb verb)
{
    std::vector<std::wstring>      hostPaths;
    std::wstring                   created;
    CassoExplorerActions::Outcome  outcome;
    DiskImageFormat                format = DiskImageFormat::Dsk;



    m_context.browser.GetSelection().GetSelectedHostPaths (hostPaths);

    if (hostPaths.size() != 1 || !ActionsArchive::TryGetConvertFormat (verb, format))
    {
        return;
    }

    outcome = m_context.actions.ConvertImage (hostPaths[0], format, created);

    m_context.verbs.ReportOutcome (outcome, L"Convert");
    m_context.folderSync.RefreshAfterHostChange();

    if (!created.empty())
    {
        m_context.listPane.SelectRowNamed (std::filesystem::path (created).filename().wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::CopyPartitionToImage
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::CopyPartitionToImage (const std::wstring & partition)
{
    std::wstring                   created;
    CassoExplorerActions::Outcome  outcome;
    std::wstring                   target;



    if (partition.empty() || !m_context.opener.PickPartitionImage (partition, target))
    {
        return;
    }

    outcome = m_context.actions.CopyPartitionToImage (partition, created, target);

    m_context.verbs.ReportOutcome (outcome, L"Create image from partition");

    if (!created.empty())
    {
        m_context.window.ShowMessage (L"The partition was copied to " + created + L".", MB_ICONINFORMATION);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ReplacePartition
//
//  The image is picked, then the replace is confirmed with a question that
//  lists the partition and the image.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::ReplacePartition (const std::wstring & partition)
{
    HRESULT                        hr       = S_OK;
    std::wstring                   question;
    std::filesystem::path          picked;
    FileDialogSpec                 spec;
    CassoExplorerActions::Outcome  outcome;
    bool                           chosen   = false;



    if (partition.empty())
    {
        return;
    }

    hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);

    if (FAILED (hr) || !chosen)
    {
        return;
    }

    outcome = m_context.actions.DescribePartitionReplace (partition, picked.wstring(), question);

    if (!outcome.Succeeded())
    {
        m_context.verbs.ReportOutcome (outcome, L"Replace partition");
        return;
    }

    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, question.c_str(), L"Replace partition",
                        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Replace", L"Cancel" }) != IDOK)
    {
        return;
    }

    outcome = m_context.actions.ReplacePartition (partition, picked.wstring());

    m_context.verbs.ReportOutcome (outcome, L"Replace partition");
    m_context.listPane.FillList();

    //  The partition's tree node is labeled with its volume's name, which the
    //  new image can change. A card's partitions are listed again from the
    //  card, and an image file's from the file.
    if (PhysicalCards::IsOnCard (partition))
    {
        m_context.folderSync.CheckDrives();
    }
    else
    {
        m_context.treePane.RefreshTree();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::TestImage
//
//  What the test found, or that it found nothing, in a message.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::TestImage()
{
    std::wstring                   image  = m_context.cassoTarget.GetSelectedImagePath();
    std::wstring                   report;
    CassoExplorerActions::Outcome  outcome;



    if (image.empty())
    {
        return;
    }

    outcome = m_context.actions.TestImage (image, report);

    m_context.window.ShowMessage (report.empty() ? std::filesystem::path (image).filename().wstring() + L" has no problems." : report,
                                  SUCCEEDED (outcome.hr) ? MB_ICONINFORMATION : MB_ICONWARNING);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ExtractAll
//
//  The archive's own Extract All: its wizard, for where the files go, then
//  the copy of the files there, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::ExtractAll()
{
    HRESULT       hr     = S_OK;
    std::wstring  target = m_context.browser.GetArchives().GetExtractTarget();



    BAIL_OUT_IF (target.empty(), S_OK);

    hr = m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), { target }, "extract");
    CHR (hr);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::TryPrepareBootDisk
//
//  A bootable disk with no image supplied copies a stock disk, which is asked
//  about and downloaded here when it isn't in the cache. A disk that starts
//  an Apple II and an Apple /// copies two. False when the user declined or
//  a download failed, and nothing further should happen.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerImageTools::TryPrepareBootDisk (const DiskOperations::NewDiskRequest & request)
{
    std::vector<StockBootDisks::Which>  disks;
    std::wstring                        label;
    std::wstring                        path;
    std::string                         error;
    HRESULT                             hr     = S_OK;



    if (!request.bootable || !StockBootDisks::TryGetStockDisks (request.formatName, request.bootableFrom, disks))
    {
        return true;
    }

    for (StockBootDisks::Which disk : disks)
    {
        if (!StockBootDisks::IsCached (disk) && !TryDownloadBootDisk (disk))
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::TryDownloadBootDisk
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerImageTools::TryDownloadBootDisk (StockBootDisks::Which disk)
{
    std::wstring  label = StockBootDisks::GetLabel (disk);
    std::wstring  path;
    std::string   error;
    HRESULT       hr    = S_OK;



    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                        std::format (L"A bootable disk copies the {} boot disk, which hasn't been downloaded. Download it now?", label).c_str(),
                        L"Download boot disk", MB_OKCANCEL | MB_ICONQUESTION, { L"Download", L"Cancel" }) != IDOK)
    {
        return false;
    }

    hr = AssetBootstrap::EnsureStockBootDisk (disk, path, error);

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"The {} boot disk could not be downloaded. {}", label, TextEncoding::NarrowToWide (error)), MB_ICONERROR);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ChooseFilesystem
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::ChooseFilesystem (VolumeKind kind)
{
    if (m_context.browser.IsAppleDriveLocation())
    {
        m_context.actions.ChooseDriveFilesystem (kind);
    }
    else
    {
        m_context.actions.ChooseImageFilesystem (m_context.actions.GetFormatTarget(), kind);
    }

    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::ChooseFormatDefault
//
//  The file system the image holds now, when it holds one; otherwise what
//  its size and kind allow. DOS 3.3 fits only a 140K floppy, so anything
//  larger -- an 800K disk, a hard disk image -- starts at ProDOS, and so does
//  a ProDOS-order image.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerImageTools::ChooseFormatDefault (const std::wstring & image)
{
    VolumeListing              listing;
    VolumeKind                 kind      = VolumeKind::Unknown;
    WIN32_FILE_ATTRIBUTE_DATA  data      = {};
    std::wstring               extension = std::filesystem::path (image).extension().wstring();
    uint64_t                   size      = 0;



    if (m_context.browser.GetOperations().List (TextEncoding::WideToNarrow (image), listing, kind).Succeeded())
    {
        if (kind == VolumeKind::ProDos)
        {
            return CassoExplorerNewDiskChoices::kFormatProDos;
        }

        if (kind == VolumeKind::Dos33)
        {
            return CassoExplorerNewDiskChoices::kFormatDos33;
        }

        if (kind == VolumeKind::Pascal)
        {
            return CassoExplorerNewDiskChoices::kFormatPascal;
        }

        if (kind == VolumeKind::Cpm)
        {
            return CassoExplorerNewDiskChoices::kFormatCpm;
        }
    }

    if (GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &data))
    {
        size = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;
    }

    for (const wchar_t * prodos : { L".po", L".2mg", L".2img", L".hdv" })
    {
        if (_wcsicmp (extension.c_str(), prodos) == 0)
        {
            return CassoExplorerNewDiskChoices::kFormatProDos;
        }
    }

    //  A nibble or WOZ image is larger than the sectors it holds; its size
    //  says nothing about the disk.
    if (size > kDos33ImageBytes && _wcsicmp (extension.c_str(), L".nib") != 0 && _wcsicmp (extension.c_str(), L".nb2") != 0 && _wcsicmp (extension.c_str(), L".woz") != 0)
    {
        return CassoExplorerNewDiskChoices::kFormatProDos;
    }

    return CassoExplorerNewDiskChoices::kFormatDos33;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::CreateDisk
//
//  Create an empty disk in the listed folder, whatever is selected, as File
//  Explorer's New does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::CreateDisk (const CassoExplorerNewDiskDialog::Outcome & newDisk)
{
    CassoExplorerActions::Outcome  outcome = m_context.actions.CreateImage (m_context.browser.GetLocation().path, newDisk.fileName, newDisk.request);



    m_context.verbs.ReportOutcome (outcome, L"New disk");
    m_context.folderSync.RefreshAfterHostChange();
    m_context.listPane.SelectRowNamed (newDisk.fileName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::CreateDiskForPicker
//
//  A drive's picker creates a disk in the folder it lists, or in the
//  request's create folder from a list that is no folder. The dialog opens
//  on the kind of disk the drive takes. True with the new image's path when
//  the drive accepts what was made.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerImageTools::CreateDiskForPicker (std::wstring & outPath)
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

    outPath = BrowserRowPaths::JoinPath (location.path, newDisk.fileName);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::InspectDiskImages
//
//  With no app, as in a test, no inspector is opened.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::InspectDiskImages (const std::vector<std::wstring> & paths)
{
    if (m_context.app.host != nullptr && m_context.theme != nullptr && !paths.empty())
    {
        m_context.app.host->InspectDiskImages (m_context.window.GetHwnd(), paths, *m_context.theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools::RepairDiskImage
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerImageTools::RepairDiskImage (const std::wstring & path)
{
    if (m_context.app.host != nullptr && m_context.theme != nullptr)
    {
        m_context.app.host->RepairDiskImage (m_context.window.GetHwnd(), path, *m_context.theme);
    }
}
