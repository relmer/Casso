#pragma once

#include "Pch.h"

#include "DiskMarkPattern.h"

class DecodeSettings;
struct FieldMarks;
struct FormatStamp;
struct TrackMarks;
struct WozMetadata;





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks
//
//  The marks a format stamp lists for a whole track, as a decoder matches
//  them: the address prologs in the 16-sector or the 13-sector list by the
//  track's encoding, and the data prologs. As decode settings, each run of
//  whole tracks with the same marks other than the standard ones is a range
//  with those marks that matches the standard ones too.
//
////////////////////////////////////////////////////////////////////////////////

class StampedTrackMarks
{
public:
    static bool  TryGetTrackMarks (const FormatStamp & stamp, int track, TrackMarks & outMarks);
    static bool  TryLoadStamp     (const WozMetadata & meta, FormatStamp & outStamp);
    static void  ToDecodeSettings (const FormatStamp & stamp, DecodeSettings & outSettings);

private:
    static bool  TryGetCustomMarks (const FormatStamp & stamp, int track, FieldMarks & outMarks);
    static bool  IsSameMarks       (const FieldMarks & a, const FieldMarks & b);
    static void  AddCustomMarks    (const vector<std::array<Byte, 3>> & prologs, const vector<DiskMarkPattern> & skip, vector<DiskMarkPattern> & ioList);
};
