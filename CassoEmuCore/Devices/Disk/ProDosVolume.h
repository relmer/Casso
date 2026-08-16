#pragma once

#include "Pch.h"

#include "IVolume.h"
#include "ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume
//
//  A ProDOS filesystem over a flat sector buffer.
//
//  Unlike the DOS 3.3 side, a reader already existed here -- ProDosReader,
//  written to pull PRODOS and BASIC.SYSTEM out of a downloaded master for
//  bootable-disk creation. It is reused for extraction rather than duplicated.
//  What it does not provide is enumeration or a per-file block list, both of
//  which the listing and the integrity pass need, so those are walked here.
//
//  Addressing for the integrity pass is the block number itself, giving 280
//  units on a 5.25-inch volume.
//
//  Subdirectories are not yet traversed. A path naming one is refused rather
//  than reduced to its last component, so the capability can be filled in later
//  without any caller having been silently given the wrong file in the meantime.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolume : public IVolume
{
public:
    explicit ProDosVolume (const vector<Byte> & sectors);

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    //  The ProDOS file types this feature names on the command line. The full
    //  set is 256 values; these are the ones a developer places or extracts.
    static constexpr Byte  kTypeText   = 0x04;
    static constexpr Byte  kTypeBinary = 0x06;
    static constexpr Byte  kTypeBasic  = 0xFC;
    static constexpr Byte  kTypeSystem = 0xFF;

    //  Write-enable bit of the access byte. Clear means the file is locked.
    static constexpr Byte  kAccessWriteEnable = 0x02;

private:
    //  One directory record, plus where it was found.
    struct RawEntry
    {
        int       dirBlock    = 0;
        size_t    entryOffset = 0;
        Byte      storage     = 0;
        Byte      fileType    = 0;
        Byte      access      = 0;
        Word      keyPointer  = 0;
        Word      blocksUsed  = 0;
        Word      auxType     = 0;
        uint32_t  eof         = 0;
        string    name;
    };

    Byte  ReadByte (int block, size_t offset) const;
    Word  ReadWord (int block, size_t offset) const;

    //  Walks the volume directory chain under a guard. Damage is appended
    //  rather than thrown.
    void  CollectEntries (vector<RawEntry>    & outEntries,
                          vector<std::string> & outDamage,
                          bool                & outFullyParsed) const;

    //  Every block one entry occupies, index blocks included. False when the
    //  structure could not be walked, which the caller reports as an
    //  unfollowable chain rather than as a short file.
    bool  CollectFileBlocks (const RawEntry    & entry,
                             vector<uint32_t>  & outBlocks,
                             ChainWalkGuard    & guard) const;

    const vector<Byte> &  m_sectors;
};
