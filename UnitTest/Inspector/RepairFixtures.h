#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RepairFixture / RepairFixtures
//
//  Made-up 140 KB images with bookkeeping damage planted in them, each with
//  the image before the damage, for the verdict and Repair:
//
//  - DOS 3.3, from FileMapTestImages' volume with DOS's header, a boot
//    sector and DOS on tracks 0 to 2: a bitmap that marks a file's sector
//    free and an unused sector used, a catalog whose last sector links back
//    to its second, a zeroed VTOC header, a next track to allocate off the
//    disk, a file whose catalog count differs, a cross-link, and a chain off
//    the disk.
//  - ProDOS: a bitmap that marks a file's block free and an unused block
//    used, and a volume directory whose file count differs.
//  - Apple Pascal: a directory whose file count differs.
//
//  The DOS images are in DOS order and the ProDOS and Pascal images in
//  ProDOS order. The cross-link and the chain off the disk can't be repaired.
//
////////////////////////////////////////////////////////////////////////////////

struct RepairFixture
{
    std::wstring  name;
    vector<Byte>  before;
    vector<Byte>  damaged;
    bool          isProDosOrder = false;
    bool          canRepair     = true;
};


class RepairFixtures
{
public:
    static constexpr int  kUnusedTrack  = 10;
    static constexpr int  kUnusedSector = 5;
    static constexpr int  kUnusedBlock  = 100;

    static vector<Byte>  MakeDos33  ();
    static vector<Byte>  MakeProDos ();
    static vector<Byte>  MakePascal ();

    static RepairFixture  MakeDos33Bitmap          ();
    static RepairFixture  MakeDos33CatalogLoop     ();
    static RepairFixture  MakeDos33HeaderZeroed    ();
    static RepairFixture  MakeDos33NextTrackOff    ();
    static RepairFixture  MakeDos33CountDiffers    ();
    static RepairFixture  MakeDos33CrossLink       ();
    static RepairFixture  MakeDos33ChainOffDisk    ();
    static RepairFixture  MakeProDosBitmapAndCount ();
    static RepairFixture  MakePascalCount          ();

    //  Every fixture, the ones that can be repaired first.
    static vector<RepairFixture>  MakeAll ();

    //  The 256 bytes of a DOS sector of an image in DOS order.
    static Byte *  GetDosSector (vector<Byte> & image, int track, int sector);

private:
    static void  MarkDos33 (vector<Byte> & image, int track, int sector, bool isUsed);
};
