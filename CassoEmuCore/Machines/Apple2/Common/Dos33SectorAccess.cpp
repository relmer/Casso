#include "Pch.h"

#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccess::Dos33SectorAccess
//
////////////////////////////////////////////////////////////////////////////////

Dos33SectorAccess::Dos33SectorAccess (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
    if (!Dos33Geometry::TryDetect (sectors, m_geometry))
    {
        m_geometry = Dos33Geometry::GetForSize (sectors.size());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccess::WriteByteAt
//
//  Out-of-range writes do nothing, mirroring ReadByte. Nothing here addresses a
//  sector it did not first obtain from the allocator or the catalog walk, so
//  this is a backstop rather than a supported way to address the volume.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33SectorAccess::WriteByteAt (vector<Byte> & buffer, int track, int sector, size_t offset, Byte value) const
{
    size_t  at = 0;



    if (track < 0 || track >= m_geometry.tracks
     || sector < 0 || sector >= m_geometry.sectorsPerTrack)
    {
        return;
    }

    at = m_geometry.GetSectorOffset (track, sector) + offset;

    if (at >= buffer.size())
    {
        return;
    }

    buffer[at] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccess::ReadByte
//
//  Out-of-range reads answer zero rather than asserting. Every caller here is
//  following a pointer that came off the disk, and a damaged disk names sectors
//  that do not exist -- that is a finding to report, not a programming error.
//
////////////////////////////////////////////////////////////////////////////////

Byte Dos33SectorAccess::ReadByte (int track, int sector, size_t offset) const
{
    size_t  at = 0;



    if (track < 0 || track >= m_geometry.tracks
     || sector < 0 || sector >= m_geometry.sectorsPerTrack)
    {
        return 0;
    }

    at = m_geometry.GetSectorOffset (track, sector) + offset;

    if (at >= m_sectors.size())
    {
        return 0;
    }

    return AccessSector (track, sector)[at];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccess::AccessSector
//
////////////////////////////////////////////////////////////////////////////////

const vector<Byte> & Dos33SectorAccess::AccessSector (int track, int sector) const
{
    if (m_readObserver != nullptr)
    {
        m_readObserver->OnSectorRead ((size_t) ToUnit (track, sector));
    }

    return m_sectors;
}





