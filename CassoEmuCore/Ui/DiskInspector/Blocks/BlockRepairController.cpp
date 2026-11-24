#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/DiskInspectCommand.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/IDriveSpace.h"
#include "Seams/IHostDialogs.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"
#include "Ui/DiskInspector/Blocks/BlockRepairController.h"
#include "Ui/DiskInspector/Blocks/BlockRepairTarget.h"
#include "Ui/DiskInspector/RepairDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::BlockRepairController
//
////////////////////////////////////////////////////////////////////////////////

BlockRepairController::BlockRepairController (const Parts & parts) :
    m_parts (parts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::GetVolume
//
//  The volume of the current place, or the image's only volume.
//
////////////////////////////////////////////////////////////////////////////////

int BlockRepairController::GetVolume() const
{
    int  volume = m_parts.navigation.GetNavigator().GetPlace().volume;



    if (volume < 0 && m_parts.model.GetVolumeCount() == 1)
    {
        volume = 0;
    }

    return volume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::GetNotice
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepairController::GetNotice (bool & outIsBad, bool & outCanRepair)
{
    BlockInspectorModel                       & model   = m_parts.model;
    int                                         volume  = model.IsOpen() ? GetVolume() : -1;
    uint32_t                                    current = model.GetMapGeneration();
    FileSystemVerdict                           verdict = FileSystemVerdict::None;
    RepairPlan                                  plan;
    vector<DiskInspectCommand::BlockFinding>    findings;



    if (current != m_noticeGeneration || volume != m_noticeVolume)
    {
        m_noticeGeneration = current;
        m_noticeVolume     = volume;
        m_notice.clear();
        m_isNoticeBad      = false;
        m_canNoticeRepair  = false;

        verdict = (volume >= 0) ? BlockRepair::GetVerdict (model.GetVolumeMap (volume)) : FileSystemVerdict::None;

        if (verdict == FileSystemVerdict::Bad || verdict == FileSystemVerdict::Suspect)
        {
            findings          = DiskInspectCommand::GetBlockFindings (model.GetVolumeMap (volume));
            plan              = model.PlanRepair (volume, {});
            m_notice          = FormatNotice (verdict, findings.empty() ? std::wstring() : findings.front().sentence, plan);
            m_isNoticeBad     = verdict == FileSystemVerdict::Bad;
            m_canNoticeRepair = plan.canRepair;
        }
    }

    outIsBad     = m_isNoticeBad;
    outCanRepair = m_canNoticeRepair;

    return m_notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::FormatNotice
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepairController::FormatNotice (FileSystemVerdict verdict, const std::wstring & finding, const RepairPlan & plan)
{
    std::wstring  sentence = finding;
    std::wstring  text;



    if (!sentence.empty())
    {
        sentence[0] = static_cast<wchar_t> (towupper (sentence[0]));
    }

    if (!sentence.empty() && !sentence.ends_with (L'.'))
    {
        sentence += L".";
    }

    text = FileSystemCheck::FormatVerdict (verdict) + (sentence.empty() ? std::wstring (L".") : L": " + sentence);

    if (!plan.canRepair)
    {
        text += L" Repair isn't available. " + plan.reason;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::Repair
//
//  The confirmation lists every change the volume can have, and the checked
//  ones are written to a repaired copy, as the track inspector writes one,
//  where BlockRepairTarget chooses.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepairController::Repair()
{
    BlockInspectorModel   & model  = m_parts.model;
    int                     volume = GetVolume();
    RepairPlan              full   = model.PlanRepair (volume, {});
    RepairDialog            dialog;
    BlockRepairSource       source;
    BlockRepairDestination  destination;
    std::wstring            intro;
    int                     lines  = 0;
    std::set<std::wstring>  chosen;
    HRESULT                 hr     = S_OK;



    if (m_parts.jobs.IsBusy() || !full.canRepair)
    {
        m_parts.dialogs.ShowWarning (L"Repair isn't available. " + (m_parts.jobs.IsBusy() ? std::wstring (L"Wait for the current job to finish.") : full.reason));
        return;
    }

    intro = BlockRepairTarget::BuildIntro (m_parts.file.IsOnCard(), m_parts.file.GetFileName(), m_parts.file.GetHostFileName(), full.before == FileSystemVerdict::Suspect, model.IsSectorVolume (volume), lines);

    //  Blocks in no file can be another file system's on a hybrid disk or a
    //  larger DOS image, so freeing them starts unchecked.
    chosen = DiskRepair::GetKeys (full);
    chosen.erase (L"unowned");

    dialog.Configure (m_parts.dialogs.GetDialogTheme(), intro, lines, full, [&model, volume] (const std::set<std::wstring> & keys) { return model.PlanRepair (volume, keys); }, chosen);

    hr = dialog.ShowModal (m_parts.dialogs.GetDialogOwner(), m_parts.dialogs.GetDialogTheme());

    if (FAILED (hr) || !dialog.IsConfirmed())
    {
        return;
    }

    source.isCard         = m_parts.file.IsOnCard();
    source.imagePath      = m_parts.file.GetPath();
    source.hostFileName   = m_parts.file.GetHostFileName();
    source.volumeLabel    = model.GetVolumeMap (volume).volumeName;
    source.isSectorVolume = model.IsSectorVolume (volume);
    source.volumeBytes    = (uint64_t) model.GetVolumeSize (volume) * BlockInspectorModel::kBlockBytes;
    destination           = BlockRepairTarget::ChooseDestination (source, m_parts.file.GetFileIo(), m_parts.drives);

    if (destination.isUserChoice && !PickTarget (destination.why, destination.bytes, destination.host))
    {
        return;
    }

    QueueCopy (volume, dialog.GetPlan(), source.isCard, destination.host.wstring() + destination.rest);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::PickTarget
//
//  The Save dialog opens again until the file isn't the image and its
//  drive has room for the copy. False when the user cancels.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockRepairController::PickTarget (const std::wstring & why, uint64_t bytes, std::filesystem::path & ioTarget)
{
    const std::wstring     & image     = m_parts.file.GetPath();
    std::wstring             extension = ioTarget.has_extension() ? ioTarget.extension().wstring().substr (1) : std::wstring (L"po");
    FileDialogSpec           spec;
    std::filesystem::path    picked;
    bool                     isPicked  = false;
    bool                     isChosen  = false;
    std::error_code          error;
    HRESULT                  hr        = S_OK;



    if (!why.empty())
    {
        m_parts.dialogs.ShowWarning (why);
    }

    spec.title            = L"Save the repaired copy";
    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = ioTarget.filename().wstring();
    spec.initialFolder    = ioTarget.parent_path();

    while (!isChosen)
    {
        hr = m_parts.dialogs.GetHostDialogs().PickFileToSave (m_parts.dialogs.GetDialogOwner(), spec, picked, isPicked);

        if (FAILED (hr) || !isPicked)
        {
            return false;
        }

        if (!m_parts.file.IsOnCard() && (std::filesystem::equivalent (picked, image, error) || _wcsicmp (std::filesystem::absolute (picked, error).wstring().c_str(), std::filesystem::absolute (image, error).wstring().c_str()) == 0))
        {
            m_parts.dialogs.ShowWarning (L"That file is the image this window opened. Choose another file for the copy.");
        }
        else if (!m_parts.drives.HasRoomFor (picked.parent_path(), bytes))
        {
            m_parts.dialogs.ShowWarning (L"That drive doesn't have room for the repaired copy. Choose another folder.");
        }
        else
        {
            isChosen = true;
        }
    }

    ioTarget = picked;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController::QueueCopy
//
//  The image copied and the plan's blocks written into the copy, or a card's
//  partition written whole, on the worker's thread. A copy that fails
//  partway is deleted.
//
////////////////////////////////////////////////////////////////////////////////

void BlockRepairController::QueueCopy (int volume, const RepairPlan & plan, bool isCard, const std::filesystem::path & target)
{
    auto                    result = std::make_shared<BlockJobResult>();
    BlockInspectorModel   * model  = &m_parts.model;
    IDiskFileIo           * fileIo = &m_parts.file.GetFileIo();
    std::filesystem::path   image  = m_parts.file.GetPath();



    m_parts.note.SetNote (L"Writing the repaired copy...");

    m_parts.jobs.QueueJob ({ L"Writing the repaired copy", false,
                             [result, model, fileIo, image, target, volume, plan, isCard] (BlockJobProgress & progress)
                             {
                                 std::string   path       = TextEncoding::WideToUtf8 (target.wstring());
                                 std::wstring  targetHost = TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (path));

                                 //  An image in a container is copied with its host file.
                                 if (!isCard && !CopyFileW (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (image.wstring()))).c_str(), targetHost.c_str(), FALSE))
                                 {
                                     result->hr      = HRESULT_FROM_WIN32 (GetLastError());
                                     result->message = L"The image can't be copied.";
                                 }
                                 else
                                 {
                                     result->hr = model->WriteRepairTo (*fileIo, path, volume, plan, isCard, result->message, &progress);
                                 }

                                 if (FAILED (result->hr) && fileIo->Exists (TextEncoding::WideToUtf8 (targetHost)))
                                 {
                                     (void) fileIo->Remove (TextEncoding::WideToUtf8 (targetHost));
                                 }
                             },
                             [this, result, target] ()
                             {
                                 m_parts.note.SetNote (SUCCEEDED (result->hr) ? L"Repaired copy written: " + std::filesystem::path (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (target.wstring())))).filename().wstring()
                                                                              : L"Repair failed: " + (result->message.empty() ? std::wstring (L"The repaired copy can't be written.") : result->message));
                                 m_parts.view.Refresh();
                             } });
}
