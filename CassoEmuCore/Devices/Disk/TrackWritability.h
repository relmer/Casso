#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackWritability
//
//  Whether an image's tracks can be rewritten as standard sectors without
//  losing what they hold.
//
//  The test is POSITIVE PROOF of standard-ness, never recognition of copy
//  protection. Enumerating protection schemes is a losing game -- the list is
//  never complete, and a scheme it misses is written over silently. Requiring
//  proof instead fails safe: anything not demonstrably standard is refused,
//  whatever the reason.
//
//  Two whole-image checks come first because they are free and decisive. A
//  quarter-track map that resolves any position somewhere other than its whole
//  track means the disk holds data between tracks, which the sector layer has
//  nowhere to put. Track lengths materially off nominal say the same thing more
//  weakly. Only then is each track judged on what it actually decoded.
//
//  Refusal is per track, not per image (excepting the whole-image checks): a
//  protected track elsewhere on the disk does not block a write that never
//  touches it.
//
////////////////////////////////////////////////////////////////////////////////

class TrackWritability
{
public:
    //  Judges the whole image, then each track. The report comes from
    //  denibblizing the same image -- writability is decided by what was
    //  actually recovered, not by a second opinion about the bits.
    static TrackWritability  Evaluate (const DiskImage & img, const SectorDecodeReport & report);

    //  Non-empty when the whole image is unwritable, and why. Set before any
    //  track is examined.
    const std::string &  GetImageRefusalReason () const { return m_imageRefusalReason; }

    bool  IsImageWritable () const { return m_imageRefusalReason.empty(); }

    //  A track qualifies only when it decoded to a complete standard set, or
    //  was blank. Blank is writable on purpose: refusing it would make blank
    //  and newly formatted media unwritable, which is the wrong answer to
    //  "nothing was there".
    bool  IsTrackWritable (int track) const;

    //  Every track the operation needs must be writable. The whole-image
    //  refusal outranks the per-track answer.
    bool  AreTracksWritable (std::span<const int> tracks) const;

private:
    std::string   m_imageRefusalReason;
    vector<bool>  m_trackWritable;
};
