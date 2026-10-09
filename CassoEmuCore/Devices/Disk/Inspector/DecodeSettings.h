#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FieldLocator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeRange / DecodeSettings
//
//  What the analyzer matches and checks, for a range of whole tracks. Custom
//  marks, each nibble a value or ?? for any, are matched before the standard
//  ones, and the standard ones too unless "Match standard marks too" is off.
//  Turning a check off makes the analyzer report the fields it covers as not
//  checked rather than good or bad. A later range overrides an earlier one
//  for the tracks they share. Standard by default, and kept only while the
//  window shows the disk.
//
////////////////////////////////////////////////////////////////////////////////

struct DecodeChecks
{
    bool  isAddressChecksumOn = true;
    bool  isDataChecksumOn    = true;
    bool  isEpilogueOn        = true;
};


struct DecodeRange
{
    int           firstTrack       = 0;
    int           lastTrack        = DiskImage::kMaxTracks - 1;
    FieldMarks    customMarks;
    bool          matchStandardToo = true;
    DecodeChecks  checks;
};


class DecodeSettings
{
public:
    static DecodeSettings  MakeStandard();

    bool          IsStandard         () const;
    FieldMarks    GetMarksForTrack   (int track) const;
    DecodeChecks  GetChecksForTrack  (int track) const;
    void          AddRange           (const DecodeRange & range);

    const vector<DecodeRange> &  GetRanges () const { return m_ranges; }

private:
    const DecodeRange *  FindRange (int track) const;

    vector<DecodeRange>  m_ranges;
};
