#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMapWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33MapReader
//
//  A DOS 3.3 volume (FR-084, FR-086, FR-087): found by its VTOC on track 17
//  sector 0, then its catalog chain, each entry's track/sector lists in
//  order with their data sectors and the holes of a sparse file, deleted
//  entries by the track kept in their name's last byte, and the VTOC's
//  bitmap for what the volume marks used. The VTOC's header and each
//  catalog entry are checked, and a catalog too odd to be DOS's isn't
//  followed.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33MapReader
{
public:
    static constexpr int   kVtocTrack       = 17;
    static constexpr int   kCatalogSector   = 15;
    static constexpr int   kHeaderBytes     = 0x38;
    static constexpr int   kNextTrack       = 0x30;
    static constexpr int   kDirection       = 0x31;
    static constexpr int   kLink            = 0x01;
    static constexpr int   kInitLinkedMin   = 4;
    static constexpr Byte  kStandardVolume  = 254;

    //  Whether track 17 sector 0 is a VTOC, and the size it gives.
    static bool  IsFound      (const SectorSource & source, int & outTracks, int & outSectors);

    //  Whether a disk with no VTOC header has a catalog on track 17 linked
    //  as DOS's INIT links it, with at least four sectors of believable
    //  entries, and DOS on tracks 0 to 2.
    static bool  IsHeaderLost (const SectorSource & source);
    static void  Read         (const SectorSource & source, FileMap & outMap);

    //  The VTOC header DOS 3.3's INIT writes, bytes $00 to $37, with the
    //  next track to allocate set to track 17 moving up.
    static void  MakeStandardHeader (Byte volume, Byte * outVtoc);

    //  The catalog sectors on track 17 from sector $F down while each is
    //  readable and believable, and how many of them are linked as INIT
    //  links them: each to the one below, the last to none.
    static void  GetInitCatalog (const SectorSource & source, vector<int> & outSectors, int & outLinked);

    static bool  IsBelievableSector (const Byte * sector);
    static bool  IsKnownType        (Byte type);
    static int   GetCell            (int track, int sector) { return track * SectorSource::kSectors + sector; }

private:
    struct Entry
    {
        int           cell   = -1;
        int           offset = 0;
        const Byte *  bytes  = nullptr;
    };

    static void          CheckHeader       (FileMapWriter & writer, const Byte * vtoc);
    static bool          CheckEntries      (FileMapWriter & writer, const vector<Entry> & entries);
    static void          ReadEntry         (FileMapWriter & writer, const SectorSource & source, const Entry & entry);
    static void          WalkLists         (FileMapWriter & writer, const SectorSource & source, int file, int track, int sector);
    static void          CheckCount        (FileMapWriter & writer, int file, const Entry & entry);
    static void          CheckLength       (FileMapWriter & writer, const SectorSource & source, const MappedFile & file, Byte type);
    static bool          IsBelievableEntry (const Byte * entry);
    static bool          IsNameMixed       (const Byte * name);
    static std::wstring  ReadName          (const Byte * name, int length);
    static std::wstring  GetTypeLetter     (Byte type);
};
