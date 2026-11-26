#include "Pch.h"

#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScanner::Dos33IntegrityScanner
//
////////////////////////////////////////////////////////////////////////////////

Dos33IntegrityScanner::Dos33IntegrityScanner (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScanner::ClaimVolumeStructures
//
//  The tracks a formatted volume reserves for itself: 0-2, where DOS lives on a
//  bootable disk and which every 1980s formatter kept allocated on a data disk
//  too, and 17, which holds the VTOC and the catalog.
//
//  Sixty-four sectors, named by nothing in the catalog. Crediting them to the
//  volume is what makes the free map and the claim map agree on a healthy disk;
//  without it every DOS 3.3 volume reports those sixty-four as space allocated
//  to nobody, which is indistinguishable from the leak a bad delete leaves.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33IntegrityScanner::ClaimVolumeStructures (VolumeIntegrityReport & outReport) const
{
    int  track  = 0;
    int  sector = 0;



    for (track = 0; track < m_access.GetGeometry().tracks; track++)
    {
        bool  reserved = track < Dos33Skeleton::kDosImageTracks
                      || track == Dos33Skeleton::kVtocTrack;

        if (!reserved)
        {
            continue;
        }

        for (sector = 0; sector < m_access.GetGeometry().sectorsPerTrack; sector++)
        {
            outReport.AddClaim (m_access.ToUnit (track, sector), VolumeIntegrityReport::kVolumeOwner);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScanner::BuildIntegrityReport
//
//  Every catalog entry's chain walked into the claim map, then compared against
//  the VTOC's own free bitmap. The volume's own tracks -- the boot area and the
//  catalog track -- are claimed too, under a reserved owner: they are as
//  allocated as any file's sectors, and a report that ignored them would call
//  every volume inconsistent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33IntegrityScanner::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    bool                       fullyParsed = true;
    uint16_t                   owner       = 0;
    int                        track       = 0;
    int                        sector      = 0;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);

    outReport.Reset (m_access.GetGeometry().GetUnitCount());

    m_catalog.CollectEntries (entries, damage, fullyParsed);
    outReport.SetCatalogFullyParsed (fullyParsed);

    ClaimVolumeStructures (outReport);

    for (owner = 0; owner < (uint16_t) entries.size(); owner++)
    {
        ChainWalkGuard    guard (m_access.GetGeometry().GetUnitCount());
        vector<uint32_t>  units;
        vector<uint32_t>  listUnits;
        bool              walked = m_chain.CollectDataSectors (entries[owner], units, listUnits, guard);

        // The list sectors themselves are part of the file's footprint. An
        // entry occupying no sectors contributes none of either: the walk
        // returns before it starts, so a decorative entry's $7F/$7F pointer is
        // never treated as a sector it owns.
        for (uint32_t unit : listUnits)
        {
            outReport.AddClaim (unit, owner);
        }

        for (uint32_t unit : units)
        {
            outReport.AddClaim (unit, owner);
        }

        if (!walked)
        {
            outReport.MarkChainUnfollowable (owner);
        }
    }

    // The free bitmap's view. A CLEAR bit means the sector is in use.
    for (track = 0; track < m_access.GetGeometry().tracks; track++)
    {
        for (sector = 0; sector < m_access.GetGeometry().sectorsPerTrack; sector++)
        {
            bool  isFree = m_freeMap.IsFree (track, sector);

            outReport.SetAllocatedInFreeMap (m_access.ToUnit (track, sector), !isFree);
        }
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33IntegrityScanner::HandBackVerifiedResult
//
//  A write that never inspects what it produced is how a decoder shipped
//  returning success over a buffer it had partly zeroed. Every computed result
//  here is read back through the same integrity pass a user's disk goes
//  through, refused if the edit made the volume worse, and handed over only
//  once it survives that.
//
//  WORSE, not imperfect. Asking whether the result is CLEAN would refuse two
//  correct outcomes: a delete that declines to free a cross-linked sector
//  leaves space allocated with nothing claiming it, which is exactly what the
//  leak rule requires, and a volume that already carried a disagreement would
//  become permanently uneditable -- on the degraded disks this feature exists
//  to recover. The comparison is against the input, on the sets that matter.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33IntegrityScanner::HandBackVerifiedResult (
    const VolumeIntegrityReport  & before,
    const vector<Byte>           & result,
    vector<Byte>                 & outBuffer)
{
    HRESULT                hr   = S_OK;
    Dos33SectorAccess      access (result);
    Dos33IntegrityScanner  scanner (access);
    bool                   safe = false;
    VolumeIntegrityReport  after;



    hr = scanner.BuildIntegrityReport (after);
    CHRA (hr);

    safe = after.IsSafeToCommitAfter (before);
    CBRAEx (safe, E_UNEXPECTED);

    outBuffer = result;

Error:
    return hr;
}





