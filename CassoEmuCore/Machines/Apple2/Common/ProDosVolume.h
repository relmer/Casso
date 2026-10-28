#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"
#include "Devices/Disk/ChainWalkGuard.h"
#include "Machines/Apple2/Common/ProDosVolumeWriter.h"
#include "Machines/Apple2/Common/ProDosVolumeRemover.h"





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
//  Placement and removal are in ProDosVolumeWriter and ProDosVolumeRemover,
//  rather than in an extended ProDosFileWriter, which stays what it was built
//  to be: the bootable-disk helper that installs PRODOS and BASIC.SYSTEM into
//  a freshly formatted buffer. It writes THROUGH the caller's buffer,
//  allocates from the bitmap alone, and asserts on bad arguments -- all
//  correct for a caller supplying its own constants, and all wrong for one
//  relaying what a user typed. What keeps the two from drifting is that files
//  the writer places are read back through ProDosReader, which neither of
//  them shares.
//
//  Subdirectories are listed, read and written by path. Every file operation
//  walks the path's directories and acts in the directory the leaf sits in.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolume : public IVolume
{
public:
    explicit ProDosVolume (const vector<Byte> & sectors);
    ProDosVolume (const ProDosVolume &) = delete;

    //  Called with each sector the volume reads from here on; null for none.
    void     SetReadObserver (ISectorReadObserver * observer) { m_readObserver = observer; }

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    //  Placement goes to the writer and removal to the remover.
    HRESULT  CreateDirectory   (const FilePath & path, vector<Byte> & outBuffer) const override { return m_writer.CreateDirectory (path, outBuffer); }
    HRESULT  Write             (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const override { return m_writer.Write (path, payload, outBuffer); }
    HRESULT  SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const override { return m_writer.SetStartupProgram (path, outBuffer); }
    HRESULT  Rename            (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const override { return m_writer.Rename (from, to, outBuffer); }

    HRESULT  BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const override { return m_remover.BuildRemovalPlan (path, outPlan); }
    HRESULT  RemoveDirectory  (const FilePath & path, bool force, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override { return m_remover.RemoveDirectory (path, force, outBuffer, outOutcome); }
    HRESULT  Delete           (const FilePath & path, vector<Byte> & outBuffer) const override { return m_remover.Delete (path, outBuffer); }

    //  Delete, plus an account of what it freed, what it refused to free, and
    //  the conditions that bound the answer. The interface form forwards here
    //  and throws the account away; anything reporting to a user wants it.
    HRESULT  Delete           (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override { return m_remover.Delete (path, outBuffer, outOutcome); }

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;

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

    //  The buffer, for a read of one block of it. Every read of the volume's
    //  own buffer goes through here, so the read observer has each sector.
    const vector<Byte> &  AccessBlock (int block) const;
    Word  ReadWord (int block, size_t offset) const;

    //  Volume bitmap: one bit per block, MSB of byte 0 is block 0, SET is free.
    static bool  IsFreeInBitmap  (const vector<Byte> & buffer, uint32_t block);

    //  An upper-case name with the case word applied, for display. A word
    //  without its top bit describes nothing, and the name comes back as the
    //  directory holds it.
    static std::string  ApplyCaseFlags (const std::string & name, Word caseFlags);

    //  Case-insensitive, because the directory is upper case and the caller's
    //  shell is not.
    static bool  TryFindEntry (const vector<RawEntry>  & entries,
                               const std::string       & leaf,
                               uint16_t                & outOwner);

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

    //  A record the boot path could hand control to: a system program, and not
    //  the kernel that does the handing.
    static bool  IsLaunchableSystemFile (const RawEntry & entry);

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

    friend class ProDosVolumeWriter;
    friend class ProDosVolumeRemover;

    const vector<Byte> &   m_sectors;
    ISectorReadObserver  * m_readObserver = nullptr;

    ProDosVolumeWriter   m_writer  { *this };
    ProDosVolumeRemover  m_remover { *this };
};
