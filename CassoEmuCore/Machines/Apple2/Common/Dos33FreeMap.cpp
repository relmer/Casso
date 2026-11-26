#include "Pch.h"

#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::Dos33FreeMap
//
////////////////////////////////////////////////////////////////////////////////

Dos33FreeMap::Dos33FreeMap (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::IsFree
//
//  The bitmap in the volume's buffer, read through the sector access so the
//  read observer has the VTOC sector.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33FreeMap::IsFree (int track, int sector) const
{
    return IsFreeInBitmap (m_access.AccessSector (Dos33Skeleton::kVtocTrack, 0), track, sector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::CountFreeUnits
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Dos33FreeMap::CountFreeUnits() const
{
    const Dos33Geometry  & geometry  = m_access.GetGeometry();
    int                    track     = 0;
    int                    sector    = 0;
    uint32_t               freeUnits = 0;



    for (track = 0; track < geometry.tracks; track++)
    {
        for (sector = 0; sector < geometry.sectorsPerTrack; sector++)
        {
            bool  isFree = IsFree (track, sector);

            freeUnits += isFree ? 1u : 0u;
        }
    }

    return freeUnits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::GetAllocationTrack
//
//  The order free sectors are handed out in: track 18 outward to the rim, then
//  track 16 inward to track 0. That is what the VTOC's own allocation hint and
//  direction byte describe, and it keeps a file's sectors near its catalog.
//
//  The catalog track itself is never produced, so it cannot be handed out even
//  if its free bitmap were wrong.
//
////////////////////////////////////////////////////////////////////////////////

int Dos33FreeMap::GetAllocationTrack (int index) const
{
    int  outwardCount = m_access.GetGeometry().tracks - Dos33Skeleton::kVtocTrack - 1;



    if (index < outwardCount)
    {
        return Dos33Skeleton::kVtocTrack + 1 + index;
    }

    return Dos33Skeleton::kVtocTrack - 1 - (index - outwardCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::IsFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33FreeMap::IsFreeInBitmap (const vector<Byte> & buffer, int track, int sector) const
{
    size_t  byteAt = 0;



    if (track < 0 || track >= m_access.GetGeometry().tracks
     || sector < 0 || sector >= m_access.GetGeometry().sectorsPerTrack)
    {
        return false;
    }

    byteAt = m_access.GetGeometry().GetBitmapByteOffset (track, sector);

    if (byteAt >= buffer.size())
    {
        return false;
    }

    return (buffer[byteAt] & m_access.GetGeometry().GetBitmapMask (sector)) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::SetFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

void Dos33FreeMap::SetFreeInBitmap (vector<Byte> & buffer, int track, int sector, bool isFree) const
{
    size_t  byteAt = 0;
    Byte    mask   = 0;



    if (track < 0 || track >= m_access.GetGeometry().tracks
     || sector < 0 || sector >= m_access.GetGeometry().sectorsPerTrack)
    {
        return;
    }

    byteAt = m_access.GetGeometry().GetBitmapByteOffset (track, sector);
    mask   = m_access.GetGeometry().GetBitmapMask (sector);

    if (byteAt >= buffer.size())
    {
        return;
    }

    if (isFree)
    {
        buffer[byteAt] = (Byte) (buffer[byteAt] | mask);
    }
    else
    {
        buffer[byteAt] = (Byte) (buffer[byteAt] & ~mask);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap::TryAllocateUnits
//
//  Hands out exactly `count` sectors or none at all, so a refusal leaves nothing
//  half-reserved.
//
//  A sector must be free in the VTOC bitmap AND unclaimed by the catalog. The
//  second condition is the one that matters: a bitmap calling a sector free
//  while a catalog entry still points at it is the disagreement that hands the
//  next file someone else's live data, and it is exactly the state a previous
//  bad delete leaves behind. Skipping those sectors costs a little space and
//  leaves the disagreement for the integrity report to name.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33FreeMap::TryAllocateUnits (
    const VolumeIntegrityReport  & report,
    size_t                         count,
    vector<uint32_t>             & outUnits) const
{
    int     allocatableTracks = m_access.GetGeometry().tracks - 1;
    int     slot              = 0;
    size_t  taken             = 0;



    outUnits.clear();

    for (slot = 0; slot < allocatableTracks && taken < count; slot++)
    {
        int  track  = GetAllocationTrack (slot);
        int  sector = 0;

        for (sector = m_access.GetGeometry().sectorsPerTrack - 1; sector >= 0 && taken < count; sector--)
        {
            uint32_t  unit    = m_access.ToUnit (track, sector);
            bool      isFree  = IsFree (track, sector);
            bool      claimed = report.IsClaimed (unit);

            if (isFree && !claimed)
            {
                outUnits.push_back (unit);
                taken++;
            }
        }
    }

    return taken == count;
}





