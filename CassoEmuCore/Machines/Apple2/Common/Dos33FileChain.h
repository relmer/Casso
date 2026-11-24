#pragma once

#include "Pch.h"

class ChainWalkGuard;
class Dos33SectorAccess;
struct Dos33CatalogEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChain
//
//  A file's track/sector lists: walking them to find its sectors, and laying
//  a new file's lists and data sectors out in a copy of the buffer.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33FileChain
{
public:
    explicit Dos33FileChain (const Dos33SectorAccess & access);

    //  Gathers a file's data sectors by walking its track/sector list under a
    //  guard, so a corrupted chain cannot loop forever. The list sectors are
    //  returned separately. A read uses only the data sectors; the integrity
    //  pass, and through it delete, counts the list sectors too, since they are
    //  as much the file's footprint as its bytes.
    bool  CollectDataSectors (const Dos33CatalogEntry  & entry,
                              vector<uint32_t>         & outUnits,
                              vector<uint32_t>         & outListUnits,
                              ChainWalkGuard           & guard) const;

    void  ZeroUnits          (vector<Byte> & buffer, const vector<uint32_t> & units) const;

    void  PlaceFile          (vector<Byte>            & buffer,
                              const vector<uint32_t>  & units,
                              size_t                    dataSectorCount,
                              const vector<Byte>      & stored) const;

    //  Where a list's first track/sector pair is, and how many pairs one list
    //  sector has.
    static constexpr size_t  kTsOffFirstPair = 0x0C;
    static constexpr int     kTsPairsPerList = 122;

private:
    static constexpr size_t  kTsOffNextTrack  = 0x01;
    static constexpr size_t  kTsOffNextSector = 0x02;

    //  The file-relative index of the list's first data sector, so a walk that
    //  starts partway along the chain has its position.
    static constexpr size_t  kTsOffSectorOffsetLo = 0x05;
    static constexpr size_t  kTsOffSectorOffsetHi = 0x06;

    const Dos33SectorAccess  & m_access;
};
