#include "Pch.h"

#include "Devices/Disk/Stamp/StampedTrackMarks.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Machines/Apple2/Common/TrackMarks.h"
#include "Machines/Apple2/Common/WozMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::TryLoadStamp
//
//  Whether the image's metadata has a stamp this build reads.
//
////////////////////////////////////////////////////////////////////////////////

bool StampedTrackMarks::TryLoadStamp (const WozMetadata & meta, FormatStamp & outStamp)
{
    return FormatStampChunk::GetStamp (meta, outStamp) == StampState::Readable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::TryGetTrackMarks
//
//  The marks of the whole track's entry. False when the stamp has no entry
//  for the track, or the entry doesn't list a three-nibble address prolog. A
//  track listed as both encodings has its address prologs in both lists.
//
////////////////////////////////////////////////////////////////////////////////

bool StampedTrackMarks::TryGetTrackMarks (const FormatStamp & stamp, int track, TrackMarks & outMarks)
{
    constexpr size_t    kProlog  = 3;
    int                 position = track * DiskImage::kQuarterTracksPerWholeTrack;
    const StampTrack  * entry    = nullptr;



    outMarks = TrackMarks();

    for (const StampTrack & candidate : stamp.tracks)
    {
        entry = (entry == nullptr && candidate.position == position) ? &candidate : entry;
    }

    if (entry == nullptr)
    {
        return false;
    }

    for (const vector<Byte> & prolog : entry->addressPrologs)
    {
        if (prolog.size() != kProlog)
        {
            continue;
        }

        TrackMarks::Prolog  nibbles = { prolog[0], prolog[1], prolog[2] };

        if (entry->encoding != StampEncoding::Thirteen)
        {
            outMarks.address16.push_back (nibbles);
        }

        if (entry->encoding == StampEncoding::Thirteen || entry->encoding == StampEncoding::Both)
        {
            outMarks.address13.push_back (nibbles);
        }
    }

    for (const vector<Byte> & prolog : entry->dataPrologs)
    {
        if (prolog.size() == kProlog)
        {
            outMarks.data.push_back ({ prolog[0], prolog[1], prolog[2] });
        }
    }

    return !outMarks.address16.empty() || !outMarks.address13.empty();
}
