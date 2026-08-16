#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport
//
//  What a volume's catalog actually references, as opposed to what its free map
//  claims. One mechanism with four consumers:
//
//      Delete       -- what may safely be freed is what one file uniquely owns.
//      Listing      -- the damage a read reports comes from here.
//      Allocation   -- whether the free map can be trusted at all.
//      Pre-commit   -- every computed write is checked against this BEFORE it
//                      is committed, so a write verifies its own output.
//
//  The fourth is the one that changes the feature's character. A write path
//  that never inspects what it produced is how a decoder shipped returning
//  success over a buffer it had partly zeroed; checking before committing is
//  the structural answer to that whole class rather than to that one instance.
//
//  Addressing is in "units" -- sectors for DOS 3.3, blocks for ProDOS -- so the
//  same report serves both without either filesystem's geometry leaking in.
//  Owners are opaque identifiers the caller assigns, typically a catalog entry
//  index; the report never needs to know what a file is.
//
////////////////////////////////////////////////////////////////////////////////

class VolumeIntegrityReport
{
public:
    //  No owner. Distinct from owner 0, which is a real entry.
    static constexpr uint16_t  kNoOwner = 0xFFFF;

    void  Reset (uint32_t unitCount);

    //  Records that `owner` references `unit`. Called once per unit per entry
    //  as each file's chain is walked; a second claim on one unit is exactly
    //  the cross-link this report exists to find.
    void  AddClaim (uint32_t unit, uint16_t owner);

    //  What the volume's own free map says about a unit.
    void  SetAllocatedInFreeMap (uint32_t unit, bool allocated);

    //  An entry whose chain could not be walked to its end -- including one
    //  that stopped because it hit a traversal bound. Recorded rather than
    //  followed, because a walk that gave up did not reach the end.
    void  MarkChainUnfollowable (uint16_t owner);

    //  False when some catalog entries could not be read. This BOUNDS every
    //  other answer here: a file that could not be read claims nothing
    //  observable, so units it shares look unclaimed.
    void  SetCatalogFullyParsed (bool parsed);

    //  Computes the derived sets. Call once after all claims are recorded.
    void  Finish ();

    //  The safety rule delete depends on: free a unit only when this entry is
    //  its sole claimant. Anything else is either shared or not ours.
    bool  IsUniquelyOwnedBy (uint32_t unit, uint16_t owner) const;

    bool  IsClaimed             (uint32_t unit) const;
    bool  IsAllocatedInFreeMap  (uint32_t unit) const;

    const vector<uint32_t> &  GetCrossLinked           () const { return m_crossLinked; }
    const vector<uint32_t> &  GetAllocatedButUnclaimed () const { return m_allocatedButUnclaimed; }
    const vector<uint32_t> &  GetClaimedButFree        () const { return m_claimedButFree; }
    const vector<uint16_t> &  GetUnfollowableChains    () const { return m_unfollowableChains; }

    bool  IsCatalogFullyParsed () const { return m_catalogFullyParsed; }

    //  Nothing cross-linked, nothing unfollowable, and the free map agrees with
    //  the catalog. A computed write that is not clean must not be committed.
    bool  IsClean () const { return m_isClean; }

    uint32_t  GetUnitCount () const { return (uint32_t) m_claimCount.size(); }

private:
    //  Claim COUNT plus first owner, rather than a list per unit. Cross-linking
    //  is rare and the question asked of it is "exactly one, and is it mine?",
    //  which two small values answer without allocating per unit.
    vector<uint16_t>  m_claimCount;
    vector<uint16_t>  m_firstOwner;
    vector<bool>      m_allocated;
    vector<uint32_t>  m_crossLinked;
    vector<uint32_t>  m_allocatedButUnclaimed;
    vector<uint32_t>  m_claimedButFree;
    vector<uint16_t>  m_unfollowableChains;
    bool              m_catalogFullyParsed = true;
    bool              m_isClean            = false;
};
