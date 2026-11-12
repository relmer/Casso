#include "Pch.h"

#include "Devices/Disk/DiskInspectCommand.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand::Run
//
//  The image loads as a drive mounts it, so a file a drive can't mount fails
//  with the reason a mount fails with.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectCommand::Run (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result)
{
    HRESULT                          hr     = S_OK;
    std::vector<Byte>                bytes;
    std::shared_ptr<const DiskCopy>  copy;
    std::wstring                     reason;
    DiskAnalysis                     analysis;



    hr = io.ReadAllBytes (imagePath, bytes);
    CBRF (SUCCEEDED (hr), result.FailAction (imagePath, "read the image", "the file couldn't be read"));

    hr = InspectorImageLoader::LoadBytes (bytes, imagePath, true, copy, reason);
    CBRF (SUCCEEDED (hr), result.FailAction (imagePath, "inspect the image", TextEncoding::WideToNarrow (reason)));

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
