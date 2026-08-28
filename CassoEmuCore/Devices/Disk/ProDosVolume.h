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
//  Placement and removal are implemented here rather than by extending
//  ProDosFileWriter, which stays what it was built to be: the bootable-disk
//  helper that installs PRODOS and BASIC.SYSTEM into a freshly formatted
//  buffer. It writes THROUGH the caller's buffer, allocates from the bitmap
//  alone, and asserts on bad arguments -- all correct for a caller supplying
//  its own constants, and all wrong for one relaying what a user typed. What
//  keeps the two from drifting is that files written here are read back through
//  ProDosReader, which neither of them shares.
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

    //  Delete, plus an account of what it freed, what it refused to free, and
    //  the conditions that bound the answer. The interface form forwards here
    //  and throws the account away; anything reporting to a user wants it.
    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    //  The self-check every computed write and delete runs over its own output,
    //  and the ONLY way a computed buffer reaches a caller. Refuses a buffer
    //  that disagrees with itself in a way the buffer it was computed from did
    //  not, and hands it over otherwise.
    //
    //  The check and the hand-off are one operation on purpose. A check sitting
    //  BESIDE the assignment can be deleted while the assignment stays, and
    //  nothing observable changes -- the suite cannot catch that, because no
    //  input makes correct code produce a bad buffer. Routing the hand-off
    //  through the check means removing it cannot be silent.
    //
    //  A refusal means OUR writer produced a bad image, not that the user asked
    //  for something impossible, so it answers E_UNEXPECTED and asserts -- the
    //  one condition in this layer that is a defect rather than a verdict.
    //
    //  Public because a refusal is only reachable by handing it a result that is
    //  deliberately wrong.
    static HRESULT  HandBackVerifiedResult (const VolumeIntegrityReport  & before,
                                            const vector<Byte>           & result,
                                            vector<Byte>                 & outBuffer);

    //  The ProDOS file types this feature names on the command line. The full
    //  set is 256 values; these are the ones a developer places or extracts.
    static constexpr Byte  kTypeText   = 0x04;
    static constexpr Byte  kTypeBinary = 0x06;
    static constexpr Byte  kTypeBasic  = 0xFC;
    static constexpr Byte  kTypeSystem = 0xFF;

    //  Write-enable bit of the access byte. Clear means the file is locked.
    static constexpr Byte  kAccessWriteEnable = 0x02;

    //  Destroy-enable bit. ProDOS gates removal on this rather than on
    //  write-enable, and the access byte can express one without the other, so
    //  the two refusals are decided by different bits.
    static constexpr Byte  kAccessDestroyEnable = 0x80;

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

    //  The largest EOF a directory entry can record, the field being three
    //  bytes wide.
    static constexpr size_t  kMaxFileBytes = 0xFFFFFF;

    //  The kernel. It is a SYS file like any system program, and it is never the
    //  one a boot launches: the boot block loads it BY NAME, and a volume whose
    //  startup program was the kernel would only load it a second time.
    static constexpr const char *  kpszKernelName = "PRODOS";

    Byte  ReadByte (int block, size_t offset) const;
    Word  ReadWord (int block, size_t offset) const;

    static void  WriteByteAt (vector<Byte> & buffer, int block, size_t offset, Byte value);
    static void  WriteWordAt (vector<Byte> & buffer, int block, size_t offset, Word value);

    //  Volume bitmap: one bit per block, MSB of byte 0 is block 0, SET is free.
    static bool  IsFreeInBitmap  (const vector<Byte> & buffer, uint32_t block);
    static void  SetFreeInBitmap (vector<Byte> & buffer, uint32_t block, bool isFree);

    //  Fifteen characters, a letter first, then letters digits and periods --
    //  ProDOS's own rule. THIS VALIDATES ONLY NAMES BEING CREATED; reading
    //  imposes no rule at all, on either filesystem.
    static bool  TryEncodeDirectoryName (const std::string & name, std::string & outName);

    //  Which storage type a file of this many data blocks needs, and how many
    //  index blocks come with it -- none for a seedling, one for a sapling, and
    //  for a tree one per group of 256 plus the master index above them.
    static Byte    StorageTypeFor      (size_t dataBlockCount);
    static size_t  OverheadBlocksFor   (size_t dataBlockCount);

    //  One pointer in an index block: low byte in the first half, high byte in
    //  the second.
    static void  WriteIndexPointer (vector<Byte> & buffer, int indexBlock, size_t slot, Word target);

    //  Case-insensitive, because the directory is upper case and the caller's
    //  shell is not.
    static bool  TryFindEntry (const vector<RawEntry>  & entries,
                               const std::string       & leaf,
                               uint16_t                & outOwner);

    //  A record the boot path could hand control to: a system program, and not
    //  the kernel that does the handing.
    static bool  IsLaunchableSystemFile (const RawEntry & entry);

    //  A directory record is reusable exactly when its storage-type nibble is
    //  zero. Unlike the DOS 3.3 catalog there is no decorative-entry practice
    //  here to work around: ProDOS states occupancy in a field of its own.
    bool  TryFindFreeDirectorySlot (int & outBlock, size_t & outOffset) const;

    //  Hands out `count` blocks, or none at all. A block must be free in the
    //  bitmap AND unclaimed by the directory -- a bitmap calling a block free
    //  while an entry still points at it is what a bad delete leaves behind,
    //  and handing it out destroys the other file with nothing reporting it.
    bool  TryAllocateBlocks (const VolumeIntegrityReport  & report,
                             size_t                         count,
                             vector<uint32_t>             & outBlocks) const;

    static void  ZeroBlocks (vector<Byte> & buffer, const vector<uint32_t> & blocks);

    //  Lays down a file whose name the directory does not already hold, over
    //  the buffer this volume was constructed with. Replacement stages the
    //  removal into a working buffer and calls this on a volume over that, so
    //  one code path places every file and a replacement cannot half-happen.
    HRESULT  AddFile (const std::string   & name,
                      Byte                  fileType,
                      Word                  auxType,
                      const vector<Byte>  & bytes,
                      vector<Byte>        & outBuffer) const;

    //  Lays the allocated blocks out in the order the allocator handed them
    //  over: master index first when there is one, then each index block
    //  followed by the data blocks it describes. Fails when the list does not
    //  match the shape, rather than walking off the end of it.
    static HRESULT  PlaceFile (vector<Byte>            & buffer,
                               const vector<uint32_t>  & blocks,
                               size_t                    dataBlockCount,
                               const vector<Byte>      & bytes);

    static void  WriteDirectoryEntry (vector<Byte>       & buffer,
                                      int                  dirBlock,
                                      size_t               entryOffset,
                                      const std::string  & name,
                                      Byte                 storage,
                                      Byte                 fileType,
                                      Word                 keyBlock,
                                      Word                 blocksUsed,
                                      uint32_t             eof,
                                      Word                 auxType);

    static void  AdjustFileCount (vector<Byte> & buffer, int delta);

    //  A binary's auxiliary type IS its load address, and this filesystem
    //  stores no header inside the file to hold one instead.
    static HRESULT  ResolveAuxType (const FilePayload & payload, Word & outAuxType);

    //  Turns what a delete decided into text a user can act on.
    static void  AppendDeleteWarnings (DeleteOutcome & inOutOutcome);

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
