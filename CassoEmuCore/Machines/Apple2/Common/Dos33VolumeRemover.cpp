#include "Pch.h"

#include "Machines/Apple2/Common/Dos33VolumeRemover.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/FilePath.h"
#include "Utils.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemover::Dos33VolumeRemover
//
////////////////////////////////////////////////////////////////////////////////

Dos33VolumeRemover::Dos33VolumeRemover (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemover::AppendDeleteWarnings
//
////////////////////////////////////////////////////////////////////////////////

void Dos33VolumeRemover::AppendDeleteWarnings (DeleteOutcome & inOutOutcome)
{
    size_t  leaked = inOutOutcome.leakedUnits.size();



    if (leaked > 0)
    {
        inOutOutcome.warnings.push_back (
            std::to_string (leaked) + " "
          + Utils::GetSingularOrPluralForm ((long long) leaked, "sector", "sectors")
          + " this file referenced are claimed by another catalog entry"
            " as well, and were left allocated rather than freed");
    }

    if (inOutOutcome.chainWasDamaged)
    {
        inOutOutcome.warnings.push_back (
            "this file's sector chain could not be followed to its end, so only"
            " the sectors the walk reached were considered");
    }

    if (!inOutOutcome.catalogFullyParsed)
    {
        inOutOutcome.warnings.push_back (
            "the catalog did not parse completely, so an entry that could not be"
            " read claims nothing this pass can see, and a sector it shares with the"
            " deleted file may have been freed while that entry still uses it");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemover::Delete
//
//  The interface form. Everything it decided is available from the overload
//  below; this one discards it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33VolumeRemover::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemover::Delete
//
//  Removes a catalog entry and returns ONLY the sectors the integrity report
//  shows this entry alone claims.
//
//  That restriction is the whole point. Freeing a sector a second file also
//  references does not fail: the delete succeeds, the volume looks healthy, and
//  the other file is destroyed the next time anything allocates -- possibly
//  weeks later, with nothing connecting the two events. Space left allocated
//  with nothing owning it is recoverable at any time; a sector handed out from
//  under a live file is not. So everything not uniquely owned is reported as
//  leaked and left exactly where it is.
//
//  A FILE WHOSE CHAIN IS DAMAGED IS STILL DELETABLE. Refusing would strand the
//  volume -- the entry could never be removed and its space never reclaimed --
//  and the developer this serves is working with precisely those disks. What the
//  walk reached is freed on the same unique-ownership terms; what it could not
//  reach was never observed and is not touched.
//
//  An entry occupying no sectors claims nothing, so nothing is freed for it. The
//  discriminator is the entry's own sector count, which is what the chain walk
//  already uses; twenty of the sixty-three entries on Merlin's own disk are of
//  that shape and following their $7F/$7F pointer would be inventing a chain.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33VolumeRemover::Delete (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    bool                       single      = path.IsSingleComponent();
    bool                       isLocked    = false;
    bool                       fullyParsed = true;
    uint16_t                   owner       = 0;
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    vector<Byte>               result;
    VolumeIntegrityReport      report;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, path, owner);
    CHR (hr);

    isLocked = (entries[owner].typeByte & Dos33Volume::kLockedBit) != 0;
    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    outOutcome                    = DeleteOutcome();
    outOutcome.catalogFullyParsed = report.IsCatalogFullyParsed();

    result = m_access.GetBuffer();

    // The report walks the same catalog in the same order over the same buffer,
    // so an owner index names the same entry on both sides.
    for (uint32_t unit : report.GetClaimsOf (owner))
    {
        bool  mineAlone = report.IsUniquelyOwnedBy (unit, owner);

        if (mineAlone)
        {
            m_freeMap.SetFreeInBitmap (result, m_access.GetTrack (unit), m_access.GetSector (unit), true);
            outOutcome.freedUnits.push_back (unit);
        }
        else
        {
            outOutcome.leakedUnits.push_back (unit);
        }
    }

    for (uint16_t broken : report.GetUnfollowableChains())
    {
        if (broken == owner)
        {
            outOutcome.chainWasDamaged = true;
        }
    }

    m_catalog.WriteTombstone (result, entries[owner]);

    AppendDeleteWarnings (outOutcome);

    hr = Dos33IntegrityScanner::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





