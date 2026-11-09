#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Devices/Disk/ChainWalkGuard.h"

class ProDosVolume;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter
//
//  Placement on a ProDOS volume: files written, replaced and renamed, new
//  directories, and the startup program. Every result is computed into a
//  new buffer and checked before it is returned to the caller.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosVolumeWriter
{
public:
    explicit ProDosVolumeWriter (const ProDosVolume & volume);

    HRESULT  CreateDirectory (const FilePath & path, vector<Byte> & outBuffer) const;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const;

    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const;

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const;

private:
    friend class ProDosVolume;
    friend class ProDosVolumeRemover;

    static void  WriteByteAt (vector<Byte> & buffer, int block, size_t offset, Byte value);
    static void  WriteWordAt (vector<Byte> & buffer, int block, size_t offset, Word value);

    //  Volume bitmap: one bit per block, MSB of byte 0 is block 0, SET is free.
    static void  SetFreeInBitmap (vector<Byte> & buffer, uint32_t block, bool isFree);

    //  Fifteen characters, a letter first, then letters digits and periods --
    //  ProDOS's own rule. THIS VALIDATES ONLY NAMES BEING CREATED; reading
    //  imposes no rule at all, on either filesystem.
    static bool  TryEncodeDirectoryName (const std::string & name, std::string & outName);

    //  The same, and the GS/OS case word for the name as it was typed. The
    //  stored name is upper case either way; the word is what a IIgs reads to
    //  show the name back the way it was written.
    static bool  TryEncodeDirectoryName (const std::string & name, std::string & outName, Word & outCaseFlags);

    //  Which storage type a file of this many data blocks needs, and how many
    //  index blocks come with it -- none for a seedling, one for a sapling, and
    //  for a tree one per group of 256 plus the master index above them.
    static Byte    GetStorageType    (size_t dataBlockCount);
    static size_t  GetOverheadBlocks (size_t dataBlockCount);

    //  One pointer in an index block: low byte in the first half, high byte in
    //  the second.
    static void  WriteIndexPointer (vector<Byte> & buffer, int indexBlock, size_t slot, Word target);

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

    //  The payload's access byte and dates written over the new record of
    //  `name` in the directory, when the payload has them.
    static void  StampEntry (vector<Byte> & buffer, int dirKeyBlock, const std::string & name, const FilePayload & payload);

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

    const ProDosVolume  & m_volume;
};