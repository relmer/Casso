#include "Pch.h"

#include "Machines/Apple2/Common/ProDosVolumeWriter.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Utils.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::ProDosVolumeWriter
//
////////////////////////////////////////////////////////////////////////////////

ProDosVolumeWriter::ProDosVolumeWriter (const ProDosVolume & volume)
    : m_volume (volume)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::WriteByteAt
//
//  Refuses a block the volume cannot hold rather than indexing past the end of
//  the buffer, for the same reason ReadByte does.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::WriteByteAt (vector<Byte> & buffer, int block, size_t offset, Byte value)
{
    size_t  at = 0;



    if (block < 0 || block >= ProDosSkeleton::kTotalBlocks)
    {
        return;
    }

    at = ProDosSkeleton::GetBlockByteOffset (block, offset);

    if (at < buffer.size())
    {
        buffer[at] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::WriteWordAt
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::WriteWordAt (vector<Byte> & buffer, int block, size_t offset, Word value)
{
    WriteByteAt (buffer, block, offset,     (Byte) (value & 0xFF));
    WriteByteAt (buffer, block, offset + 1, (Byte) (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::SetFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::SetFreeInBitmap (vector<Byte> & buffer, uint32_t block, bool isFree)
{
    size_t  at   = 0;
    Byte    mask = (Byte) (0x80 >> (block % 8));
    Byte    bits = 0;



    if (block >= (uint32_t) ProDosSkeleton::kTotalBlocks)
    {
        return;
    }

    at = ProDosSkeleton::GetBlockByteOffset (ProDosSkeleton::kBitmapBlock, (size_t) (block / 8));

    if (at >= buffer.size())
    {
        return;
    }

    bits       = buffer[at];
    buffer[at] = isFree ? (Byte) (bits | mask) : (Byte) (bits & ~mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::TryEncodeDirectoryName
//
//  Fifteen characters at most, a letter first, then letters digits and periods
//  -- the rule ProDOS itself enforces.
//
//  THIS VALIDATES ONLY NAMES BEING CREATED. Reading imposes no rule at all: a
//  printable-text check on names being READ was tried on the DOS 3.3 side and
//  reverted for rejecting twenty entries a vendor shipped. Creating a name is
//  the opposite direction -- one ProDOS would not accept is one the guest can
//  never open again, so refusing costs nothing and rejects nothing that exists.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolumeWriter::TryEncodeDirectoryName (const std::string & name, std::string & outName)
{
    Word  ignored = 0;



    return TryEncodeDirectoryName (name, outName, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::TryEncodeDirectoryName
//
//  The stored name is upper case whatever was typed, and the case word records
//  what was typed, so a IIgs shows `Desk.Accs` where an Apple //e shows
//  `DESK.ACCS` and both are reading the same entry.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolumeWriter::TryEncodeDirectoryName (const std::string & name, std::string & outName, Word & outCaseFlags)
{
    size_t  length = name.size();
    size_t  i      = 0;
    int     bit    = 0;
    Byte    first  = 0;



    outName.clear();

    outCaseFlags = ProDosSkeleton::kCaseFlagsPresent;

    if (length == 0 || length > ProDosSkeleton::kVolumeNameBytes)
    {
        return false;
    }

    first = (Byte) name[0];

    if (!(first >= 'A' && first <= 'Z') && !(first >= 'a' && first <= 'z'))
    {
        return false;
    }

    for (i = 0; i < length; i++)
    {
        Byte  c         = (Byte) name[i];
        bool  isLetter  = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool  isDigit   = c >= '0' && c <= '9';
        bool  permitted = isLetter || isDigit || c == '.';

        if (!permitted)
        {
            outName.clear();

            outCaseFlags = 0;

            return false;
        }

        // The guest's keyboard produces upper case and its directory stores it,
        // so a lower-case name would list as something nobody can type. The
        // case word records what was typed for anything that reads it.
        bit = ProDosSkeleton::kCaseFirstCharBit - (int) i;

        if (c >= 'a' && c <= 'z' && bit >= 0)
        {
            outCaseFlags |= (Word) (1u << bit);
        }

        outName += (char) ((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::GetStorageType
//
//  A seedling IS its data block. A sapling adds one index block of up to 256
//  pointers above it. Past that the file needs a master index of index blocks,
//  which is a tree -- and it is that growth, not the first two, that a writer
//  handling only small files never exercises.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProDosVolumeWriter::GetStorageType (size_t dataBlockCount)
{
    if (dataBlockCount <= 1)
    {
        return ProDosSkeleton::kStorageSeedling;
    }

    if (dataBlockCount <= ProDosSkeleton::kPointersPerIndex)
    {
        return ProDosSkeleton::kStorageSapling;
    }

    return ProDosSkeleton::kStorageTree;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::GetOverheadBlocks
//
//  How many blocks the structure costs on top of the data itself. This is what
//  makes the difference between the recorded block count and the file's length
//  -- an index block is as allocated as the bytes it points at.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosVolumeWriter::GetOverheadBlocks (size_t dataBlockCount)
{
    size_t  groups = (dataBlockCount + ProDosSkeleton::kPointersPerIndex - 1)
                   / ProDosSkeleton::kPointersPerIndex;



    if (dataBlockCount <= 1)
    {
        return 0;
    }

    if (dataBlockCount <= ProDosSkeleton::kPointersPerIndex)
    {
        return 1;
    }

    return groups + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::WriteIndexPointer
//
//  An index block holds 256 low bytes followed by the 256 matching high bytes,
//  rather than 256 little-endian pairs. Writing it as pairs produces a file
//  that reads back correctly for any block number below 256 and silently
//  wrongly above it, which on a 280-block volume is most of the disk.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::WriteIndexPointer (
    vector<Byte>  & buffer,
    int             indexBlock,
    size_t          slot,
    Word            target)
{
    WriteByteAt (buffer, indexBlock, slot, (Byte) (target & 0xFF));

    WriteByteAt (buffer, indexBlock, slot + ProDosSkeleton::kPointersPerIndex,
                 (Byte) (target >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::TryFindFreeDirectorySlot
//
//  The first record in the volume directory chain whose storage-type nibble is
//  zero. That nibble is ProDOS's own statement of occupancy, so unlike the
//  DOS 3.3 catalog -- where an entry claiming no sectors is still an entry --
//  there is nothing here to infer from what the record points at.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolumeWriter::TryFindFreeDirectorySlot (int dirKeyBlock, int & outBlock, size_t & outOffset) const
{
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    int             dirBlock = dirKeyBlock;
    int             n        = 0;



    while (dirBlock != 0)
    {
        bool  inRange = dirBlock > 0 && dirBlock < ProDosSkeleton::kTotalBlocks;
        bool  stepOk  = inRange && guard.TryVisit ((uint32_t) dirBlock);
        int   first   = (dirBlock == dirKeyBlock) ? 1 : 0;

        if (!stepOk)
        {
            break;
        }

        for (n = first; n < (int) ProDosSkeleton::kEntriesPerBlock; n++)
        {
            size_t  at      = ProDosSkeleton::kOffFirstEntry
                            + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte    typeLen = m_volume.ReadByte (dirBlock, at);

            if ((typeLen & 0xF0) == ProDosSkeleton::kStorageInactive)
            {
                outBlock  = dirBlock;
                outOffset = at;

                return true;
            }
        }

        dirBlock = m_volume.ReadWord (dirBlock, ProDosSkeleton::kOffNextBlock);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::TryAllocateBlocks
//
//  Hands out exactly `count` blocks or none at all, so a refusal leaves nothing
//  half-reserved.
//
//  A block must be free in the bitmap AND unclaimed by the directory. The
//  second condition is the one that matters: a bitmap calling a block free
//  while an entry still points at it is exactly the state a previous bad delete
//  leaves behind, and handing that block to the next file destroys the first
//  one with nothing reporting it. Skipping those costs a little space and
//  leaves the disagreement for the integrity report to name.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolumeWriter::TryAllocateBlocks (
    const VolumeIntegrityReport  & report,
    size_t                         count,
    vector<uint32_t>             & outBlocks) const
{
    uint32_t  block = 0;
    size_t    taken = 0;



    outBlocks.clear();

    for (block = 0; block < (uint32_t) ProDosSkeleton::kTotalBlocks && taken < count; block++)
    {
        bool  isFree  = ProDosVolume::IsFreeInBitmap (m_volume.AccessBlock (ProDosSkeleton::kBitmapBlock), block);
        bool  claimed = report.IsClaimed (block);

        if (isFree && !claimed)
        {
            outBlocks.push_back (block);
            taken++;
        }
    }

    return taken == count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::ZeroBlocks
//
//  Every block a new file occupies is cleared first. An index block still
//  holding a previous file's pointers would be read as this file's structure,
//  and the tail of a partly filled last block would hand back bytes nobody
//  ever wrote.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::ZeroBlocks (vector<Byte> & buffer, const vector<uint32_t> & blocks)
{
    for (uint32_t block : blocks)
    {
        size_t  i = 0;

        for (i = 0; i < (size_t) ProDosSkeleton::kBlockByteSize; i++)
        {
            WriteByteAt (buffer, (int) block, i, 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::PlaceFile
//
//  Lays the allocated blocks out as ProDOS does. The allocator hands them over
//  in the order they are consumed here: the master index first when the file
//  needs one, then each index block followed by the data blocks it describes.
//
//  A seedling has neither, and one loop covers all three shapes rather than
//  three code paths that can disagree about the boundary between them.
//
//  The block list is checked against the shape before a single pointer is
//  consumed, and the count is derived HERE rather than borrowed from the
//  allocator's own helper. Borrowing it would make the check agree with the
//  allocator by construction, including when both are wrong -- which is the
//  case that matters, since the walk answers a short list by reading past the
//  end of it. In a release build that is silent memory corruption rather than
//  a failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::PlaceFile (
    vector<Byte>            & buffer,
    const vector<uint32_t>  & blocks,
    size_t                    dataBlockCount,
    const vector<Byte>      & bytes)
{
    HRESULT  hr        = S_OK;
    size_t   supplied  = blocks.size();
    size_t   needed    = 0;
    size_t   cursor    = 0;
    size_t   master    = 0;
    size_t   group     = 0;
    size_t   groups    = (dataBlockCount + ProDosSkeleton::kPointersPerIndex - 1)
                       / ProDosSkeleton::kPointersPerIndex;
    size_t   placed    = 0;
    size_t   dataBytes = bytes.size();
    bool     seedling  = dataBlockCount <= 1;
    bool     tree      = dataBlockCount > ProDosSkeleton::kPointersPerIndex;



    needed = dataBlockCount + (seedling ? 0 : groups) + (tree ? 1 : 0);

    CBRAEx (supplied == needed, E_UNEXPECTED);

    if (tree)
    {
        master = blocks[cursor];
        cursor++;
    }

    for (group = 0; group < groups; group++)
    {
        size_t  index       = 0;
        size_t  k           = 0;
        size_t  inThisGroup = std::min ((size_t) ProDosSkeleton::kPointersPerIndex,
                                        dataBlockCount - placed);

        if (!seedling)
        {
            index = blocks[cursor];
            cursor++;

            if (tree)
            {
                WriteIndexPointer (buffer, (int) master, group, (Word) index);
            }
        }

        for (k = 0; k < inThisGroup; k++)
        {
            size_t  data     = blocks[cursor];
            size_t  from     = (placed + k) * (size_t) ProDosSkeleton::kBlockByteSize;
            size_t  left     = (from < dataBytes) ? dataBytes - from : 0;
            size_t  copyable = std::min ((size_t) ProDosSkeleton::kBlockByteSize, left);
            size_t  i        = 0;

            cursor++;

            if (!seedling)
            {
                WriteIndexPointer (buffer, (int) index, k, (Word) data);
            }

            for (i = 0; i < copyable; i++)
            {
                WriteByteAt (buffer, (int) data, i, bytes[from + i]);
            }
        }

        placed += inThisGroup;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::WriteDirectoryEntry
//
//  The whole record is cleared before any field is written. A slot being reused
//  after a delete still carries the previous file's dates and version bytes,
//  and leaving them would attach one file's history to another.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::WriteDirectoryEntry (
    vector<Byte>       & buffer,
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
    Word                 caseFlags)
{
    size_t  nameBytes = name.size();
    size_t  i         = 0;



    for (i = 0; i < (size_t) ProDosSkeleton::kEntryLength; i++)
    {
        WriteByteAt (buffer, dirBlock, entryOffset + i, 0);
    }

    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffTypeName,
                 (Byte) (storage | (Byte) nameBytes));

    for (i = 0; i < nameBytes; i++)
    {
        WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffName + i,
                     (Byte) name[i]);
    }

    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffFileType, fileType);
    WriteWordAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffKeyPointer, keyBlock);
    WriteWordAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffBlocksUsed, blocksUsed);

    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffEof,
                 (Byte) (eof & 0xFF));
    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffEof + 1,
                 (Byte) ((eof >> 8) & 0xFF));
    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffEof + 2,
                 (Byte) ((eof >> 16) & 0xFF));

    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffAccess,
                 ProDosSkeleton::kAccessDefault);
    WriteWordAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffAuxType, auxType);
    //  A word, not a byte: a directory's key block can sit past block 255.
    WriteWordAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffHeaderPointer,
                 (Word) headerPointer);

    //  The case of the name as it was typed, which ProDOS 8 never reads.
    WriteWordAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffCaseFlags, caseFlags);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::AdjustFileCount
//
//  A directory header's own tally of active entries. It is not what enumeration
//  reads -- that walks the records -- but ProDOS itself reads it, so a directory
//  whose tally disagrees with its records is one the guest mounts oddly. A
//  subdirectory keeps its count at the same offset in its own key block as the
//  volume directory keeps its own.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::AdjustFileCount (vector<Byte> & buffer, int dirKeyBlock, int delta)
{
    size_t  countAt = ProDosSkeleton::kOffFirstEntry + ProDosSkeleton::kHdrOffFileCount;
    size_t  at      = ProDosSkeleton::GetBlockByteOffset (dirKeyBlock, countAt);
    size_t  atHigh  = ProDosSkeleton::GetBlockByteOffset (dirKeyBlock, countAt + 1);
    int     count   = 0;



    if (at >= buffer.size() || atHigh >= buffer.size())
    {
        return;
    }

    count = buffer[at] | (buffer[atHigh] << 8);
    count += delta;

    if (count < 0)
    {
        count = 0;
    }

    WriteWordAt (buffer, dirKeyBlock, countAt, (Word) count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::LinkDirectoryBlock
//
//  The new block goes on the end of the chain, back-linked to the one before
//  it. A subdirectory also has a record in its parent whose blocks-used and
//  length describe the directory rather than any file, so both move with it;
//  the volume directory has no such record and needs neither.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::LinkDirectoryBlock (vector<Byte> & buffer, int dirKeyBlock, uint32_t newBlock)
{
    ProDosVolume    volume (buffer);
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    size_t          headerAt    = ProDosSkeleton::kOffFirstEntry;
    int             last        = dirKeyBlock;
    int             parent      = 0;
    size_t          recordAt    = 0;
    Word            blocksUsed  = 0;
    Byte            entryNumber = 0;
    Byte            entryLength = 0;
    bool            isSubdir    = false;



    while (true)
    {
        int   next   = volume.ReadWord (last, ProDosSkeleton::kOffNextBlock);
        bool  stepOk = next > 0 && next < ProDosSkeleton::kTotalBlocks && guard.TryVisit ((uint32_t) next);

        if (!stepOk)
        {
            break;
        }

        last = next;
    }

    WriteWordAt (buffer, last, ProDosSkeleton::kOffNextBlock, (Word) newBlock);
    WriteWordAt (buffer, (int) newBlock, ProDosSkeleton::kOffPrevBlock, (Word) last);
    WriteWordAt (buffer, (int) newBlock, ProDosSkeleton::kOffNextBlock, 0);

    isSubdir = (volume.ReadByte (dirKeyBlock, headerAt + ProDosSkeleton::kHdrOffTypeName) & 0xF0)
               == ProDosSkeleton::kStorageSubdirHdr;

    if (!isSubdir)
    {
        return;
    }

    parent      = volume.ReadWord (dirKeyBlock, headerAt + ProDosSkeleton::kHdrOffParentPointer);
    entryNumber = volume.ReadByte (dirKeyBlock, headerAt + ProDosSkeleton::kHdrOffParentEntryNumber);
    entryLength = volume.ReadByte (dirKeyBlock, headerAt + ProDosSkeleton::kHdrOffParentEntryLength);

    if (parent <= 0 || parent >= ProDosSkeleton::kTotalBlocks || entryNumber == 0 || entryLength == 0)
    {
        return;
    }

    recordAt = ProDosSkeleton::kOffFirstEntry + (size_t) (entryNumber - 1) * entryLength;

    //  The header's own account of where its record sits is read off the disk,
    //  so a damaged one can point past the end of the block it names.
    if (recordAt + ProDosSkeleton::kEntryLength > (size_t) ProDosSkeleton::kBlockByteSize)
    {
        return;
    }

    blocksUsed = (Word) (volume.ReadWord (parent, recordAt + ProDosSkeleton::kEntOffBlocksUsed) + 1);

    WriteWordAt (buffer, parent, recordAt + ProDosSkeleton::kEntOffBlocksUsed, blocksUsed);
    WriteByteAt (buffer, parent, recordAt + ProDosSkeleton::kEntOffEof,
                 (Byte) ((blocksUsed * ProDosSkeleton::kBlockByteSize) & 0xFF));
    WriteByteAt (buffer, parent, recordAt + ProDosSkeleton::kEntOffEof + 1,
                 (Byte) (((blocksUsed * ProDosSkeleton::kBlockByteSize) >> 8) & 0xFF));
    WriteByteAt (buffer, parent, recordAt + ProDosSkeleton::kEntOffEof + 2,
                 (Byte) (((blocksUsed * ProDosSkeleton::kBlockByteSize) >> 16) & 0xFF));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::WriteSubdirectoryHeader
//
//  A subdirectory's key block opens with a header of its own rather than with a
//  file record: the same name and geometry fields the volume header has,
//  the 0x75 marker ProDOS checks before trusting the block, and the three
//  fields that lead back to the record above it.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::WriteSubdirectoryHeader (
    vector<Byte>       & buffer,
    int                  keyBlock,
    const std::string  & name,
    int                  parentBlock,
    size_t               parentEntryOffset)
{
    size_t  headerAt    = ProDosSkeleton::kOffFirstEntry;
    size_t  nameBytes   = name.size();
    size_t  entryNumber = (parentEntryOffset - ProDosSkeleton::kOffFirstEntry) / ProDosSkeleton::kEntryLength + 1;
    size_t  i           = 0;



    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffTypeName,
                 (Byte) (ProDosSkeleton::kStorageSubdirHdr | (Byte) nameBytes));

    for (i = 0; i < nameBytes; i++)
    {
        WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffName + i, (Byte) name[i]);
    }

    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffSubdirMarker, ProDosSkeleton::kSubdirMarker);
    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffAccess,          ProDosSkeleton::kAccessDefault);
    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffEntryLength,     ProDosSkeleton::kEntryLength);
    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffEntriesPerBlock, ProDosSkeleton::kEntriesPerBlock);

    WriteWordAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffFileCount, 0);
    WriteWordAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffParentPointer, (Word) parentBlock);

    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffParentEntryNumber, (Byte) entryNumber);
    WriteByteAt (buffer, keyBlock, headerAt + ProDosSkeleton::kHdrOffParentEntryLength, ProDosSkeleton::kEntryLength);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::CreateDirectory
//
//  One block, holding the new directory's header and nothing else, plus a
//  record for it in the directory above. A directory with no free record of its
//  own grows by a block first, allocated in the same call so the two cannot be
//  handed the same one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::CreateDirectory (const FilePath & path, vector<Byte> & outBuffer) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    bool                            nameOk      = false;
    bool                            exists      = false;
    bool                            slotOk      = false;
    bool                            rooted      = false;
    bool                            allocated   = false;
    bool                            fullyParsed = true;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    int                             slotBlock   = 0;
    size_t                          slotOffset  = 0;
    uint16_t                        holder      = 0;
    uint32_t                        keyBlock    = 0;
    uint32_t                        growBlock   = 0;
    Word                            caseFlags   = 0;
    std::string                     name;
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    vector<Byte>                    result;
    vector<uint32_t>                blocks;
    VolumeIntegrityReport           report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    nameOk = TryEncodeDirectoryName (path.GetLeaf(), name, caseFlags);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), dirKeyBlock);
    CHR (hr);

    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    exists = ProDosVolume::TryFindEntry (entries, name, holder);
    CBREx (!exists, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    slotOk = TryFindFreeDirectorySlot (dirKeyBlock, slotBlock, slotOffset);

    //  The volume directory is its four blocks: ProDOS 8 returns error $49
    //  for one more entry there, where it adds a block to a subdirectory.
    rooted = dirKeyBlock == ProDosSkeleton::kDirKeyBlock;
    CBREx (slotOk || !rooted, HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    allocated = TryAllocateBlocks (report, slotOk ? 1 : 2, blocks);
    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    keyBlock = blocks[0];
    result   = m_volume.m_sectors;

    if (!slotOk)
    {
        growBlock = blocks[1];

        ZeroBlocks (result, vector<uint32_t> { growBlock });
        LinkDirectoryBlock (result, dirKeyBlock, growBlock);
        SetFreeInBitmap (result, growBlock, false);

        slotBlock  = (int) growBlock;
        slotOffset = ProDosSkeleton::kOffFirstEntry;
    }

    ZeroBlocks (result, vector<uint32_t> { keyBlock });
    SetFreeInBitmap (result, keyBlock, false);

    WriteSubdirectoryHeader (result, (int) keyBlock, name, slotBlock, slotOffset);

    WriteDirectoryEntry (result,
                         slotBlock,
                         slotOffset,
                         name,
                         ProDosSkeleton::kStorageSubdir,
                         ProDosVolume::kTypeDirectory,
                         (Word) keyBlock,
                         1,
                         (uint32_t) ProDosSkeleton::kBlockByteSize,
                         0,
                         dirKeyBlock,
                         caseFlags);

    AdjustFileCount (result, dirKeyBlock, 1);

    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::ResolveAuxType
//
//  WHERE A BINARY'S LOAD ADDRESS LIVES IS THE ONE THING THE TWO FILESYSTEMS
//  DISAGREE ABOUT. DOS 3.3 writes it into the file's own first four bytes;
//  ProDOS records it in the directory entry's auxiliary type and stores no
//  header at all. So this filesystem's writer must not prepend anything, and a
//  reader that applied DOS-style stripping here would come back four bytes
//  short with no other symptom.
//
//  A binary with no load address is refused rather than defaulted, because
//  $0000 is a legal load address and a default would be indistinguishable from
//  an answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::ResolveAuxType (const FilePayload & payload, Word & outAuxType)
{
    HRESULT  hr       = S_OK;
    bool     isBinary = payload.type == ProDosVolume::kTypeBinary;
    bool     hasAddr  = payload.hasLoadAddress;



    outAuxType = 0;

    CBREx (!isBinary || hasAddr, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));

    if (isBinary)
    {
        outAuxType = payload.loadAddress;
    }
    else if (payload.hasAuxType)
    {
        outAuxType = payload.auxType;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::AddFileWithCase
//
//  Allocate from the volume bitmap, build whatever index structure the size
//  calls for, write the data blocks, create the directory entry, and leave the
//  bitmap agreeing with what was allocated.
//
//  The name arrives upper case, as the directory stores it, and the case word
//  beside it records what the caller typed.
//
//  Storage type GROWS with the file. A seedling is its own data block, a
//  sapling adds one index block, and past 256 data blocks a master index of
//  index blocks is required -- the shape no volume in the fixture set can hold,
//  since a tree needs more blocks than a 5.25-inch disk has files big enough to
//  fill.
//
//  Nothing is written into the volume this object holds. The complete new
//  buffer is produced instead, so a refusal at any point leaves the caller with
//  exactly what it had.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::AddFileWithCase (
    int                   dirKeyBlock,
    const std::string   & name,
    Word                  caseFlags,
    Byte                  fileType,
    Word                  auxType,
    const vector<Byte>  & bytes,
    vector<Byte>        & outBuffer) const
{
    HRESULT                hr           = S_OK;
    size_t                 payloadSize  = bytes.size();
    bool                   slotOk       = false;
    bool                   rooted       = false;
    bool                   allocated    = false;
    bool                   haveKeyBlock = false;
    int                    slotBlock    = 0;
    uint32_t               growBlock    = 0;
    size_t                 slotOffset   = 0;
    size_t                 dataBlocks   = 0;
    size_t                 overhead     = 0;
    vector<Byte>           result;
    vector<uint32_t>       blocks;
    VolumeIntegrityReport  report;



    slotOk = TryFindFreeDirectorySlot (dirKeyBlock, slotBlock, slotOffset);

    //  As in CreateDirectory: the volume directory does not grow.
    rooted = dirKeyBlock == ProDosSkeleton::kDirKeyBlock;
    CBREx (slotOk || !rooted, HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    dataBlocks =(payloadSize + (size_t) ProDosSkeleton::kBlockByteSize - 1)
               / (size_t) ProDosSkeleton::kBlockByteSize;

    // A file of no bytes still occupies its key block, which is what makes it a
    // file rather than a name.
    if (dataBlocks == 0)
    {
        dataBlocks = 1;
    }

    //  A directory with no free record takes one more block, allocated in the
    //  same call as the file's own. Two calls could hand the same block to
    //  both, since neither is recorded in the bitmap until the result is built.
    overhead  = GetOverheadBlocks (dataBlocks);
    allocated = TryAllocateBlocks (report, dataBlocks + overhead + (slotOk ? 0 : 1), blocks);

    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    haveKeyBlock = !blocks.empty();
    CBRA (haveKeyBlock);

    result = m_volume.m_sectors;

    if (!slotOk)
    {
        growBlock = blocks.back();
        blocks.pop_back();

        ZeroBlocks (result, vector<uint32_t> { growBlock });
        LinkDirectoryBlock (result, dirKeyBlock, growBlock);
        SetFreeInBitmap (result, growBlock, false);

        slotBlock  = (int) growBlock;
        slotOffset = ProDosSkeleton::kOffFirstEntry;
    }

    ZeroBlocks (result, blocks);

    hr = PlaceFile (result, blocks, dataBlocks, bytes);
    CHRA (hr);

    for (uint32_t block : blocks)
    {
        SetFreeInBitmap (result, block, false);
    }

    WriteDirectoryEntry (result,
                         slotBlock,
                         slotOffset,
                         name,
                         GetStorageType (dataBlocks),
                         fileType,
                         (Word) blocks[0],
                         (Word) blocks.size(),
                         (uint32_t) payloadSize,
                         auxType,
                         dirKeyBlock,
                         caseFlags);

    AdjustFileCount (result, dirKeyBlock, 1);

    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::Write
//
//  Adds a file, or replaces one of the same name.
//
//  REPLACEMENT IS COMPUTED AS ONE WHOLE, matching the DOS 3.3 side. The removal
//  is staged into a working buffer no caller can see, the new file is placed
//  over that, and only a finished buffer is handed back. A removal APPLIED to
//  the caller's image followed by a placement would free the old file and lose
//  it outright if the placement then failed, which is worse than refusing.
//
//  REPLACEMENT NEEDS DESTROY PERMISSION AS WELL AS WRITE PERMISSION, and that
//  is a consequence worth stating rather than discovering. ProDOS gates the two
//  operations on different bits of the access byte, and a replacement here
//  releases the old file's blocks -- a destroy in every way that matters -- so
//  the staged removal applies its own gate. A file marked writable but not
//  destroyable is therefore refused, which is the conservative answer while
//  this layer rebuilds a file rather than rewriting it in place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    size_t                          payloadSize = payload.bytes.size();
    bool                            fits        = payloadSize <= ProDosVolume::kMaxFileBytes;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    bool                            nameOk      = false;
    bool                            exists      = false;
    bool                            isLocked    = false;
    bool                            fullyParsed = true;
    uint16_t                        owner       = 0;
    Word                            auxType     = 0;
    Word                            caseFlags   = 0;
    std::string                     name;
    vector<Byte>                    staged;
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    DeleteOutcome                   removal;



    //  Binds to `staged` by reference, and is read only once it is filled.
    ProDosVolume           stagedVolume (staged);



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    nameOk = TryEncodeDirectoryName (path.GetLeaf(), name, caseFlags);

    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));
    CBREx (fits,   HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

    hr = ResolveAuxType (payload, auxType);
    CHR (hr);

    //  The file lands in the directory the path walks to, which is the volume
    //  directory for a path of one name.
    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), dirKeyBlock);
    CHR (hr);

    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    exists = ProDosVolume::TryFindEntry (entries, name, owner);

    if (exists)
    {
        isLocked = (entries[owner].access & ProDosVolume::kAccessWriteEnable) == 0;
    }

    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (!exists)
    {
        hr = AddFileWithCase (dirKeyBlock, name, caseFlags, payload.type, auxType, payload.bytes, outBuffer);
        CHR (hr);
    }
    else
    {
        // The removal answers to the same rules a bare delete follows,
        // including its refusal to hand back a block another entry claims. The
        // replacement simply does not get that space.
        hr = m_volume.m_remover.Delete (path, staged, removal);
        CHR (hr);

        hr = stagedVolume.m_writer.AddFileWithCase (dirKeyBlock, name, caseFlags, payload.type, auxType, payload.bytes, outBuffer);
        CHR (hr);
    }

    StampEntry (outBuffer, dirKeyBlock, name, payload);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::StampEntry
//
//  The access byte and dates the payload came with, over the defaults a new
//  record gets, so a copy is locked and dated as its original is. They take
//  no blocks, so the record is changed after the file is placed.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeWriter::StampEntry (vector<Byte> & buffer, int dirKeyBlock, const std::string & name, const FilePayload & payload)
{
    ProDosVolume         written (buffer);
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    bool                            parsed  = true;
    bool                            found   = false;
    uint16_t                        local   = 0;



    if (payload.hasAccess || payload.hasDates)
    {
        written.CollectEntries (dirKeyBlock, entries, damage, parsed);
        found = ProDosVolume::TryFindEntry (entries, name, local);
    }

    if (found && payload.hasAccess)
    {
        WriteByteAt (buffer, entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffAccess, payload.access);
    }

    if (found && payload.hasDates)
    {
        WriteWordAt (buffer, entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffCreated,      payload.createdDate);
        WriteWordAt (buffer, entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffCreated + 2,  payload.createdTime);
        WriteWordAt (buffer, entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffModified,     payload.modifiedDate);
        WriteWordAt (buffer, entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffModified + 2, payload.modifiedTime);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::SetStartupProgram
//
//  ProDOS stores no startup-program name anywhere. The kernel launches the
//  first system program it comes to as it walks the volume directory, so the
//  ORDERING IS THE MECHANISM, and setting the startup program means putting the
//  chosen record in front of every other one the walk would stop at.
//
//  THIS IS DELIBERATELY NOT SHARED WITH THE DOS 3.3 SIDE. The two filesystems do
//  not spell one idea differently: DOS 3.3 patches a name into its own code and
//  ProDOS moves a directory record, and no "write the boot name" helper can be
//  written that is honest about both. One would either patch a name ProDOS never
//  reads or reorder a catalog DOS pays no attention to.
//
//  The two records are SWAPPED rather than the directory being repacked. Every
//  other record keeps its position, its dates and its pointers, which is what
//  makes this an edit a user can reason about -- and what keeps the volume's own
//  tally of entries and every block pointer on it exactly as they were.
//
//  The kernel is excluded from the candidates on purpose: it is a system program
//  too, the boot block finds it by name rather than by position, and nominating
//  it would ask a booted ProDOS to load ProDOS.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    bool                            single      = path.IsSingleComponent();
    bool                            found       = false;
    bool                            launchable  = false;
    bool                            fullyParsed = true;
    uint16_t                        owner       = 0;
    uint16_t                        first       = 0;
    uint16_t                        index       = 0;
    size_t                          i           = 0;
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    vector<Byte>                    result;
    VolumeIntegrityReport           report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    m_volume.CollectEntries (ProDosSkeleton::kDirKeyBlock, entries, damage, fullyParsed);

    found = ProDosVolume::TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    launchable = ProDosVolume::IsLaunchableSystemFile (entries[owner]);
    CBREx (launchable, HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    // Entries arrive in the order the directory chain yields them, which is the
    // order the boot path walks, so the first launchable record here is the one
    // it would launch today.
    first = owner;

    for (index = 0; index < (uint16_t) entries.size(); index++)
    {
        bool  candidate = ProDosVolume::IsLaunchableSystemFile (entries[index]);

        if (candidate)
        {
            first = index;
            break;
        }
    }

    result = m_volume.m_sectors;

    // A file already in front of every other candidate leaves the volume
    // exactly as it was, rather than being rewritten to what it already says.
    if (first != owner)
    {
        for (i = 0; i < (size_t) ProDosSkeleton::kEntryLength; i++)
        {
            Byte  chosen   = m_volume.ReadByte (entries[owner].dirBlock, entries[owner].entryOffset + i);
            Byte  standing = m_volume.ReadByte (entries[first].dirBlock, entries[first].entryOffset + i);

            WriteByteAt (result, entries[first].dirBlock, entries[first].entryOffset + i, chosen);
            WriteByteAt (result, entries[owner].dirBlock, entries[owner].entryOffset + i, standing);
        }
    }

    // The same self-check every other mutating call runs over its own output.
    // A swap moves no block and frees nothing, so a disagreement here would mean
    // the records were not the ones the walk described.
    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::Rename
//
//  The record's name field and its length nibble change; the storage type,
//  the key pointer, the dates and every block stay as they were.
//
//  A SUBDIRECTORY IS REFUSED, for the reason Delete refuses one: its name is
//  held twice, in the parent's record and in its own header block, and this
//  layer does not walk into it to fix the second copy. ProDOS gates renaming
//  on its own access bit, distinct from write and destroy, and that bit is the
//  one consulted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::Rename (
    const FilePath     & from,
    const std::string  & to,
    vector<Byte>       & outBuffer) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    bool                            nameOk      = false;
    bool                            found       = false;
    bool                            taken       = false;
    bool                            isLocked    = false;
    bool                            isDirectory = false;
    bool                            fullyParsed = true;
    uint16_t                        owner       = 0;
    uint16_t                        holder      = 0;
    size_t                          i           = 0;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    Word                            caseFlags   = 0;
    std::string                     name;
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    vector<Byte>                    result;
    VolumeIntegrityReport           report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    nameOk = TryEncodeDirectoryName (to, name, caseFlags);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (from), dirKeyBlock);
    CHR (hr);

    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    found = ProDosVolume::TryFindEntry (entries, from.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    isDirectory = entries[owner].storage == ProDosSkeleton::kStorageSubdir;
    isLocked    = (entries[owner].access & ProDosVolume::kAccessRenameEnable) == 0;

    CBREx (!isDirectory, HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED));
    CBREx (!isLocked,    HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    taken = ProDosVolume::TryFindEntry (entries, name, holder) && holder != owner;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_volume.m_sectors;

    WriteByteAt (result,
                 entries[owner].dirBlock,
                 entries[owner].entryOffset + ProDosSkeleton::kEntOffTypeName,
                 (Byte) (entries[owner].storage | (Byte) name.size()));

    for (i = 0; i < ProDosSkeleton::kVolumeNameBytes; i++)
    {
        Byte  c = (i < name.size()) ? (Byte) name[i] : (Byte) 0;

        WriteByteAt (result,
                     entries[owner].dirBlock,
                     entries[owner].entryOffset + ProDosSkeleton::kEntOffName + i,
                     c);
    }

    //  The new name's own case, recorded where a IIgs reads it.
    WriteWordAt (result,
                 entries[owner].dirBlock,
                 entries[owner].entryOffset + ProDosSkeleton::kEntOffCaseFlags,
                 caseFlags);

    // The same self-check every other mutating call runs over its own output.
    // A rename moves no block, so a disagreement would mean the name landed
    // outside the record it was meant for.
    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeWriter::SetAttributes
//
//  Each field of the record whose has flag is set, and nothing else: no block
//  moves. A subdirectory's type can't change, since it is what makes it one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolumeWriter::SetAttributes (
    const FilePath        & path,
    const FileAttributes  & attributes,
    vector<Byte>          & outBuffer) const
{
    HRESULT                         hr          = S_OK;
    size_t                          bufferBytes = m_volume.m_sectors.size();
    bool                            found       = false;
    bool                            isDirectory = false;
    bool                            fullyParsed = true;
    uint16_t                        owner       = 0;
    int                             dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    int                             block       = 0;
    size_t                          entry       = 0;
    vector<ProDosVolume::RawEntry>  entries;
    vector<std::string>             damage;
    vector<Byte>                    result;
    VolumeIntegrityReport           report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    hr = m_volume.ResolveDirectory (ProDosVolume::GetParentPath (path), dirKeyBlock);
    CHR (hr);

    m_volume.CollectEntries (dirKeyBlock, entries, damage, fullyParsed);

    found = ProDosVolume::TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    isDirectory = entries[owner].storage == ProDosSkeleton::kStorageSubdir;
    CBREx (!isDirectory || !attributes.hasType, HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED));

    hr = m_volume.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_volume.m_sectors;
    block  = entries[owner].dirBlock;
    entry  = entries[owner].entryOffset;

    if (attributes.hasType)
    {
        WriteByteAt (result, block, entry + ProDosSkeleton::kEntOffFileType, attributes.type);
    }

    if (attributes.hasAuxType)
    {
        WriteWordAt (result, block, entry + ProDosSkeleton::kEntOffAuxType, attributes.auxType);
    }

    if (attributes.hasAccess)
    {
        WriteByteAt (result, block, entry + ProDosSkeleton::kEntOffAccess, attributes.access);
    }

    if (attributes.hasCreated)
    {
        WriteWordAt (result, block, entry + ProDosSkeleton::kEntOffCreated,     attributes.createdDate);
        WriteWordAt (result, block, entry + ProDosSkeleton::kEntOffCreated + 2, attributes.createdTime);
    }

    if (attributes.hasModified)
    {
        WriteWordAt (result, block, entry + ProDosSkeleton::kEntOffModified,     attributes.modifiedDate);
        WriteWordAt (result, block, entry + ProDosSkeleton::kEntOffModified + 2, attributes.modifiedTime);
    }

    hr = ProDosVolume::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}
