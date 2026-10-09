#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsForm
//
//  What the "Decode settings" dialog holds, as text and switches, and the
//  decode range it makes (FR-019): a track range, the five marks with ?? for
//  any byte and an empty field for none, whether the standard marks are
//  matched too, and the three checks. Kept apart from the dialog so the
//  parsing can be tested.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodeSettingsForm
{
    std::wstring  firstTrack       = L"0";
    std::wstring  lastTrack        = L"39";
    std::wstring  address16;
    std::wstring  address13;
    std::wstring  data;
    std::wstring  addressEpilogue;
    std::wstring  dataEpilogue;
    bool          matchStandardToo = true;
    bool          checkAddress     = true;
    bool          checkData        = true;
    bool          checkEpilogues   = true;

    //  The range the form describes, or false with the reason in outError.
    bool  TryBuildRange (DecodeRange & outRange, std::wstring & outError) const;

    //  The form for the range that covers a track, or the standard one.
    static DecodeSettingsForm  MakeFrom (const DecodeSettings & settings, int track);

private:
    static bool          TryParseMark (const std::wstring & text, LPCWSTR name, vector<DiskMarkPattern> & outPatterns, std::wstring & outError);
    static std::wstring  FormatMarks  (const vector<DiskMarkPattern> & patterns);
};
