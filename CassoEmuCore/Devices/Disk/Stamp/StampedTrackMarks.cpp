#include "Pch.h"

#include "Devices/Disk/Stamp/StampedTrackMarks.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
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





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::ToDecodeSettings
//
////////////////////////////////////////////////////////////////////////////////

void StampedTrackMarks::ToDecodeSettings (const FormatStamp & stamp, DecodeSettings & outSettings)
{
    DecodeRange  range;
    bool         isOpen = false;



    outSettings = DecodeSettings::MakeStandard();

    for (int track = 0; track <= DiskImage::kMaxTracks; track++)
    {
        FieldMarks  marks;
        bool        hasMarks = track < DiskImage::kMaxTracks && TryGetCustomMarks (stamp, track, marks);

        if (isOpen && hasMarks && track == range.lastTrack + 1 && IsSameMarks (marks, range.customMarks))
        {
            range.lastTrack = track;
            continue;
        }

        if (isOpen)
        {
            outSettings.AddRange (range);
            isOpen = false;
        }

        if (hasMarks)
        {
            range             = DecodeRange();
            range.firstTrack  = track;
            range.lastTrack   = track;
            range.customMarks = marks;
            range.isFromStamp = true;
            isOpen            = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::TryGetCustomMarks
//
//  The track's stamped marks that the standard ones don't match. False when
//  there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool StampedTrackMarks::TryGetCustomMarks (const FormatStamp & stamp, int track, FieldMarks & outMarks)
{
    FieldMarks  standard = FieldMarks::MakeStandard();
    TrackMarks  marks;



    outMarks = FieldMarks();

    if (!TryGetTrackMarks (stamp, track, marks))
    {
        return false;
    }

    AddCustomMarks (marks.address16, standard.address16, outMarks.address16);
    AddCustomMarks (marks.address13, standard.address13, outMarks.address13);
    AddCustomMarks (marks.data,      standard.data,      outMarks.data);

    return !outMarks.address16.empty() || !outMarks.address13.empty() || !outMarks.data.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::AddCustomMarks
//
//  Each prolog as an exact mark, but for the ones in `skip` and the ones the
//  list has.
//
////////////////////////////////////////////////////////////////////////////////

void StampedTrackMarks::AddCustomMarks (const vector<TrackMarks::Prolog> & prologs, const vector<DiskMarkPattern> & skip, vector<DiskMarkPattern> & ioList)
{
    for (const TrackMarks::Prolog & prolog : prologs)
    {
        DiskMarkPattern  mark = DiskMarkPattern::MakeExact (prolog[0], prolog[1], prolog[2]);

        if (std::ranges::find (skip, mark) == skip.end() && std::ranges::find (ioList, mark) == ioList.end())
        {
            ioList.push_back (mark);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampedTrackMarks::IsSameMarks
//
////////////////////////////////////////////////////////////////////////////////

bool StampedTrackMarks::IsSameMarks (const FieldMarks & a, const FieldMarks & b)
{
    return a.address16 == b.address16 && a.address13 == b.address13 && a.data == b.data;
}





