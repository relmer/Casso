#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackDecodeOutcome
//
//  What a track's bit stream yielded. Two independent questions decide it,
//  and neither substitutes for the other:
//
//      Were any address fields found?   Unformatted vs. everything else.
//      Was coverage complete?           Complete vs. Partial.
//
//  A track whose address fields are present but whose data fields all fail is
//  Partial -- damaged -- not blank. Zeros for an Unformatted track are correct
//  and intended for a sector image; zeros for a Partial one are data that could
//  not be read, and no consumer may confuse the two.
//
////////////////////////////////////////////////////////////////////////////////

enum class TrackDecodeOutcome
{
    Unformatted,   // No address field anywhere in a full revolution.
    Complete,      // All 16 logical sectors filled, each exactly once.
    Partial,       // Address fields present, coverage incomplete or duplicated.
};





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeReport
//
//  Per-track result of denibblization, so a caller can tell a clean track from
//  an undecodable one instead of seeing an undecodable track as zeros.
//
//  Completeness is a COVERAGE property, not a loop property: "the decoder ran
//  and did not fail" does not mean the track is intact, because a sector number
//  outside the valid range and two sectors claiming the same number both leave
//  a logical slot unfilled without anything failing. One 16-bit mask decides
//  the outcome for all such cases at once, including any nobody anticipated.
//
//  The mask is tested for "every slot filled EXACTLY once" rather than merely
//  "every slot filled". The stronger test costs one comparison and does not
//  depend on how many times the decode loop is allowed to iterate -- a bound
//  that is a plausible thing to change, and the moment it does, a duplicate can
//  coexist with a full mask and a fullness test stops meaning what it says.
//
////////////////////////////////////////////////////////////////////////////////

class SectorDecodeReport
{
public:
    //  All 16 logical sectors present in the coverage mask.
    static constexpr Word  kFullCoverage = 0xFFFF;

    void  Reset       (int trackCount);
    void  SetOutcome  (int track, TrackDecodeOutcome outcome, Word coverage, bool duplicated);

    int                 GetTrackCount () const;
    TrackDecodeOutcome  GetOutcome    (int track) const;
    Word                GetCoverage   (int track) const;
    bool                IsDuplicated  (int track) const;

    //  True iff this sector of the FLAT OUTPUT BUFFER was recovered. Indexed by
    //  output slot, not by the number the address field carried -- consumers
    //  hold the buffer, and the interleave between the two is the track
    //  layer's business.
    bool  IsSectorRecovered (int track, int sector) const;

    //  Any track is Partial -- the image cannot be losslessly rewritten and a
    //  caller that discards this distinction reintroduces the defect.
    bool  HasDataLoss () const;

    //  Logical sectors left uncovered on Partial tracks ONLY. An Unformatted
    //  track contributes nothing because nothing was lost.
    int   GetUnrecoveredCount () const;

private:
    std::vector<TrackDecodeOutcome>  m_outcome;
    std::vector<Word>                m_coverage;
    std::vector<bool>                m_duplicated;
};
