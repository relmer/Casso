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

    //  Which units this entry claims. Answers the first clause of the
    //  requirement directly, rather than making a caller re-walk every chain to
    //  recover something the pass already saw.
    const std::vector<uint32_t> &  GetClaimsOf (uint16_t owner) const;

    //  Every entry claiming a unit, not merely how many. A damage report that
    //  can only say "two files share this sector" without naming them leaves
    //  the user unable to act on it.
    const std::vector<uint16_t> &  GetClaimantsOf (uint32_t unit) const;

    const std::vector<uint32_t> &  GetCrossLinked           () const { return m_crossLinked; }
    const std::vector<uint32_t> &  GetAllocatedButUnclaimed () const { return m_allocatedButUnclaimed; }
    const std::vector<uint32_t> &  GetClaimedButFree        () const { return m_claimedButFree; }
    const std::vector<uint16_t> &  GetUnfollowableChains    () const { return m_unfollowableChains; }

    bool  IsCatalogFullyParsed () const { return m_catalogFullyParsed; }

    //  Nothing cross-linked, nothing unfollowable, and the free map agrees with
    //  the catalog. A computed write that is not clean must not be committed.
    bool  IsClean () const { return m_isClean; }

    uint32_t  GetUnitCount () const { return (uint32_t) m_claimantsByUnit.size(); }

private:
    //  Claims are held BOTH ways -- by unit and by owner -- because the
    //  requirement asks both questions and neither slicing derives the other
    //  without re-walking every chain.
    //
    //  An earlier version kept only a count plus the first owner per unit. That
    //  satisfies delete, which asks only "exactly one, and is it mine?", but it
    //  cannot say which units a given file claims, and can only count
    //  cross-links rather than name the files sharing a sector -- which is
    //  precisely what a damage report exists to tell someone.
    //
    //  Space is not a reason to prefer the leaner form, at any size this
    //  project can encounter. The total is bounded by allocated units, not by
    //  how they are indexed, and the ceiling is the FORMAT rather than any
    //  drive: ProDOS block pointers are 16 bits, so 65,535 blocks -- 32 MB -- is
    //  the largest volume that can exist. Fully allocated at that maximum, both
    //  slices together are a few hundred kilobytes, transiently. No volume this
    //  project can mount makes the argument real, larger media included.
    //
    //  The axis worth watching is TIME. This pass is O(volume) and runs before
    //  every computed write; at a few tens of thousands of units that is
    //  microseconds per command-line invocation. It only becomes interesting if
    //  a caller runs it on something frequent, such as a live UI refresh, and
    //  that is a caching question rather than a structural one.
    std::vector<std::vector<uint16_t>>  m_claimantsByUnit;
    std::vector<std::vector<uint32_t>>  m_claimsByOwner;
    std::vector<bool>                   m_allocated;
    std::vector<uint16_t>               m_emptyClaimants;
    std::vector<uint32_t>               m_emptyClaims;
    std::vector<uint32_t>               m_crossLinked;
    std::vector<uint32_t>               m_allocatedButUnclaimed;
    std::vector<uint32_t>               m_claimedButFree;
    std::vector<uint16_t>               m_unfollowableChains;
    bool                                m_catalogFullyParsed    = true;
    bool                                m_isClean               = false;
};
