#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerImageTools.h"
#include "CassoExplorer/CassoExplorerCopyOptionsDialog.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/Managers/ExplorerCollectionTools.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerSearch.h"
#include "Config/FileAssociations.h"
#include "Seams/Win32UserClasses.h"
#include "Update/Win32InstallEnvironment.h"
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
#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/FullSendWindowManager.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/WozPropertiesDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::ExplorerVerbs
//
////////////////////////////////////////////////////////////////////////////////

ExplorerVerbs::ExplorerVerbs (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::GetNamingStyle
//
//  The host file name style the Options dialog set; its choices are in the
//  style's own order.
//
////////////////////////////////////////////////////////////////////////////////

HostFileNaming::Style ExplorerVerbs::GetNamingStyle() const
{
    if (m_context.prefs.hostNaming == CassoExplorerPrefs::kNamingCiderPress)
    {
        return HostFileNaming::Style::CiderPress;
    }

    if (m_context.prefs.hostNaming == CassoExplorerPrefs::kNamingAppleSingle)
    {
        return HostFileNaming::Style::AppleSingle;
    }

    return HostFileNaming::Style::Descriptive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::GetRecycledPaths
//
//  Where each of the Recycle Bin's items was when it was deleted.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerVerbs::GetRecycledPaths (const std::vector<std::wstring> & ids)
{
    std::vector<IShellItemVerbs::RecycledItem>  items;
    std::vector<std::wstring>                   paths;
    HRESULT                                     hr    = m_context.shellVerbs.ListRecycled (items);



    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const IShellItemVerbs::RecycledItem & item : items)
    {
        if (std::find (ids.begin(), ids.end(), item.id) != ids.end())
        {
            paths.push_back ((std::filesystem::path (item.originalFolder) / item.name).wstring());
        }
    }

    return paths;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::RunRecycleBinVerb
//
//  A verb on the Recycle Bin's items, carried out by the bin itself. Whether
//  the verb was the bin's to carry out.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerVerbs::RunRecycleBinVerb (CassoExplorerActions::Verb verb)
{
    using Verb = CassoExplorerActions::Verb;

    HRESULT                    hr        = S_OK;
    std::vector<std::wstring>  ids;
    std::vector<std::wstring>  originals;



    m_context.browser.GetLoader().GetSelectedRecycledIds (ids);

    switch (verb)
    {
        case Verb::Open:
            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_context.shellVerbs.RunRecycledVerb (m_context.window.GetHwnd(), ids, IShellItemVerbs::RecycledVerb::Properties);
            break;

        case Verb::Restore:
        case Verb::Delete:
            //  Restore with nothing selected restores everything, as the
            //  command bar's Restore all items does.
            if (ids.empty() && verb == Verb::Restore)
            {
                m_context.browser.GetLoader().GetAllRecycledIds (ids);
            }

            BAIL_OUT_IF (ids.empty(), S_OK);
            originals = GetRecycledPaths (ids);
            hr        = m_context.shellVerbs.RunRecycledVerb (m_context.window.GetHwnd(), ids, (verb == Verb::Restore) ? IShellItemVerbs::RecycledVerb::Restore
                                                                                              : IShellItemVerbs::RecycledVerb::Delete);

            if (SUCCEEDED (hr) && verb == Verb::Restore)
            {
                m_context.manifestSync.NoteRestores (originals);
            }

            m_context.folderSync.RefreshAfterHostChange();
            break;

        case Verb::EmptyRecycleBin:
            hr = m_context.shellVerbs.EmptyRecycleBin (m_context.window.GetHwnd());
            m_context.folderSync.RefreshAfterHostChange();
            break;

        default:
            return false;
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerVerbs::RunVerb (CassoExplorerActions::Verb verb)
{
    HRESULT                              hr        = S_OK;
    std::filesystem::path                picked;
    bool                                 chosen    = false;
    FileDialogSpec                       spec;
    int                                  answer    = 0;
    CassoExplorerActions::Outcome        outcome;
    std::wstring                         newName;
    std::wstring                         partition;
    std::vector<std::wstring>            hostPaths;
    CassoExplorerNewDiskDialog::Outcome  newDisk;
    HostFileNaming::Style                style     = GetNamingStyle();



    //  The Recycle Bin's items are the shell's to restore, delete and
    //  describe; opening one shows its properties, as Explorer's does.
    if (m_context.browser.GetLocation().kind == Location::Kind::RecycleBin && RunRecycleBinVerb (verb))
    {
        return;
    }

    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:
            if (m_context.browser.IsImageLocation() && !(m_context.browser.GetSelectedRows().size() == 1 && m_context.browser.OpenRow (m_context.browser.GetSelectedRows()[0])))
            {
                m_context.opener.OpenSelectedEntries();
            }
            else if (m_context.browser.IsImageLocation())
            {
                m_context.listPane.FillList();
            }
            else if (m_context.browser.GetSelectedRows().size() > 1)
            {
                m_context.opener.OpenEachSelected();
            }
            else if (!m_context.browser.GetSelectedRows().empty() && m_context.browser.OpenRow (m_context.browser.GetSelectedRows()[0]))
            {
                m_context.listPane.FillList();
            }
            else if (!m_context.cassoTarget.GetSelectedImagePath().empty())
            {
                //  Not something to browse: a real file, in its own program.
                hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), m_context.cassoTarget.GetSelectedImagePath());
            }

            break;

        case CassoExplorerActions::Verb::Get:
            hr = m_context.dialogs.PickFolder (m_context.window.GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_context.actions.GetSelected (picked.wstring(), style, m_context.prompts.MakeCollisionPrompt()), L"Copy");
            }

            break;

        //  The choices become the options of every later copy, drag out and
        //  Copy to folder, and are saved with Casso Explorer's settings. A
        //  selection with no file that has options goes straight to the
        //  folder.
        case CassoExplorerActions::Verb::GetWithOptions:
        {
            std::vector<CopyOptionRows::Row>  rows;
            DecodeOptions                     options = m_context.browser.GetPreviewer().GetDefaultDecodeOptions();

            m_context.actions.FindCopyOptionRows (rows);

            if (!rows.empty() && !CassoExplorerCopyOptionsDialog::Choose (m_context.window.GetHwnd(), m_context.theme, rows, options))
            {
                break;
            }

            m_context.prefs.copyOptions = options;
            m_context.browser.GetPreviewer().SetDefaultDecodeOptions (options);

            hr = m_context.dialogs.PickFolder (m_context.window.GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_context.actions.GetSelected (picked.wstring(), style, m_context.prompts.MakeCollisionPrompt()), L"Copy");
            }

            break;
        }

        case CassoExplorerActions::Verb::Put:
            hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_context.actions.PutFiles ({ picked.wstring() }, m_context.prompts.MakeAddressPrompt());

                //  An Apple drive's writes finish later, over the link, with no undo.
                if (!m_context.browser.IsAppleDriveLocation())
                {
                    m_context.undo.PushPut (outcome, m_context.browser.GetLocation().path, m_context.browser.GetLocation().innerPath);
                }

                ReportOutcome (outcome, L"Put");
                m_context.listPane.FillList();
            }

            break;

        case CassoExplorerActions::Verb::Cut:
        case CassoExplorerActions::Verb::Copy:
            if (m_context.browser.IsVolumeLocation() && verb == CassoExplorerActions::Verb::Cut)
            {
                m_context.clipboard.CutEntriesToClipboard (GetNamingStyle());
                break;
            }

            if (m_context.browser.IsVolumeLocation())
            {
                m_context.clipboard.CopyEntriesToClipboard (GetNamingStyle());
                break;
            }

            if (m_context.browser.GetSelection().TryGetSelectedShellIds (hostPaths))
            {
                hr = m_context.shellVerbs.PlaceOnClipboard (m_context.window.GetHwnd(), hostPaths, verb == CassoExplorerActions::Verb::Cut);
            }

            m_context.listPane.SetCutPaths ((SUCCEEDED (hr) && verb == CassoExplorerActions::Verb::Cut) ? hostPaths : std::vector<std::wstring>());
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Share:
            m_context.browser.GetSelection().GetSelectedHostPaths (hostPaths);
            hr = hostPaths.empty() ? E_NOT_SET : m_context.shellVerbs.Share (m_context.window.GetHwnd(), hostPaths);

            if (FAILED (hr))
            {
                m_context.window.ShowMessage (std::format (L"Windows could not open the share window for the selected files (0x{:08X}).", (uint32_t) hr), MB_ICONWARNING);
            }

            break;

        case CassoExplorerActions::Verb::ExtractAll:
            m_context.imageTools.ExtractAll();
            break;

        case CassoExplorerActions::Verb::ExtractFiles:
            m_context.imageTools.ExtractImageFiles (m_context.cassoTarget.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::InspectDiskImage:
            m_context.opener.InspectSelected();
            break;

        case CassoExplorerActions::Verb::CompareDiskImages:
            m_context.imageTools.CompareSelectedDiskImages();
            break;

        case CassoExplorerActions::Verb::SelectForCompare:
            m_context.actions.SetComparePick (m_context.cassoTarget.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::CompareToPick:
            m_context.imageTools.CompareToPick (m_context.cassoTarget.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::Paste:
            if (m_context.browser.IsVolumeLocation())
            {
                m_context.clipboard.PasteIntoVolume();
                break;
            }

            m_context.clipboard.PasteHere (m_context.clipboard.GetPasteFolder());
            break;

        case CassoExplorerActions::Verb::Delete:
            m_context.deletion.DeleteSelected();
            break;

        case CassoExplorerActions::Verb::Boot:
            ReportOutcome (m_context.actions.BootSelected(), L"Set startup program");
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Rename:
            m_context.rename.BeginRename();
            break;

        case CassoExplorerActions::Verb::Lock:
        case CassoExplorerActions::Verb::Unlock:
            ReportOutcome (m_context.actions.SetLockedSelected (verb == CassoExplorerActions::Verb::Lock), (verb == CassoExplorerActions::Verb::Lock) ? L"Lock" : L"Unlock");
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::ConvertTo:
        {
            std::string  converted;

            outcome = m_context.actions.ConvertSelected (converted);
            ReportOutcome (outcome, L"Convert");
            m_context.listPane.FillList();

            //  On an image the new file is listed now, selected for renaming;
            //  on a linked drive it is written once the read is done.
            if (outcome.Succeeded() && !m_context.browser.IsAppleDriveLocation() && m_context.listPane.SelectRowNamed (TextEncoding::NarrowToWide (converted)))
            {
                m_context.rename.BeginRename();
            }

            break;
        }

        case CassoExplorerActions::Verb::ConvertToTapeAudio:
        case CassoExplorerActions::Verb::ConvertToDiskAudio:
        case CassoExplorerActions::Verb::AddToA2retroNet:
            m_context.opener.RunFileVerb (verb);
            break;

        case CassoExplorerActions::Verb::InsertDrive1:
        case CassoExplorerActions::Verb::InsertDrive2:
            m_context.cassoTarget.InsertIntoDrive (m_context.cassoTarget.GetSelectedImagePath(), verb == CassoExplorerActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassoExplorerActions::Verb::OpenInNewCasso:
            m_context.cassoTarget.OpenInNewCasso (m_context.cassoTarget.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::NewDisk:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, false, [this] (const std::wstring & fileName)
            {
                return m_context.actions.IsNameTaken (m_context.browser.GetLocation().path, fileName);
            });

            if (newDisk.confirmed && m_context.imageTools.TryPrepareBootDisk (newDisk.request))
            {
                m_context.imageTools.CreateDisk (newDisk);
            }

            break;

        case CassoExplorerActions::Verb::NewFolder:
            //  An unused default name, open for renaming at once.
            newName = m_context.actions.GetNewFolderName();

            if (!m_context.browser.IsVolumeLocation())
            {
                hr = m_context.shellVerbs.CreateFolder (m_context.window.GetHwnd(), m_context.browser.GetLocation().path, newName);

                if (SUCCEEDED (hr))
                {
                    m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::HostNewFolder, Location(), { (std::filesystem::path (m_context.browser.GetLocation().path) / newName).wstring() } });
                }

                m_context.folderSync.RefreshAfterHostChange();
            }
            else
            {
                outcome = m_context.actions.CreateFolder (newName);

                if (outcome.Succeeded() && !m_context.browser.IsAppleDriveLocation())
                {
                    m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::ImageNewFolder, m_context.browser.GetLocation(), {}, std::wstring(), newName });
                }

                ReportOutcome (outcome, L"New folder");
                m_context.listPane.FillList();
            }

            if (m_context.listPane.SelectRowNamed (newName))
            {
                m_context.rename.BeginRename();
            }

            break;

        case CassoExplorerActions::Verb::Format:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, true, {}, m_context.imageTools.ChooseFormatDefault (m_context.actions.GetFormatTarget()),
                                                       std::filesystem::path (m_context.actions.GetFormatTarget()).filename().wstring());

            if (newDisk.confirmed && m_context.imageTools.TryPrepareBootDisk (newDisk.request))
            {
                answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                                         (L"Format " + CassoExplorerActions::GetLeafName (m_context.actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_OKCANCEL | MB_ICONWARNING, { L"Format", L"Cancel" });

                if (answer == IDOK)
                {
                    ReportOutcome (m_context.actions.FormatImage (newDisk.request), L"Format");
                    m_context.listPane.FillList();
                }
            }

            break;

        case CassoExplorerActions::Verb::Krunch:
            ReportOutcome (m_context.actions.KrunchImage(), L"Krunch");
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::MakeArchive:
        case CassoExplorerActions::Verb::MakeShrinkItDisk:
            m_context.imageTools.MakeArchive (verb == CassoExplorerActions::Verb::MakeShrinkItDisk);
            break;

        case CassoExplorerActions::Verb::ConvertToDsk:
        case CassoExplorerActions::Verb::ConvertToPo:
        case CassoExplorerActions::Verb::ConvertToTwoImg:
        case CassoExplorerActions::Verb::ConvertToHdv:
        case CassoExplorerActions::Verb::ConvertToWoz:
            m_context.imageTools.ConvertImage (verb);
            break;

        case CassoExplorerActions::Verb::Test:
            m_context.imageTools.TestImage();
            break;

        case CassoExplorerActions::Verb::CopyPartitionToImage:
            m_context.actions.TryGetPartition (partition);
            m_context.imageTools.CopyPartitionToImage (partition);
            break;

        case CassoExplorerActions::Verb::ReplacePartition:
            m_context.actions.TryGetPartition (partition);
            m_context.imageTools.ReplacePartition (partition);
            break;

        case CassoExplorerActions::Verb::Refresh:
            m_context.commandManager.Dispatch (CassoExplorerCommands::kRefresh);
            break;

        case CassoExplorerActions::Verb::ReadAsCpm:
        case CassoExplorerActions::Verb::DetectFilesystem:
            m_context.imageTools.ChooseFilesystem ((verb == CassoExplorerActions::Verb::ReadAsCpm) ? VolumeKind::Cpm : VolumeKind::Unknown);
            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerVerbs::ReportOutcome (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_context.statusBar.SetDetail (std::format (L"{}: {} file(s)", verbName, outcome.written));

        //  For a copy salvaged from a damaged disk, what could not be read is shown.
        if (!outcome.warning.empty())
        {
            m_context.window.ShowMessage (outcome.warning, MB_ICONWARNING);
        }

        return;
    }

    m_context.window.ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::GetVerbTitle
//
//  A verb's label as a window's title: no access key, no trailing dots.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerVerbs::GetVerbTitle (CassoExplorerActions::Verb verb)
{
    std::wstring  title = ExplorerMenus::GetVerbLabel (verb);



    std::erase (title, L'&');

    while (!title.empty() && title.back() == L'.')
    {
        title.pop_back();
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerVerbs::RunRawVerb (CassoExplorerActions::Verb verb)
{
    HRESULT                          hr       = S_OK;
    bool                             sectors  = verb == CassoExplorerActions::Verb::ReadSectors || verb == CassoExplorerActions::Verb::WriteSectors;
    bool                             reading  = verb == CassoExplorerActions::Verb::ReadSectors || verb == CassoExplorerActions::Verb::ReadBlocks;
    std::wstring                     title    = GetVerbTitle (verb);
    std::filesystem::path            picked;
    bool                             chosen   = false;
    FileDialogSpec                   spec;
    int                              answer   = 0;
    CassoExplorerActions::Outcome    outcome;
    CassoExplorerRawDialog::Outcome  where;
    std::wstring                     target   = m_context.actions.GetFormatTarget();



    if (target.empty())
    {
        return;
    }

    //  The raw files are bytes as they lie on the disk; the picker keeps its
    //  own folder and names typed, apart from Casso's other pickers.
    spec.filters    = { FileDialogFilter { sectors ? L"Sector dumps (*.bin)" : L"Block dumps (*.bin)", L"*.bin" },
                        FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid = s_kRawPickerGuid;

    if (!reading)
    {
        spec.title = title;
        hr         = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }
    }

    where = CassoExplorerRawDialog::Ask (m_context.window.GetHwnd(), m_context.theme, title, sectors, reading,
                                         sectors ? CassoExplorerRawChoices::kBlocks : m_context.actions.GetTargetBlockCount());

    if (!where.confirmed)
    {
        return;
    }

    if (reading)
    {
        spec.defaultFileName  = sectors ? L"sectors.bin" : L"blocks.bin";
        spec.defaultExtension = L"bin";
        spec.askToReplace     = false;
        spec.title            = L"Save as";

        //  The picker's own replace prompt is Win32's; this one follows the
        //  theme, and No goes back to the picker.
        do
        {
            hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);

            if (FAILED (hr) || !chosen)
            {
                return;
            }

            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
        }
        while (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
               DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                               (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                               L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK);

        if (!IsRawTargetCurrent (target))
        {
            return;
        }

        outcome = sectors ? m_context.actions.ReadSectors (where.start, where.sector, where.count, picked.wstring(), where.numbering)
                          : m_context.actions.ReadBlocks  (where.start, where.count, picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                             (L"The file's bytes will be written straight onto " + CassoExplorerActions::GetLeafName (target) + L", over what is there now.").c_str(),
                             title.c_str(),
                             MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" });

    if (answer != IDOK || !IsRawTargetCurrent (target))
    {
        return;
    }

    outcome = sectors ? m_context.actions.WriteSectors (where.start, where.sector, picked.wstring(), where.numbering)
                      : m_context.actions.WriteBlocks  (where.start, picked.wstring());

    ReportOutcome (outcome, L"Write");
    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerVerbs::IsRawTargetCurrent
//
//  The window's timers run while the dialogs are open, and a refresh can move
//  the list off the image the raw read or write was chosen for.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerVerbs::IsRawTargetCurrent (const std::wstring & target)
{
    if (m_context.actions.GetFormatTarget() == target)
    {
        return true;
    }

    m_context.window.ShowMessage (CassoExplorerActions::GetLeafName (target) + L" is no longer selected, so nothing was read or written.", MB_ICONWARNING);
    return false;
}
