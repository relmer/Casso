#include "Pch.h"

#include "Ui/DiskInspector/DecodeSettingsForm.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsForm::TryBuildRange
//
//  Tracks are whole tracks, 0 to 39, first no later than last. Each mark is
//  two or three bytes; a field left empty adds no custom mark.
//
////////////////////////////////////////////////////////////////////////////////

bool DecodeSettingsForm::TryBuildRange (DecodeRange & outRange, std::wstring & outError) const
{
    static constexpr int  kLastTrack = DiskImage::kMaxTracks - 1;



    DecodeRange  range;
    bool         isValid = true;



    outError.clear();

    if (!InspectorFormat::TryParseDecimal (firstTrack, range.firstTrack) || !InspectorFormat::TryParseDecimal (lastTrack, range.lastTrack)
        || range.firstTrack < 0 || range.lastTrack > kLastTrack || range.firstTrack > range.lastTrack)
    {
        outError = std::format (L"Give whole tracks from 0 to {}, the first no later than the last.", kLastTrack);
        isValid  = false;
    }

    isValid = isValid && TryParseMark (address16,       L"16-sector address prologue", range.customMarks.address16,       outError);
    isValid = isValid && TryParseMark (address13,       L"13-sector address prologue", range.customMarks.address13,       outError);
    isValid = isValid && TryParseMark (data,            L"Data prologue",              range.customMarks.data,            outError);
    isValid = isValid && TryParseMark (addressEpilogue, L"Address epilogue",           range.customMarks.addressEpilogue, outError);
    isValid = isValid && TryParseMark (dataEpilogue,    L"Data epilogue",              range.customMarks.dataEpilogue,    outError);

    range.matchStandardToo           = matchStandardToo;
    range.checks.isAddressChecksumOn = checkAddress;
    range.checks.isDataChecksumOn    = checkData;
    range.checks.isEpilogueOn        = checkEpilogues;

    if (isValid)
    {
        outRange = range;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsForm::MakeFrom
//
//  The dialog opens on the settings in force for the selected track.
//
////////////////////////////////////////////////////////////////////////////////

DecodeSettingsForm DecodeSettingsForm::MakeFrom (const DecodeSettings & settings, int track)
{
    DecodeSettingsForm  form;



    for (const DecodeRange & range : settings.GetRanges())
    {
        if (track >= range.firstTrack && track <= range.lastTrack)
        {
            form.firstTrack       = std::to_wstring (range.firstTrack);
            form.lastTrack        = std::to_wstring (range.lastTrack);
            form.address16        = FormatMarks (range.customMarks.address16);
            form.address13        = FormatMarks (range.customMarks.address13);
            form.data             = FormatMarks (range.customMarks.data);
            form.addressEpilogue  = FormatMarks (range.customMarks.addressEpilogue);
            form.dataEpilogue     = FormatMarks (range.customMarks.dataEpilogue);
            form.matchStandardToo = range.matchStandardToo;
            form.checkAddress     = range.checks.isAddressChecksumOn;
            form.checkData        = range.checks.isDataChecksumOn;
            form.checkEpilogues   = range.checks.isEpilogueOn;
        }
    }

    return form;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsForm::TryParseMark
//
////////////////////////////////////////////////////////////////////////////////

bool DecodeSettingsForm::TryParseMark (const std::wstring & text, LPCWSTR name, vector<DiskMarkPattern> & outPatterns, std::wstring & outError)
{
    DiskMarkPattern  pattern;
    std::string      error;
    bool             isValid = true;



    if (text.find_first_not_of (L' ') != std::wstring::npos)
    {
        isValid = DiskMarkPattern::TryParse (TextEncoding::WideToUtf8 (text), pattern, error);

        if (isValid)
        {
            outPatterns.push_back (pattern);
        }
        else
        {
            outError = std::wstring (name) + L": " + TextEncoding::Utf8ToWide (error);
        }
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsForm::FormatMarks
//
//  The first custom mark, as typed: hex bytes and ?? one space apart.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DecodeSettingsForm::FormatMarks (const vector<DiskMarkPattern> & patterns)
{
    std::wstring  text;
    int           i    = 0;



    if (!patterns.empty())
    {
        for (i = 0; i < patterns[0].GetLength(); i++)
        {
            text += (i > 0 ? L" " : L"") + (patterns[0].IsAny (i) ? std::wstring (L"??") : std::format (L"{:02X}", patterns[0].GetValue (i)));
        }
    }

    return text;
}
