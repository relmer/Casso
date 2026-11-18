#include "Pch.h"

#include "Devices/Disk/DiskRepairCommand.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskInspectCommand.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommand::Run
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepairCommand::Run (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT        hr         = S_OK;
    std::string    imagePath  = options.disk.imagePath;
    vector<Byte>   bytes;
    DiskAnalysis   analysis;
    RepairOptions  repair;
    RepairPlan     plan;
    std::wstring   reason;
    std::string    written;
    bool           isRead     = false;
    bool           isAnalyzed = false;
    bool           isWritten  = false;
    bool           isBlock    = BlockImageFile::IsBlockImage (io, imagePath);



    if (isBlock)
    {
        RunBlocks (io, options, result);
    }

    BAIL_OUT_IF (isBlock, S_OK);

    hr     = io.ReadAllBytes (imagePath, bytes);
    isRead = SUCCEEDED (hr);
    CBRF (isRead, result.FailAction (imagePath, "read the image", "the file couldn't be read"));

    hr         = FileSystemCheck::Analyze (bytes, imagePath, analysis, reason);
    isAnalyzed = SUCCEEDED (hr);
    CBRF (isAnalyzed, result.FailAction (imagePath, "check the image", TextEncoding::WideToNarrow (reason)));

    repair.isKeepingUnowned = options.disk.noFreeUnowned;
    plan                    = DiskRepair::Plan (analysis, repair);
    result.output          += FormatVerdict (imagePath, analysis);

    for (const RepairChange & change : plan.changes)
    {
        result.output += imagePath + ": change: " + TextEncoding::WideToNarrow (change.text) + "\n";
    }

    if (!plan.canRepair)
    {
        result.output     += imagePath + ": can't repair: " + TextEncoding::WideToNarrow (plan.reason) + "\n";
        result.exitStatus  = DiskCommandResult::kWithComplaints;
    }
    else if (!options.disk.listOnly)
    {
        hr        = RepairCopy::Write (io, imagePath, options.disk.hostFile, plan, written, reason);
        isWritten = SUCCEEDED (hr);
        CBRF (isWritten, result.FailAction (imagePath, "write the repaired copy", TextEncoding::WideToNarrow (reason)));

        result.output += imagePath + ": repaired copy written to " + written + "\n";
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommand::RunBlocks
//
//  Each volume's verdict, findings and changes after the volume, then one
//  repaired copy with every volume's changes. Blocks in no file stay marked
//  used: on a hybrid disk or one with a larger DOS image, they can be
//  another file system's or a program's data.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepairCommand::RunBlocks (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                             hr        = S_OK;
    std::string                         imagePath = options.disk.imagePath;
    std::string                         target    = options.disk.hostFile;
    std::unique_ptr<BlockImageFile>     file;
    BlockInspectorModel                 model;
    vector<std::pair<int, RepairPlan>>  repairs;
    vector<Byte>                        bytes;
    std::wstring                        error;
    std::string                         prefix;
    std::string                         count;
    RepairPlan                          full;
    RepairPlan                          plan;
    std::set<std::wstring>              chosen;
    bool                                isOpen    = false;
    bool                                isTaken   = false;
    bool                                isCopied  = false;
    bool                                isWritten = false;
    bool                                isDone    = false;
    int                                 volume    = 0;



    hr     = BlockImageFile::Open (io, imagePath, file, error);
    isOpen = SUCCEEDED (hr);
    CBRF (isOpen, result.FailAction (imagePath, "check the image", TextEncoding::WideToNarrow (error)));

    hr     = model.Open (std::move (file));
    isOpen = SUCCEEDED (hr);
    CBRF (isOpen, result.FailAction (imagePath, "check the image", "its blocks couldn't be read"));

    for (volume = 0; volume < model.GetVolumeCount(); volume++)
    {
        prefix         = imagePath + ": " + DiskInspectCommand::FormatVolume (model, volume) + ": ";
        full           = model.PlanRepair (volume, {});
        result.output += prefix + "file system: " + TextEncoding::WideToNarrow (FileSystemCheck::FormatVerdict (full.before)) + "\n";

        for (const DiskInspectCommand::BlockFinding & finding : DiskInspectCommand::GetBlockFindings (model.GetVolumeMap (volume)))
        {
            count          = finding.isRole ? std::format (": {} {}{}", finding.units.size(), model.IsSectorVolume (volume) ? "sector" : "block", finding.units.size() == 1 ? "" : "s") : std::string();
            result.output += prefix + TextEncoding::WideToNarrow (finding.text) + count + "\n";
        }

        for (const RepairChange & change : full.changes)
        {
            result.output += prefix + (change.key == L"unowned" ? "not changed: " : "change: ") + TextEncoding::WideToNarrow (change.text) + "\n";
        }

        chosen = DiskRepair::GetKeys (full);
        chosen.erase (L"unowned");
        plan   = chosen.empty() ? full : model.PlanRepair (volume, chosen);

        if (!full.canRepair && full.before != FileSystemVerdict::Ok && full.before != FileSystemVerdict::None)
        {
            result.output += prefix + "can't repair: " + TextEncoding::WideToNarrow (full.reason) + "\n";
        }
        else if (!chosen.empty() && plan.canRepair)
        {
            repairs.push_back ({ volume, plan });
        }

        if (full.before == FileSystemVerdict::Bad && (chosen.empty() || !plan.canRepair))
        {
            result.exitStatus = DiskCommandResult::kWithComplaints;
        }
    }

    isDone = repairs.empty() || options.disk.listOnly;
    BAIL_OUT_IF (isDone, S_OK);

    //  An image in a container is copied with its host file, and the commit
    //  to the copy writes the containers again around the repaired image.
    target  = target.empty() ? RepairCopy::GetHostCopyPath (io, imagePath) : target + imagePath.substr (RepairCopy::GetHostPath (imagePath).size());
    isTaken = io.Exists (RepairCopy::GetHostPath (target));
    CBRF (!isTaken, result.FailAction (imagePath, "write the repaired copy", fs::path (RepairCopy::GetHostPath (target)).filename().string() + " already exists"));

    hr       = io.ReadAllBytes (RepairCopy::GetHostPath (imagePath), bytes);
    isCopied = SUCCEEDED (hr);
    CBRF (isCopied, result.FailAction (imagePath, "write the repaired copy", "the image couldn't be read"));

    hr       = io.WriteAllBytes (RepairCopy::GetHostPath (target), bytes);
    isCopied = SUCCEEDED (hr);
    CBRF (isCopied, result.FailAction (imagePath, "write the repaired copy", "the copy couldn't be written"));

    for (const auto & [repaired, repair] : repairs)
    {
        hr        = model.WriteRepairTo (io, target, repaired, repair, false, error);
        isWritten = SUCCEEDED (hr);

        if (!isWritten)
        {
            hr = io.Remove (RepairCopy::GetHostPath (target));
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        CBRF (isWritten, result.FailAction (imagePath, "write the repaired copy", TextEncoding::WideToNarrow (error)));
    }

    result.output += imagePath + ": repaired copy written to " + target + "\n";

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommand::FormatVerdict
//
//  "<image>: file system: Bad", then each file-system finding after the
//  image's path.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskRepairCommand::FormatVerdict (const std::string & imagePath, const DiskAnalysis & analysis)
{
    std::string  text = imagePath + ": file system: " + TextEncoding::WideToNarrow (FileSystemCheck::FormatVerdict (analysis.verdict)) + "\n";



    for (const Finding & finding : analysis.findings)
    {
        if (finding.category == FindingCategory::FileSystem)
        {
            text += imagePath + ": " + TextEncoding::WideToNarrow (FindingFormatter::Format (finding)) + "\n";
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommand::AddVerdict
//
////////////////////////////////////////////////////////////////////////////////

void DiskRepairCommand::AddVerdict (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result)
{
    HRESULT          hr      = S_OK;
    vector<Byte>     bytes;
    DiskAnalysis     analysis;
    std::wstring     reason;
    const Finding *  finding = nullptr;



    hr = io.ReadAllBytes (imagePath, bytes);
    CHR (hr);

    hr = FileSystemCheck::Analyze (bytes, imagePath, analysis, reason);
    CHR (hr);

    finding        = FileSystemCheck::GetVerdictFinding (analysis);
    result.output += imagePath + ": file system: " + TextEncoding::WideToNarrow (FileSystemCheck::FormatVerdict (analysis.verdict)) +
                     ((finding != nullptr) ? ", " + TextEncoding::WideToNarrow (FindingFormatter::Format (*finding)) : std::string()) + "\n";

    if (analysis.verdict == FileSystemVerdict::Bad)
    {
        result.exitStatus = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}
