#include "Pch.h"

#include "TrackWritability.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability::Evaluate
//
//  Whole-image checks first, because they are free and they settle the question
//  for every track at once.
//
//  A sector image maps every quarter-track position to its own whole track. A
//  bit-stream image installs an explicit map from what was captured, so any
//  position resolving elsewhere means the disk carries data at half- or
//  quarter-track positions. Rewriting it as sectors has nowhere to put that,
//  and the loss would be silent -- exactly what this class exists to prevent.
//
//  Only then is each track judged, and only on what denibblization actually
//  recovered. A track that decoded to a complete standard set can be re-encoded
//  losslessly; a blank one can be written because there is nothing to lose; and
//  a partial one cannot, because the bits that failed to decode are the ones a
//  rewrite would discard.
//
////////////////////////////////////////////////////////////////////////////////

TrackWritability TrackWritability::Evaluate (const DiskImage & img, const SectorDecodeReport & report)
{
    TrackWritability  result;
    int               trackCount = img.GetTrackCount();
    int               track      = 0;
    int               quarter    = 0;
    int               slot       = 0;
    int               expected   = 0;
    bool              mapped     = true;



    // Whole-image: data between whole tracks cannot be represented as sectors.
    for (quarter = 0; mapped && quarter < DiskImage::kQuarterTrackCount; quarter++)
    {
        slot     = img.ResolveQuarterTrack (quarter);
        expected = quarter / DiskImage::kQuarterTracksPerWholeTrack;

        // An unmapped position carries nothing, so it is not evidence either way.
        if (slot >= 0 && slot != expected)
        {
            mapped = false;
        }
    }

    if (!mapped)
    {
        result.m_imageRefusalReason =
            "the image holds data at half- or quarter-track positions, which cannot be "
            "represented as standard sectors";
    }

    result.m_trackWritable.assign ((size_t) ((trackCount > 0) ? trackCount : 0), false);

    for (track = 0; track < trackCount; track++)
    {
        TrackDecodeOutcome  outcome = report.GetOutcome (track);

        result.m_trackWritable[(size_t) track] = outcome == TrackDecodeOutcome::Complete
                                              || outcome == TrackDecodeOutcome::Unformatted;
    }

    return result;
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
