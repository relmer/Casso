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
