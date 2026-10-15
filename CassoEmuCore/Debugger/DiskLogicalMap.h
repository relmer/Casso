#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogicalMap
//
//  Where DOS 3.3's logical sectors and ProDOS's blocks lie on a 16-sector
//  disk, in the physical sector numbers the address fields have: a DOS
//  sector is one physical sector of its track, and a block two of track
//  block / 8.
//
////////////////////////////////////////////////////////////////////////////////

class DiskLogicalMap
{
public:
    static constexpr int   kSectorsPerTrack = 16;
    static constexpr int   kBlocksPerTrack  = 8;
    static constexpr Word  kBlockCount      = 280;

    //  The physical sector DOS 3.3's logical sector is written under; -1
    //  for a number past 15.
    static int   GetDosPhysical   (int logical);

    //  A block's track and the physical sectors of its first and second
    //  256 bytes.
    static void  GetBlockPhysical (Word block, int & outTrack, int & outFirst, int & outSecond);
};
