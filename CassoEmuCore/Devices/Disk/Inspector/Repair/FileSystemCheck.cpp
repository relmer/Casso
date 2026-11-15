#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck::FormatVerdict
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileSystemCheck::FormatVerdict (FileSystemVerdict verdict)
{
    std::wstring  text;



    switch (verdict)
    {
        case FileSystemVerdict::Ok:      text = L"OK";                      break;
        case FileSystemVerdict::Suspect: text = L"Suspect";                 break;
        case FileSystemVerdict::Bad:     text = L"Bad";                     break;
        case FileSystemVerdict::None:    text = L"No standard file system"; break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck::GetFirstFinding
//
////////////////////////////////////////////////////////////////////////////////

const Finding * FileSystemCheck::GetFirstFinding (const DiskAnalysis & analysis, FindingWeight weight)
{
    const Finding *  first = nullptr;



    for (const Finding & finding : analysis.findings)
    {
        if (first == nullptr && finding.category == FindingCategory::FileSystem && finding.weight == weight)
        {
            first = &finding;
        }
    }

    return first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck::GetVerdictFinding
//
////////////////////////////////////////////////////////////////////////////////

const Finding * FileSystemCheck::GetVerdictFinding (const DiskAnalysis & analysis)
{
    const Finding *  finding = nullptr;



    switch (analysis.verdict)
    {
        case FileSystemVerdict::Bad:     finding = GetFirstFinding (analysis, FindingWeight::Damage);  break;
        case FileSystemVerdict::Suspect: finding = GetFirstFinding (analysis, FindingWeight::Suspect); break;
        default:                                                                                        break;
    }

    return finding;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck::IsCopyProtected
//
//  A nonstandard track, 13-sector fields beside 16-sector ones, a field mark,
//  epilogue, track or volume that a standard format doesn't write, an
//  address field with no data field, a sector repeated or numbered out of
//  range, nibbles outside the table, a track record between or across whole
//  tracks; or random bits or an odd track length on at least a quarter of
//  the tracks with data. A standard capture has random bits on a track or
//  two at most.
//
////////////////////////////////////////////////////////////////////////////////

bool FileSystemCheck::IsCopyProtected (const DiskAnalysis & analysis)
{
    static constexpr int  kOddShare = 4;



    std::set<int>  odd;
    bool           isProtected = analysis.summary.nonstandardTracks > 0 || analysis.summary.format == DiskFormatClass::ThirteenAndSixteen;



    for (const Finding & finding : analysis.findings)
    {
        switch (finding.kind)
        {
            case FindingKind::AddressTrackDiffers:
            case FindingKind::VolumeDiffers:
            case FindingKind::AddressEpilogueNonstandard:
            case FindingKind::DataEpilogueNonstandard:
            case FindingKind::AddressWithoutData:
            case FindingKind::DataWithoutAddress:
            case FindingKind::NibblesOutsideTable:
            case FindingKind::SectorRepeated:
            case FindingKind::SectorNumberOutOfRange:
            case FindingKind::StrayFieldMark:
            case FindingKind::ThirteenSectorFields:
            case FindingKind::RecordBetweenTracks:
            case FindingKind::RecordWithoutWholeTrack:
            case FindingKind::RecordAcrossWholeTracks:
                isProtected = true;
                break;

            case FindingKind::RandomBitsOnFormattedTrack:
            case FindingKind::TrackLengthDiffers:
                odd.insert (finding.quarterTrack);
                break;

            default:
                break;
        }
    }

    return isProtected || (!odd.empty() && static_cast<int> (odd.size()) * kOddShare >= analysis.summary.tracksWithData);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck::Analyze
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileSystemCheck::Analyze (const vector<Byte> & bytes, const std::string & utf8Path, DiskAnalysis & outAnalysis, std::wstring & outReason)
{
    HRESULT                          hr   = S_OK;
    std::shared_ptr<const DiskCopy>  copy;



    hr = InspectorImageLoader::LoadBytes (bytes, utf8Path, true, copy, outReason);
    CHR (hr);

    DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), outAnalysis);

Error:
    return hr;
}
