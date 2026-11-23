#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Geometry.h"

class ISectorReadObserver;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33SectorAccess
//
//  Byte-level access to a flat DOS 3.3 sector buffer: its geometry, reads of
//  the buffer that report each sector to the read observer, and writes into a
//  copy of it.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33SectorAccess
{
public:
    //  Stores a reference to the buffer and never modifies it.
    explicit Dos33SectorAccess (const vector<Byte> & sectors);

    void                    SetReadObserver (ISectorReadObserver * observer) { m_readObserver = observer; }

    const Dos33Geometry  & GetGeometry () const { return m_geometry; }
    const vector<Byte>   & GetBuffer   () const { return m_sectors;  }

    uint32_t                ToUnit    (int track, int sector) const { return m_geometry.ToUnit (track, sector); }
    int                     GetTrack  (uint32_t unit)         const { return m_geometry.GetTrack (unit);         }
    int                     GetSector (uint32_t unit)         const { return m_geometry.GetSector (unit);        }

    Byte                    ReadByte    (int track, int sector, size_t offset) const;
    void                    WriteByteAt (vector<Byte> & buffer, int track, int sector, size_t offset, Byte value) const;

    //  The buffer, for a read of one sector of it. Every read of the buffer
    //  goes through here, so the read observer has each sector.
    const vector<Byte>   &  AccessSector (int track, int sector) const;

private:
    const vector<Byte>   &  m_sectors;
    Dos33Geometry           m_geometry;
    ISectorReadObserver  *  m_readObserver = nullptr;
};
