#include "Pch.h"

#include "Devices/Disk/DiskRepairCommand.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
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



    hr     = io.ReadAllBytes (imagePath, bytes);
    isRead = SUCCEEDED (hr);
    CBRF (isRead, result.FailAction (imagePath, "read the image", "the file couldn't be read"));

    hr         = FileSystemCheck::Analyze (bytes, imagePath, analysis, reason);
    isAnalyzed = SUCCEEDED (hr);
    CBRF (isAnalyzed, result.FailAction (imagePath, "check the image", TextEncoding::WideToNarrow (reason)));

    repair.isKeepingUnowned = options.disk.noFreeUnowned;
    plan                    = DiskRepair::Plan (analysis, repair);
    result.output          += FormatVerdict (imagePath, analysis);

    for (const std::wstring & change : plan.changes)
    {
        result.output += imagePath + ": change: " + TextEncoding::WideToNarrow (change) + "\n";
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
