#pragma once

#include "Pch.h"

struct FormatStamp;
struct TrackMarks;
struct WozMetadata;





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks
//
//  The marks a format stamp lists for a whole track, as a decoder matches
//  them: the address prologs in the 16-sector or the 13-sector list by the
//  track's encoding, and the data prologs.
//
////////////////////////////////////////////////////////////////////////////////

class StampedTrackMarks
{
public:
    static bool  TryGetTrackMarks (const FormatStamp & stamp, int track, TrackMarks & outMarks);
    static bool  TryLoadStamp     (const WozMetadata & meta, FormatStamp & outStamp);
};
