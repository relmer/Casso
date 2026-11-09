#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::MakeStandard
//
//  No ranges: every track uses the standard marks and every check.
//
////////////////////////////////////////////////////////////////////////////////

DecodeSettings DecodeSettings::MakeStandard()
{
    return DecodeSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::IsStandard
//
//  True when no range changes anything: no custom mark, the standard marks
//  matched, every check on. The settings chip shows while this is false.
//
////////////////////////////////////////////////////////////////////////////////

bool DecodeSettings::IsStandard() const
{
    bool  isStandard = true;



    for (const DecodeRange & range : m_ranges)
    {
        isStandard = isStandard
                  && range.customMarks.address16.empty()
                  && range.customMarks.address13.empty()
                  && range.customMarks.data.empty()
                  && range.customMarks.addressEpilogue.empty()
                  && range.customMarks.dataEpilogue.empty()
                  && range.matchStandardToo
                  && range.checks.isAddressChecksumOn
                  && range.checks.isDataChecksumOn
                  && range.checks.isEpilogueOn;
    }

    return isStandard;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::FindRange
//
//  The last range that covers the track, so a later range overrides an
//  earlier one where they overlap. Null when none does.
//
////////////////////////////////////////////////////////////////////////////////

const DecodeRange * DecodeSettings::FindRange (int track) const
{
    const DecodeRange *  found = nullptr;



    for (const DecodeRange & range : m_ranges)
    {
        if (track >= range.firstTrack && track <= range.lastTrack)
        {
            found = &range;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::GetMarksForTrack
//
//  Custom marks first, so a custom pattern wins where both would match, then
//  the standard ones unless the range turned them off.
//
////////////////////////////////////////////////////////////////////////////////

FieldMarks DecodeSettings::GetMarksForTrack (int track) const
{
    const DecodeRange *  range    = FindRange (track);
    FieldMarks           standard = FieldMarks::MakeStandard();
    FieldMarks           marks;



    marks = (range != nullptr) ? range->customMarks : FieldMarks();

    if (range == nullptr || range->matchStandardToo)
    {
        marks.address16.insert       (marks.address16.end(),       standard.address16.begin(),       standard.address16.end());
        marks.address13.insert       (marks.address13.end(),       standard.address13.begin(),       standard.address13.end());
        marks.data.insert            (marks.data.end(),            standard.data.begin(),            standard.data.end());
        marks.addressEpilogue.insert (marks.addressEpilogue.end(), standard.addressEpilogue.begin(), standard.addressEpilogue.end());
        marks.dataEpilogue.insert    (marks.dataEpilogue.end(),    standard.dataEpilogue.begin(),    standard.dataEpilogue.end());
    }

    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::GetChecksForTrack
//
////////////////////////////////////////////////////////////////////////////////

DecodeChecks DecodeSettings::GetChecksForTrack (int track) const
{
    const DecodeRange *  range = FindRange (track);



    return (range != nullptr) ? range->checks : DecodeChecks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettings::AddRange
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettings::AddRange (const DecodeRange & range)
{
    m_ranges.push_back (range);
}
