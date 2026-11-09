#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapTestImages
//
//  Made-up 140 KB volumes for the file map's tests (SC-014, SC-015), each
//  laid out by hand so a test knows where every file is:
//
//  - DOS 3.3: HELLO (three data sectors), BIG (three track/sector lists),
//    SPARSE (a text file with a hole), an unused catalog slot, and OLD
//    (deleted).
//  - ProDOS: SEED, SPARSE.TREE (a tree file with holes), FORKED (both forks),
//    an entry that uses no blocks, GONE (deleted), and DIR1/DIR2/DEEP two
//    subdirectories deep.
//  - Apple Pascal: SYSTEM.PASCAL, a gap, NOTES.TEXT, and a .BAD file.
//  - CP/M: 0:PIP.COM (two extents), 0:SPARSE.DAT (a hole), 5:NOTES.TXT, a
//    deleted OLD.TXT, and the system entry of user 31.
//
//  DOS 3.3 and CP/M are built in DOS order, ProDOS and Pascal in ProDOS
//  order; each converts to the other order to test both.
//
////////////////////////////////////////////////////////////////////////////////

class FileMapTestImages
{
public:
    static constexpr int  kTracks         = 35;
    static constexpr int  kSectors        = 16;
    static constexpr int  kSectorBytes    = 256;
    static constexpr int  kBlockBytes     = 512;
    static constexpr int  kImageBytes     = kTracks * kSectors * kSectorBytes;

    static vector<Byte>  MakeDos33  ();
    static vector<Byte>  MakeProDos ();
    static vector<Byte>  MakePascal ();
    static vector<Byte>  MakeCpm    ();

    static Byte *        GetDosSector (vector<Byte> & image, int track, int logical);
    static Byte *        GetBlock     (vector<Byte> & image, int block);
    static Byte *        GetCpmSector (vector<Byte> & dosOrder, int track, int cpmSector);

    static vector<Byte>  ToDosOrder    (const vector<Byte> & proDosOrder);
    static vector<Byte>  ToProDosOrder (const vector<Byte> & dosOrder);

    //  Nibblizes a DOS-order or ProDOS-order image and analyzes every track.
    static void  Analyze (const vector<Byte> & image, bool isProDosOrder, DiskAnalysis & out);

    //  Makes a physical sector of a whole track bad (its data checksum
    //  fails) or missing (no field for it), in the analysis alone.
    static void  BreakSector (DiskAnalysis & inOut, int track, int physical, bool isMissing);

private:
    static void  SetWord (Byte * at, int value);
    static void  SetDosName (Byte * at, const char * name);
};
