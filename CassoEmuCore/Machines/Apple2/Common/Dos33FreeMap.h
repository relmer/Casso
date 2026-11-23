#pragma once

#include "Pch.h"

class Dos33SectorAccess;
class VolumeIntegrityReport;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FreeMap
//
//  The free sector bitmap in the VTOC, and allocation from it.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33FreeMap
{
public:
    explicit Dos33FreeMap (const Dos33SectorAccess & access);

    //  Whether the volume's bitmap marks this sector free.
    bool      IsFree           (int track, int sector) const;
    uint32_t  CountFreeUnits   () const;

    void      SetFreeInBitmap  (vector<Byte> & buffer, int track, int sector, bool isFree) const;

    //  Allocates exactly `count` sectors, or none.
    bool      TryAllocateUnits (const VolumeIntegrityReport  & report,
                                size_t                         count,
                                vector<uint32_t>             & outUnits) const;

private:
    //  Free bitmap: four bytes per track, of which two are used. Byte 0 has
    //  sectors 15..8 in bits 7..0 and byte 1 has sectors 7..0; a SET bit
    //  means free.
    static constexpr size_t  kBitmapBytesPerTrack = 4;
    static constexpr int     kBitmapSplitSector   = 8;

    bool  IsFreeInBitmap     (const vector<Byte> & buffer, int track, int sector) const;

    //  The allocation order: outward from the catalog track first, then inward,
    //  the order of the DOS allocation hint. The catalog track is never returned.
    int   GetAllocationTrack (int index) const;

    const Dos33SectorAccess  & m_access;
};
