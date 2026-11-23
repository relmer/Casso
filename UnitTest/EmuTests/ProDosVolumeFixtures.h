#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures
//
//  Synthetic ProDOS volumes and payloads for the ProDOS tests, the directory
//  entry layout, and reads of a block's bytes and words.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolumeFixtures
{
public:

    static constexpr size_t  kKeyBlockEntry = 0x04;

    //  Where the volume directory starts. The skeleton keeps its own copy
    //  private, and the tests that predate this one spell it as a literal.
    static constexpr int     kDirKeyBlock   = 2;

    //  Directory geometry, restated here because the skeleton keeps its own
    //  copies private and a test that reads bytes has to name offsets.
    static constexpr size_t  kEntryLength      = 0x27;
    static constexpr size_t  kEntOffName       = 0x01;
    static constexpr size_t  kEntOffKeyPointer = 0x11;
    static constexpr size_t  kEntOffBlocksUsed = 0x13;
    static constexpr size_t  kEntOffEof        = 0x15;
    static constexpr size_t  kEntOffAccess     = 0x1E;

    static constexpr size_t  kBlockBytes       = 512;
    static constexpr size_t  kPointersPerIndex = 256;

    static constexpr Byte    kStorageSeedling = 0x10;

    //  Blocks 0-1 boot, 2-5 the volume directory, 6 the bitmap.
    static constexpr uint32_t  kFreeOnBlankVolume = 280 - 7;

    //  A payload that needs a THIRD index level's worth of pointers -- 258 data
    //  blocks, so the master index holds two entries and the second one is
    //  reached only by a writer that grew past a sapling. 257 * 512 + 100.
    static constexpr size_t  kTreePayloadBytes = 131'684;
    static constexpr size_t  kTreeDataBlocks   = 258;
    static constexpr size_t  kTreeTotalBlocks  = 261;   // + two index + one master

    static vector<Byte>  MakeVolume                        (const std::string & name = "NEWDISK");
    static Byte          At                                (const vector<Byte> & buffer, int block, size_t offset);
    static Word          WordAt                            (const vector<Byte> & buffer, int block, size_t offset);
    static vector<Byte>  MakePattern                       (size_t count);
    static Byte          BlockStamp                        (size_t blockIndex);
    static FilePayload   MakeBinaryPayload                 (size_t length, Word loadAddress);
    static size_t        EntryOffset                       (int slot);
    static Word          KeyPointerOf                      (const vector<Byte> & vol, int dirBlock, int slot);
    static Word          IndexPointerAt                    (const vector<Byte> & vol, int indexBlock, size_t slot);
    static void          AssertEditLeftTheVolumeConsistent (const vector<Byte> & before, const vector<Byte> & after);
    static vector<Byte>  MakeVolumeWithCrossLinkedFiles    (vector<Byte> & outSurvivorBytes, Word & outSharedBlock);
    static void          MarkFree                          (vector<Byte> & vol, uint32_t block, bool isFree);
    static vector<Byte>  MakeVolumeWithSubdirectory        ();
};
