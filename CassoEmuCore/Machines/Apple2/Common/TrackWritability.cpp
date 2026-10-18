#include "Pch.h"

#include "Machines/Apple2/Common/TrackWritability.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability::Evaluate
//
//  Whole-image checks first, because they are free and they settle the question
//  for every track at once.
//
//  A quarter track between whole tracks N and N+1 that is unmapped, or that
//  plays the record of track N or of track N+1, holds nothing of its own: that
//  covers the standard WOZ layout (N-0.25, N and N+0.25 on track N's record),
//  images that also map N+0.5 to a neighbor's record, and the layout Casso
//  writes. A quarter track on any other record holds data of its own between
//  whole tracks, which a sector write would change underneath it, so the whole
//  image is refused and the reason gives that quarter track and its record.
//
//  Only then is each track judged, and only on what denibblization actually
//  recovered. A track that decoded to a complete standard set can be written
//  in place. A partial one cannot, because the bits that failed to decode are
//  what a write could damage, and a blank one cannot either: there is no data
//  field to write into, and no track is laid down to make room.
//
////////////////////////////////////////////////////////////////////////////////

TrackWritability TrackWritability::Evaluate (const DiskImage & img, const SectorDecodeReport & report)
{
    static constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;



    TrackWritability  result;
    int               trackCount = img.GetTrackCount();
    int               track      = 0;
    int               quarter    = 0;
    int               slot       = 0;
    int               below      = 0;
    int               above      = 0;
    bool              mapped     = true;



    for (quarter = 0; mapped && quarter < DiskImage::kQuarterTrackCount; quarter++)
    {
        if (quarter % kPerTrack == 0)
        {
            continue;
        }

        slot   = img.ResolveQuarterTrack (quarter);
        below  = img.ResolveWholeTrack (quarter / kPerTrack);
        above  = img.ResolveWholeTrack (quarter / kPerTrack + 1);
        mapped = slot < 0 || slot == below || slot == above;

        if (!mapped)
        {
            result.m_imageRefusalReason = std::format ("quarter track {} plays track record {}, which holds data of its own between "
                                                       "whole tracks that a sector write would change",
                                                       FormatQuarterTrack (quarter), slot);
        }
    }

    result.m_trackWritable.assign ((size_t) ((trackCount > 0) ? trackCount : 0), false);

    for (track = 0; track < trackCount; track++)
    {
        TrackDecodeOutcome  outcome = report.GetOutcome (track);

        result.m_trackWritable[(size_t) track] = outcome == TrackDecodeOutcome::Complete;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability::FormatQuarterTrack
//
//  A quarter track as the track number a user reads: 2, 2.25, 2.5 or 2.75.
//
////////////////////////////////////////////////////////////////////////////////

std::string TrackWritability::FormatQuarterTrack (int quarterTrack)
{
    static constexpr const char *  kpszFractions[] = { "", ".25", ".5", ".75" };



    return std::to_string (quarterTrack / DiskImage::kQuarterTracksPerWholeTrack)
         + kpszFractions[quarterTrack % DiskImage::kQuarterTracksPerWholeTrack];
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability::IsTrackWritable
//
//  A track outside the image is not writable: there is nothing there to write
//  to, and answering "yes" would let a caller address space that does not exist.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackWritability::IsTrackWritable (int track) const
{
    bool  inRange  = track >= 0 && (size_t) track < m_trackWritable.size();
    bool  imageOk  = m_imageRefusalReason.empty();



    return imageOk && inRange && m_trackWritable[(size_t) track];
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability::AreTracksWritable
//
//  Every track the write needs, and no others. A protected track the operation
//  never touches is not this write's problem.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackWritability::AreTracksWritable (std::span<const int> tracks) const
{
    bool  writable = m_imageRefusalReason.empty();
    int   track    = 0;



    for (size_t i = 0; writable && i < tracks.size(); i++)
    {
        track    = tracks[i];
        writable = IsTrackWritable (track);
    }

    return writable;
}
