#include "Pch.h"

#include "VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::Reset
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::Reset (uint32_t unitCount)
{
    m_claimantsByUnit.assign (unitCount, vector<uint16_t>());
    m_allocated.assign       (unitCount, false);

    m_claimsByOwner.clear();

    m_crossLinked.clear();
    m_allocatedButUnclaimed.clear();
    m_claimedButFree.clear();
    m_unfollowableChains.clear();

    m_catalogFullyParsed = true;
    m_isClean            = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::AddClaim
//
//  A unit outside the volume is ignored rather than recorded: it cannot be
//  freed, cannot be written, and has no slot to count against. The caller's
//  chain walk reports the impossible pointer through its own guard.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::AddClaim (uint32_t unit, uint16_t owner)
{
    bool  inRange = unit < m_claimantsByUnit.size();



    if (!inRange)
    {
        return;
    }

    m_claimantsByUnit[unit].push_back (owner);

    if (owner >= m_claimsByOwner.size())
    {
        m_claimsByOwner.resize ((size_t) owner + 1);
    }

    m_claimsByOwner[owner].push_back (unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::SetAllocatedInFreeMap
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::SetAllocatedInFreeMap (uint32_t unit, bool allocated)
{
    bool  inRange = unit < m_allocated.size();



    if (inRange)
    {
        m_allocated[unit] = allocated;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::MarkChainUnfollowable
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::MarkChainUnfollowable (uint16_t owner)
{
    m_unfollowableChains.push_back (owner);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::SetCatalogFullyParsed
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::SetCatalogFullyParsed (bool parsed)
{
    m_catalogFullyParsed = parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::Finish
//
//  Derives the three disagreement sets in one pass.
//
//  Note what is NOT derived: nothing here decides that an allocated-but-
//  unclaimed unit is free. That set is either space already leaked by an
//  earlier bug or the data of a file the catalog could not show us, and there
//  is no way to tell which. Reclaiming it would be a guess whose downside is
//  overwriting someone's file, so it is reported and left alone.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeIntegrityReport::Finish()
{
    uint32_t  unit    = 0;
    uint32_t  count   = (uint32_t) m_claimantsByUnit.size();
    bool      agrees  = true;



    for (unit = 0; unit < count; unit++)
    {
        bool  claimed = !m_claimantsByUnit[unit].empty();

        if (m_claimantsByUnit[unit].size() > 1)
        {
            m_crossLinked.push_back (unit);
        }

        if (m_allocated[unit] && !claimed)
        {
            m_allocatedButUnclaimed.push_back (unit);
        }

        if (claimed && !m_allocated[unit])
        {
            m_claimedButFree.push_back (unit);
        }
    }

    // A unit the catalog references but the free map calls free is the
    // dangerous direction: the next allocation would hand out live data.
    agrees = m_crossLinked.empty()
          && m_claimedButFree.empty()
          && m_allocatedButUnclaimed.empty()
          && m_unfollowableChains.empty();

    m_isClean = agrees && m_catalogFullyParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::IsUniquelyOwnedBy
//
//  The rule delete rests on. "Exactly one claimant, and it is this one" is
//  deliberately stricter than "this one claims it": a unit two files both
//  reference must not be freed when either is deleted, because freeing it
//  hands the survivor's data to the next allocation.
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeIntegrityReport::IsUniquelyOwnedBy (uint32_t unit, uint16_t owner) const
{
    bool  inRange = unit < m_claimantsByUnit.size();



    if (!inRange)
    {
        return false;
    }

    return m_claimantsByUnit[unit].size() == 1 && m_claimantsByUnit[unit][0] == owner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::IsClaimed
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeIntegrityReport::IsClaimed (uint32_t unit) const
{
    bool  inRange = unit < m_claimantsByUnit.size();



    return inRange && !m_claimantsByUnit[unit].empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::IsAllocatedInFreeMap
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeIntegrityReport::IsAllocatedInFreeMap (uint32_t unit) const
{
    bool  inRange = unit < m_allocated.size();



    return inRange && m_allocated[unit];
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::GetClaimsOf
//
//  Which units one entry references. An entry that claimed nothing -- or an
//  owner the pass never saw -- reads as an empty set rather than as an error,
//  because "this file occupies no space" is a legitimate answer for an empty
//  file and an unknown owner has nothing to report either way.
//
////////////////////////////////////////////////////////////////////////////////

const vector<uint32_t> & VolumeIntegrityReport::GetClaimsOf (uint16_t owner) const
{
    bool  known = owner < m_claimsByOwner.size();



    if (!known)
    {
        return m_emptyClaims;
    }

    return m_claimsByOwner[owner];
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityReport::GetClaimantsOf
//
//  Every entry claiming a unit. Naming them is the difference between a damage
//  report a user can act on and one that only says a problem exists.
//
////////////////////////////////////////////////////////////////////////////////

const vector<uint16_t> & VolumeIntegrityReport::GetClaimantsOf (uint32_t unit) const
{
    bool  inRange = unit < m_claimantsByUnit.size();



    if (!inRange)
    {
        return m_emptyClaimants;
    }

    return m_claimantsByUnit[unit];
}
