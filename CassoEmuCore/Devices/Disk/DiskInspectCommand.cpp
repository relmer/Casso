#include "Pch.h"

#include "Devices/Disk/DiskInspectCommand.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::Run
//
//  An image block mode opens is analyzed as block mode does. Any other image
//  loads as a drive mounts it, so a file a drive can't mount fails with the
//  reason a mount fails with.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectCommand::Run (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result)
{
    HRESULT                          hr      = S_OK;
    std::vector<Byte>                bytes;
    std::shared_ptr<const DiskCopy>  copy;
    std::wstring                     reason;
    DiskAnalysis                     analysis;
    bool                             read    = false;
    bool                             loaded  = false;
    bool                             isBlock = BlockImageFile::IsBlockImage (io, imagePath);



    if (isBlock)
    {
        RunBlocks (io, imagePath, result);
    }

    BAIL_OUT_IF (isBlock, S_OK);

    hr   = io.ReadAllBytes (imagePath, bytes);
    read = SUCCEEDED (hr);
    CBRF (read, result.FailAction (imagePath, "read the image", "the file couldn't be read"));

    hr     = InspectorImageLoader::LoadBytes (bytes, imagePath, true, copy, reason);
    loaded = SUCCEEDED (hr);
    CBRF (loaded, result.FailAction (imagePath, "inspect the image", TextEncoding::WideToNarrow (reason)));

    DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), analysis);

    result.output += FormatSummary (analysis);

    for (const Finding & finding : analysis.findings)
    {
        result.output += FormatFinding (finding);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::RunBlocks
//
//  The image's block count and each volume's file system, name and verdict
//  on the first line, then a line for each finding.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectCommand::RunBlocks (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result)
{
    HRESULT                          hr      = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    std::wstring                     error;
    BlockInspectorModel              model;
    std::string                      summary;
    std::string                      lines;
    std::string                      units;
    FileSystemVerdict                verdict = FileSystemVerdict::None;
    bool                             isOpen  = false;
    int                              volume  = 0;



    hr     = BlockImageFile::Open (io, imagePath, file, error);
    isOpen = SUCCEEDED (hr);
    CBRF (isOpen, result.FailAction (imagePath, "inspect the image", TextEncoding::WideToNarrow (error)));

    hr     = model.Open (std::move (file));
    isOpen = SUCCEEDED (hr);
    CBRF (isOpen, result.FailAction (imagePath, "inspect the image", "its blocks couldn't be read"));

    summary = std::format ("{} blocks", model.GetBlockCount());

    for (volume = 0; volume < model.GetVolumeCount(); volume++)
    {
        const BlockMap  & map = model.GetVolumeMap (volume);

        verdict  = BlockRepair::GetVerdict (map);
        summary += "; " + FormatVolume (model, volume);
        summary += (verdict != FileSystemVerdict::None) ? ": " + TextEncoding::WideToNarrow (FileSystemCheck::FormatVerdict (verdict)) : std::string();

        for (const BlockFinding & finding : GetBlockFindings (map))
        {
            units  = finding.units.empty() ? std::string ("-") : FormatUnits (model, volume, finding.units);
            lines += std::format ("{}\t{}\t{}\n", volume + 1, units, TextEncoding::WideToNarrow (finding.text));
        }
    }

    result.output += summary + "\n" + lines;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::GetBlockFindings
//
////////////////////////////////////////////////////////////////////////////////

vector<DiskInspectCommand::BlockFinding> DiskInspectCommand::GetBlockFindings (const BlockMap & map)
{
    std::map<SectorRole, vector<uint32_t>>  damage;
    vector<BlockFinding>                     findings;
    uint32_t                                 unit = 0;



    for (unit = 0; unit < map.GetBlockCount(); unit++)
    {
        SectorRole  role = map.blocks[unit].role;

        if (role == SectorRole::AllocatedUnowned || role == SectorRole::OwnedMarkedFree || role == SectorRole::CrossLinked)
        {
            damage[role].push_back (unit);
        }
    }

    for (const auto & [role, units] : damage)
    {
        findings.push_back ({ BlockMap::GetRoleName (role), units, true });
    }

    for (const BlockCount & count : map.counts)
    {
        findings.push_back ({ count.finding, { count.keyBlock } });
    }

    if (map.hasBrokenChains)
    {
        findings.push_back ({ L"A file's blocks can't be followed to its end", {} });
    }

    if (!map.isCatalogComplete)
    {
        findings.push_back ({ L"Some of the volume's directories can't be read", {} });
    }

    return findings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::FormatVolume
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskInspectCommand::FormatVolume (const BlockInspectorModel & model, int volume)
{
    const BlockMap  & map  = model.GetVolumeMap (volume);
    std::string       text = std::format ("volume {}, ", volume + 1);



    if (map.fileSystem == BlockFileSystem::None)
    {
        text += "no standard file system";
    }
    else
    {
        text += TextEncoding::WideToNarrow (BlockBrowser::GetFileSystemName (map.fileSystem));
    }

    if (!map.volumeName.empty())
    {
        text += " \"" + TextEncoding::WideToNarrow (map.volumeName) + "\"";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::FormatUnits
//
//  A run is units in a row, and for a DOS volume, on one track.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskInspectCommand::FormatUnits (const BlockInspectorModel & model, int volume, const vector<uint32_t> & units)
{
    bool         isSectors = model.IsSectorVolume (volume);
    uint32_t     perTrack  = isSectors ? (uint32_t) std::max (1, model.GetSectorsPerTrack (volume)) : 1;
    uint32_t     first     = model.GetVolumeFirst (volume);
    std::string  text;
    size_t       i         = 0;
    size_t       j         = 0;



    for (i = 0; i < units.size(); i = j)
    {
        j = i + 1;

        while (j < units.size() && units[j] == units[j - 1] + 1 && (!isSectors || units[j] / perTrack == units[i] / perTrack))
        {
            j++;
        }

        text += text.empty() ? "" : ",";

        if (isSectors)
        {
            text += std::format ("{}/{}", units[i] / perTrack, units[i] % perTrack) + (j - i > 1 ? std::format ("-{}", units[j - 1] % perTrack) : std::string());
        }
        else
        {
            text += std::format ("{}", first + units[i]) + (j - i > 1 ? std::format ("-{}", first + units[j - 1]) : std::string());
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::FormatSummary
//
//  As the inspector's status bar has it: the format, then each count that
//  isn't zero.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskInspectCommand::FormatSummary (const DiskAnalysis & analysis)
{
    const DiskSummary  & summary = analysis.summary;
    std::string          text;
    auto                 add     = [&text] (const std::string & part) { text += (text.empty() ? "" : ", ") + part; };



    switch (summary.format)
    {
        case DiskFormatClass::NothingRecorded:    add ("nothing recorded");  break;
        case DiskFormatClass::ThirteenAndSixteen: add ("13- and 16-sector"); break;
        case DiskFormatClass::Sixteen:            add ("16-sector");         break;
        case DiskFormatClass::Thirteen:           add ("13-sector");         break;
        case DiskFormatClass::Nonstandard:        add ("nonstandard");       break;
        case DiskFormatClass::Unformatted:        add ("unformatted");       break;
        default:                                  add ("damaged");           break;
    }

    if (summary.tracksWithData > 0)
    {
        add (std::format ("{} tracks with data", summary.tracksWithData));
    }

    if (summary.sectorsFound > 0)
    {
        add (std::format ("{} of {} sectors good", summary.sectorsGood, summary.sectorsFound));
    }

    if (summary.badSectors > 0)
    {
        add (std::format ("{} bad sectors", summary.badSectors));
    }

    if (summary.nonstandardTracks > 0)
    {
        add (std::format ("{} nonstandard tracks", summary.nonstandardTracks));
    }

    if (summary.damagedTracks > 0)
    {
        add (std::format ("{} damaged tracks", summary.damagedTracks));
    }

    if (summary.unformattedTracks > 0)
    {
        add (std::format ("{} unformatted tracks", summary.unformattedTracks));
    }

    if (summary.commonVolume >= 0)
    {
        add (std::format ("volume {}", summary.commonVolume));
    }

    return text + "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::FormatFinding
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskInspectCommand::FormatFinding (const Finding & finding)
{
    std::string  track  = (finding.quarterTrack >= 0) ? std::format ("{:g}", finding.quarterTrack / 4.0) : "-";
    std::string  sector = (finding.sector >= 0)       ? std::format ("${:X}", finding.sector)            : "-";
    std::string  cell   = finding.hasCell             ? std::to_string (finding.cell)                    : "-";



    return std::format ("{}\t{}\t{}\t{}\t{}\n", track, TextEncoding::WideToNarrow (FindingFormatter::FormatCategory (finding.category)),
                        sector, cell, TextEncoding::WideToNarrow (FindingFormatter::Format (finding)));
}
