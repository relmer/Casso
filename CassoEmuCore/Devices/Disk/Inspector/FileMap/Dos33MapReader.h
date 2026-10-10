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
//  bitmap for what the volume marks used.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33MapReader
{
public:
    static constexpr int  kVtocTrack = 17;

    //  Whether track 17 sector 0 is a VTOC, and the size it gives.
    static bool  IsFound (const SectorSource & source, int & outTracks, int & outSectors);
    static void  Read    (const SectorSource & source, FileMap & outMap);

private:
    static void          ReadEntry      (FileMapWriter & writer, const SectorSource & source, const Byte * entry);
    static void          WalkLists      (FileMapWriter & writer, const SectorSource & source, int file, int track, int sector);
    static std::wstring  ReadName       (const Byte * name, int length);
    static std::wstring  GetTypeLetter  (Byte type);
    static int           GetCell        (int track, int sector) { return track * SectorSource::kSectors + sector; }
};
