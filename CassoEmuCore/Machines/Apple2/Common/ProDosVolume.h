#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Devices/Disk/ChainWalkGuard.h"





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
//  Subdirectories are listed, read and written by path. Every file operation
//  walks the path's directories and acts in the directory the leaf sits in.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolume : public IVolume
{
public:
    explicit ProDosVolume (const vector<Byte> & sectors);

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  CreateDirectory (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const override;

    HRESULT  RemoveDirectory  (const FilePath  & path,
                               bool              force,
                               vector<Byte>    & outBuffer,
                               DeleteOutcome   & outOutcome) const override;

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

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;

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

    //  A directory's file type, which ProDOS records as DIR.
    static constexpr Byte  kTypeDirectory = 0x0F;

    //  Write-enable bit of the access byte. Clear means the file is locked.
    static constexpr Byte  kAccessWriteEnable = 0x02;

    //  Destroy-enable bit. ProDOS gates removal on this rather than on
    //  write-enable, and the access byte can express one without the other, so
    //  the two refusals are decided by different bits.
    static constexpr Byte  kAccessDestroyEnable = 0x80;

    //  Rename-enable bit, which gates renaming the same way.
    static constexpr Byte  kAccessRenameEnable  = 0x40;

    //  A ProDOS date and time as Unix seconds, or false when the fields hold
    //  no date. The date word packs year, month and day; the time word packs
    //  hour and minute. Years below 40 belong to this century, the convention
    //  ProDOS 8 adopted once its two-digit years ran out.
    static bool  TryToUnixTime (Word date, Word time, int64_t & outUnix);

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
        Word      modDate     = 0;
        Word      modTime     = 0;
        Word      caseFlags   = 0;
        uint32_t  eof         = 0;
        string    name;
    };

    //  The two-digit year at which the ProDOS century convention turns over.
    static constexpr int  kCenturyPivotYear = 40;

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

    //  The same, and the GS/OS case word for the name as it was typed. The
    //  stored name is upper case either way; the word is what a IIgs reads to
    //  show the name back the way it was written.
    static bool  TryEncodeDirectoryName (const std::string & name, std::string & outName, Word & outCaseFlags);

    //  An upper-case name with the case word applied, for display. A word
    //  without its top bit describes nothing, and the name comes back as the
    //  directory holds it.
    static std::string  ApplyCaseFlags (const std::string & name, Word caseFlags);

    //  Which storage type a file of this many data blocks needs, and how many
    //  index blocks come with it -- none for a seedling, one for a sapling, and
    //  for a tree one per group of 256 plus the master index above them.
    static Byte    GetStorageType    (size_t dataBlockCount);
    static size_t  GetOverheadBlocks (size_t dataBlockCount);

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
    bool  TryFindFreeDirectorySlot (int dirKeyBlock, int & outBlock, size_t & outOffset) const;

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
    HRESULT  AddFileWithCase (int                   dirKeyBlock,
                              const std::string   & name,
                              Word                  caseFlags,
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
                                      Word                 auxType,
                                      int                  headerPointer,
                                      Word                 caseFlags);

    //  The tally of active entries in one directory's header. Every directory
    //  keeps its own, at the same offset in its key block.
    static void  AdjustFileCount (vector<Byte> & buffer, int dirKeyBlock, int delta);

    //  Links an already-allocated block onto the end of a directory's chain,
    //  and for a subdirectory grows the record its parent holds, whose blocks
    //  and length describe the directory itself.
    static void  LinkDirectoryBlock (vector<Byte> & buffer, int dirKeyBlock, uint32_t newBlock);

    //  The first record of a new subdirectory's key block: its own name, the
    //  marker ProDOS checks for, and the way back to the record above it.
    static void  WriteSubdirectoryHeader (vector<Byte>       & buffer,
                                          int                  keyBlock,
                                          const std::string  & name,
                                          int                  parentBlock,
                                          size_t               parentEntryOffset);

    //  A binary's auxiliary type IS its load address, and this filesystem
    //  stores no header inside the file to hold one instead.
    static HRESULT  ResolveAuxType (const FilePayload & payload, Word & outAuxType);

    //  Turns what a delete decided into text a user can act on.
    static void  AppendDeleteWarnings (DeleteOutcome & inOutOutcome);

    //  One directory's contents into the plan, deepest first, each path written
    //  out from the volume directory.
    void  CollectRemovalEntries (int                     keyBlock,
                                 const std::string     & prefix,
                                 ChainWalkGuard        & guard,
                                 DirectoryRemovalPlan  & inOutPlan) const;

    //  What Delete does, plus the override a subtree removal needs: with
    //  `force` a locked entry goes too, which is the only difference between
    //  removing one file and removing one inside a subtree the caller has
    //  already been answered on.
    HRESULT  DeleteEntry (const FilePath  & path,
                          bool              force,
                          vector<Byte>    & outBuffer,
                          DeleteOutcome   & outOutcome) const;

    //  Removes a directory that holds nothing: its chain of blocks goes back to
    //  the free map and its record in the parent becomes a tombstone.
    HRESULT  RemoveEmptyDirectory (const FilePath  & path,
                                   vector<Byte>    & outBuffer,
                                   DeleteOutcome   & outOutcome) const;

    //  Walks one directory's chain from its key block, under a guard. Damage
    //  is appended rather than thrown.
    void  CollectEntries (int                   keyBlock,
                          vector<RawEntry>    & outEntries,
                          vector<std::string> & outDamage,
                          bool                & outFullyParsed) const;

    //  Every entry on the volume, depth first: the volume directory's records
    //  in order, and the records inside a subdirectory right after the record
    //  that points at it. THE CLAIM MAP AND EVERY OWNER INDEX ARE IN THIS
    //  ORDER, so a caller holding an owner index and the report agree.
    void  CollectAllEntries (vector<RawEntry>    & outEntries,
                             vector<std::string> & outDamage,
                             bool                & outFullyParsed) const;

    void  CollectEntriesBelow (int                   keyBlock,
                               ChainWalkGuard      & guard,
                               vector<RawEntry>    & outEntries,
                               vector<std::string> & outDamage,
                               bool                & outFullyParsed) const;

    //  Every block of one directory's chain, from its key block. A
    //  subdirectory's blocks are as allocated as any file's.
    void  CollectDirectoryBlocks (int                keyBlock,
                                  vector<uint32_t> & outBlocks,
                                  ChainWalkGuard   & guard) const;

    //  Where an entry sits in the order CollectAllEntries produces, which is
    //  the owner index the integrity report speaks in.
    static bool  TryFindOwnerIndex (const vector<RawEntry>  & all,
                                    const RawEntry          & entry,
                                    uint16_t                & outOwner);

    //  The key block of the directory a path names, walked down from the
    //  volume directory; an empty path is the volume directory itself.
    HRESULT  ResolveDirectory (const FilePath & directory, int & outKeyBlock) const;

    //  Every component of a path but its last: the directory the leaf is in.
    static FilePath  GetParentPath (const FilePath & path);

    //  One directory record as a listing reports it.
    static FileEntry  ToFileEntry (const RawEntry & entry);

    //  Every block one entry occupies, index blocks included. False when the
    //  structure could not be walked, which the caller reports as an
    //  unfollowable chain rather than as a short file.
    bool  CollectFileBlocks (const RawEntry    & entry,
                             vector<uint32_t>  & outBlocks,
                             ChainWalkGuard    & guard) const;

    const vector<Byte> &  m_sectors;
};
