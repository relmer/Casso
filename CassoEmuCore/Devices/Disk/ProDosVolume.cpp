#include "Pch.h"

#include "ProDosVolume.h"
#include "ProDosSkeleton.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ProDosVolume
//
////////////////////////////////////////////////////////////////////////////////

ProDosVolume::ProDosVolume (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ReadByte
//
//  Refuses a block the volume cannot hold rather than indexing past the end of
//  the buffer. Block numbers multiply straight into a byte offset, so an
//  out-of-range one is not a wrong value -- it is a read outside the image.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProDosVolume::ReadByte (int block, size_t offset) const
{
    size_t  at = 0;



    if (block < 0 || block >= ProDosSkeleton::kTotalBlocks)
    {
        return 0;
    }

    at = ProDosSkeleton::BlockByteOffset (block, offset);

    if (at >= m_sectors.size())
    {
        return 0;
    }

    return m_sectors[at];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word ProDosVolume::ReadWord (int block, size_t offset) const
{
    return (Word) (ReadByte (block, offset) | (ReadByte (block, offset + 1) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::CollectEntries
//
//  Walks the volume directory chain, collecting every readable entry.
//
//  The first record of the key block is the volume header rather than a file,
//  so it is skipped. A record whose storage type is zero is an unused slot, not
//  the end of the directory -- ProDOS reuses slots in place, so a deleted file
//  leaves a hole that later entries sit beyond.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::CollectEntries (
    vector<RawEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    int             dirBlock = ProDosSkeleton::kDirKeyBlock;
    int             n        = 0;
    size_t          i        = 0;



    outFullyParsed = true;

    while (dirBlock != 0)
    {
        bool  inRange = dirBlock > 0 && dirBlock < ProDosSkeleton::kTotalBlocks;
        bool  stepOk  = inRange && guard.TryVisit ((uint32_t) dirBlock);
        int   first   = (dirBlock == ProDosSkeleton::kDirKeyBlock) ? 1 : 0;

        if (!stepOk)
        {
            outDamage.push_back ("the volume directory chain loops or leaves the volume; "
                                 "the entries listed are those reachable before it did");
            outFullyParsed = false;
            break;
        }

        for (n = first; n < (int) ProDosSkeleton::kEntriesPerBlock; n++)
        {
            RawEntry  entry;
            size_t    at      = ProDosSkeleton::kOffFirstEntry
                              + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte      typeLen = ReadByte (dirBlock, at);
            size_t    len     = (size_t) (typeLen & 0x0F);

            // A storage-type nibble of zero is an INACTIVE record -- never used
            // or deleted -- not the end of the directory. ProDOS reuses slots
            // in place, so a deleted file leaves a hole that later entries sit
            // beyond, and a deleted one keeps its name, so the name cannot be
            // what decides.
            if ((typeLen & 0xF0) == ProDosSkeleton::kStorageInactive)
            {
                continue;
            }

            entry.dirBlock    = dirBlock;
            entry.entryOffset = at;
            entry.storage     = (Byte) (typeLen & 0xF0);
            entry.fileType    = ReadByte (dirBlock, at + ProDosSkeleton::kEntOffFileType);
            entry.keyPointer  = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffKeyPointer);
            entry.blocksUsed  = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffBlocksUsed);
            entry.access      = ReadByte (dirBlock, at + ProDosSkeleton::kEntOffAccess);
            entry.auxType     = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffAuxType);

            entry.eof = (uint32_t)
                (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof)
              | (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof + 1) << 8)
              | (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof + 2) << 16));

            for (i = 0; i < len; i++)
            {
                entry.name += (char) ReadByte (dirBlock, at + ProDosSkeleton::kEntOffName + i);
            }

            outEntries.push_back (entry);
        }

        dirBlock = ReadWord (dirBlock, ProDosSkeleton::kOffNextBlock);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::CollectFileBlocks
//
//  Every block an entry occupies, index blocks included -- they are as
//  allocated as the data they point at, and a report that omitted them would
//  call every volume inconsistent.
//
//  Sparse holes are zero pointers and legitimate: ProDOS writes them for a file
//  with gaps, and they occupy nothing. Anything else out of range is corruption,
//  and following it would read outside the image.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::CollectFileBlocks (
    const RawEntry    & entry,
    vector<uint32_t>  & outBlocks,
    ChainWalkGuard    & guard) const
{
    bool  ok       = true;
    bool  keyInUse = entry.keyPointer != 0
                  && entry.keyPointer < (Word) ProDosSkeleton::kTotalBlocks;
    int   i        = 0;



    if (!keyInUse)
    {
        return false;
    }

    ok = guard.TryVisit ((uint32_t) entry.keyPointer);

    if (!ok)
    {
        return false;
    }

    outBlocks.push_back ((uint32_t) entry.keyPointer);

    // A seedling's key block IS its data, so there is nothing further to walk.
    if (entry.storage == ProDosSkeleton::kStorageSeedling)
    {
        return true;
    }

    if (entry.storage == ProDosSkeleton::kStorageSapling)
    {
        for (i = 0; ok && i < (int) ProDosSkeleton::kPointersPerIndex; i++)
        {
            Word  block = (Word) (ReadByte (entry.keyPointer, (size_t) i)
                               | (ReadByte (entry.keyPointer,
                                      (size_t) i + ProDosSkeleton::kPointersPerIndex) << 8));

            if (block == 0)
            {
                continue;   // sparse hole
            }

            if (block >= (Word) ProDosSkeleton::kTotalBlocks)
            {
                ok = false;
                break;
            }

            ok = guard.TryVisit ((uint32_t) block);

            if (ok)
            {
                outBlocks.push_back ((uint32_t) block);
            }
        }

        return ok;
    }

    if (entry.storage == ProDosSkeleton::kStorageTree)
    {
        for (i = 0; ok && i < (int) ProDosSkeleton::kPointersPerIndex; i++)
        {
            Word  indexBlock = (Word) (ReadByte (entry.keyPointer, (size_t) i)
                                    | (ReadByte (entry.keyPointer,
                                           (size_t) i + ProDosSkeleton::kPointersPerIndex) << 8));
            int   j          = 0;

            if (indexBlock == 0)
            {
                continue;
            }

            if (indexBlock >= (Word) ProDosSkeleton::kTotalBlocks)
            {
                ok = false;
                break;
            }

            ok = guard.TryVisit ((uint32_t) indexBlock);

            if (!ok)
            {
                break;
            }

            outBlocks.push_back ((uint32_t) indexBlock);

            for (j = 0; ok && j < (int) ProDosSkeleton::kPointersPerIndex; j++)
            {
                Word  block = (Word) (ReadByte (indexBlock, (size_t) j)
                                   | (ReadByte (indexBlock,
                                          (size_t) j + ProDosSkeleton::kPointersPerIndex) << 8));

                if (block == 0)
                {
                    continue;
                }

                if (block >= (Word) ProDosSkeleton::kTotalBlocks)
                {
                    ok = false;
                    break;
                }

                ok = guard.TryVisit ((uint32_t) block);

                if (ok)
                {
                    outBlocks.push_back ((uint32_t) block);
                }
            }
        }

        return ok;
    }

    // A directory or an unrecognized storage type: the key block is claimed,
    // but this layer does not walk further into it yet.
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT           hr          = S_OK;
    size_t            bufferBytes = m_sectors.size();
    vector<RawEntry>  entries;
    bool              fullyParsed = true;
    size_t            i           = 0;
    int               bit         = 0;
    uint32_t          freeBlocks  = 0;
    Byte              headerType  = 0;
    size_t            nameLen     = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    outListing = VolumeListing();

    // The volume header is the key block's first record.
    headerType = ReadByte (ProDosSkeleton::kDirKeyBlock, ProDosSkeleton::kOffFirstEntry);
    nameLen    = (size_t) (headerType & 0x0F);

    if ((headerType & 0xF0) == ProDosSkeleton::kStorageVolumeDir)
    {
        for (i = 0; i < nameLen; i++)
        {
            outListing.volumeName += (char) ReadByte (ProDosSkeleton::kDirKeyBlock,
                ProDosSkeleton::kOffFirstEntry + ProDosSkeleton::kHdrOffName + i);
        }

        outListing.hasVolumeName = true;
    }

    outListing.totalUnits = (uint32_t) ProDosSkeleton::kTotalBlocks;

    CollectEntries (entries, outListing.damage, fullyParsed);

    for (const RawEntry & entry : entries)
    {
        FileEntry  listed;

        listed.name           = entry.name;
        listed.type           = entry.fileType;
        listed.isLocked       = (entry.access & kAccessWriteEnable) == 0;
        listed.isDirectory    = entry.storage == ProDosSkeleton::kStorageSubdir;
        listed.sizeUnits      = entry.blocksUsed;
        listed.eofBytes       = entry.eof;
        listed.hasEofBytes    = true;
        listed.auxType        = entry.auxType;
        listed.hasAuxType     = true;

        // For a binary, the auxiliary type IS the load address. Naming it as
        // such saves every caller from knowing that.
        if (entry.fileType == kTypeBinary)
        {
            listed.loadAddress    = entry.auxType;
            listed.hasLoadAddress = true;
        }

        outListing.entries.push_back (listed);
    }

    // Volume bitmap: one bit per block, MSB of byte 0 is block 0, SET is free.
    for (i = 0; i < (size_t) (ProDosSkeleton::kTotalBlocks / 8); i++)
    {
        Byte  bits = ReadByte (ProDosSkeleton::kBitmapBlock, i);

        for (bit = 0; bit < 8; bit++)
        {
            freeBlocks += (uint32_t) ((bits >> bit) & 1);
        }
    }

    outListing.freeUnits = freeBlocks;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Read
//
//  Extraction goes through the existing reader rather than a second walk of the
//  same structures: two implementations of one format drift, and the one that
//  drifts silently is the one fewer people run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT  hr          = S_OK;
    size_t   bufferBytes = m_sectors.size();
    bool     single      = path.IsSingleComponent();
    Byte     fileType    = 0;
    Word     auxType     = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    // Subdirectory traversal is not built yet. Refusing a deeper path keeps the
    // capability fillable later without any caller having been handed the wrong
    // file in the meantime.
    CBREx (single, E_INVALIDARG);

    outPayload = FilePayload();

    hr = ProDosReader::ExtractFile (m_sectors, path.GetLeaf(), outPayload.bytes,
                                    fileType, auxType);
    CHR (hr);

    outPayload.type       = fileType;
    outPayload.auxType    = auxType;
    outPayload.hasAuxType = true;
    outPayload.encoding   = PayloadEncoding::Verbatim;

    if (fileType == kTypeBinary)
    {
        outPayload.loadAddress    = auxType;
        outPayload.hasLoadAddress = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::BuildIntegrityReport
//
//  Every entry's blocks walked into the claim map, then compared against the
//  volume bitmap. The directory blocks and the bitmap block itself are claimed
//  by the volume rather than by any file, so they are recorded under a reserved
//  owner -- otherwise every volume would report its own structures as space
//  allocated to nobody.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    //  Blocks 0-1 are the boot blocks, 2-5 the volume directory, 6 the bitmap.
    constexpr uint32_t  kReservedBlocks = 7;

    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_sectors.size();
    vector<RawEntry>     entries;
    vector<std::string>  damage;
    bool                 fullyParsed = true;
    uint16_t             owner       = 0;
    uint32_t             block       = 0;
    size_t               i           = 0;
    int                  bit         = 0;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    outReport.Reset ((uint32_t) ProDosSkeleton::kTotalBlocks);

    CollectEntries (entries, damage, fullyParsed);
    outReport.SetCatalogFullyParsed (fullyParsed);

    for (block = 0; block < kReservedBlocks; block++)
    {
        outReport.AddClaim (block, VolumeIntegrityReport::kVolumeOwner);
    }

    for (owner = 0; owner < (uint16_t) entries.size(); owner++)
    {
        ChainWalkGuard    guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
        vector<uint32_t>  blocks;
        bool              walked = CollectFileBlocks (entries[owner], blocks, guard);

        for (uint32_t claimed : blocks)
        {
            outReport.AddClaim (claimed, owner);
        }

        if (!walked)
        {
            outReport.MarkChainUnfollowable (owner);
        }
    }

    for (i = 0; i < (size_t) (ProDosSkeleton::kTotalBlocks / 8); i++)
    {
        Byte  bits = ReadByte (ProDosSkeleton::kBitmapBlock, i);

        for (bit = 0; bit < 8; bit++)
        {
            bool  isFree = ((bits >> bit) & 1) != 0;

            outReport.SetAllocatedInFreeMap ((uint32_t) (i * 8 + (size_t) (7 - bit)), !isFree);
        }
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::WriteByteAt
//
//  Refuses a block the volume cannot hold rather than indexing past the end of
//  the buffer, for the same reason ReadByte does.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::WriteByteAt (vector<Byte> & buffer, int block, size_t offset, Byte value)
{
    size_t  at = 0;



    if (block < 0 || block >= ProDosSkeleton::kTotalBlocks)
    {
        return;
    }

    at = ProDosSkeleton::BlockByteOffset (block, offset);

    if (at < buffer.size())
    {
        buffer[at] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::WriteWordAt
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::WriteWordAt (vector<Byte> & buffer, int block, size_t offset, Word value)
{
    WriteByteAt (buffer, block, offset,     (Byte) (value & 0xFF));
    WriteByteAt (buffer, block, offset + 1, (Byte) (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::IsFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::IsFreeInBitmap (const vector<Byte> & buffer, uint32_t block)
{
    size_t  at   = 0;
    Byte    mask = (Byte) (0x80 >> (block % 8));



    if (block >= (uint32_t) ProDosSkeleton::kTotalBlocks)
    {
        return false;
    }

    at = ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kBitmapBlock, (size_t) (block / 8));

    return at < buffer.size() && (buffer[at] & mask) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::SetFreeInBitmap
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::SetFreeInBitmap (vector<Byte> & buffer, uint32_t block, bool isFree)
{
    size_t  at   = 0;
    Byte    mask = (Byte) (0x80 >> (block % 8));
    Byte    bits = 0;



    if (block >= (uint32_t) ProDosSkeleton::kTotalBlocks)
    {
        return;
    }

    at = ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kBitmapBlock, (size_t) (block / 8));

    if (at >= buffer.size())
    {
        return;
    }

    bits       = buffer[at];
    buffer[at] = isFree ? (Byte) (bits | mask) : (Byte) (bits & ~mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::TryEncodeDirectoryName
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

bool ProDosVolume::TryEncodeDirectoryName (const std::string & name, std::string & outName)
{
    size_t  length = name.size();
    size_t  i      = 0;
    Byte    first  = 0;



    outName.clear();

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

            return false;
        }

        // The guest's keyboard produces upper case and its directory stores it,
        // so a lower-case name would list as something nobody can type.
        outName += (char) ((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::StorageTypeFor
//
//  A seedling IS its data block. A sapling adds one index block of up to 256
//  pointers above it. Past that the file needs a master index of index blocks,
//  which is a tree -- and it is that growth, not the first two, that a writer
//  handling only small files never exercises.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProDosVolume::StorageTypeFor (size_t dataBlockCount)
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
//  ProDosVolume::OverheadBlocksFor
//
//  How many blocks the structure costs on top of the data itself. This is what
//  makes the difference between the recorded block count and the file's length
//  -- an index block is as allocated as the bytes it points at.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosVolume::OverheadBlocksFor (size_t dataBlockCount)
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
//  ProDosVolume::WriteIndexPointer
//
//  An index block holds 256 low bytes followed by the 256 matching high bytes,
//  rather than 256 little-endian pairs. Writing it as pairs produces a file
//  that reads back correctly for any block number below 256 and silently
//  wrongly above it, which on a 280-block volume is most of the disk.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::WriteIndexPointer (
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
//  ProDosVolume::TryFindEntry
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::TryFindEntry (
    const vector<RawEntry>  & entries,
    const std::string       & leaf,
    uint16_t                & outOwner)
{
    uint16_t  index = 0;



    for (index = 0; index < (uint16_t) entries.size(); index++)
    {
        if (_stricmp (entries[index].name.c_str(), leaf.c_str()) == 0)
        {
            outOwner = index;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::TryFindFreeDirectorySlot
//
//  The first record in the volume directory chain whose storage-type nibble is
//  zero. That nibble is ProDOS's own statement of occupancy, so unlike the
//  DOS 3.3 catalog -- where an entry claiming no sectors is still an entry --
//  there is nothing here to infer from what the record points at.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::TryFindFreeDirectorySlot (int & outBlock, size_t & outOffset) const
{
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    int             dirBlock = ProDosSkeleton::kDirKeyBlock;
    int             n        = 0;



    while (dirBlock != 0)
    {
        bool  inRange = dirBlock > 0 && dirBlock < ProDosSkeleton::kTotalBlocks;
        bool  stepOk  = inRange && guard.TryVisit ((uint32_t) dirBlock);
        int   first   = (dirBlock == ProDosSkeleton::kDirKeyBlock) ? 1 : 0;

        if (!stepOk)
        {
            break;
        }

        for (n = first; n < (int) ProDosSkeleton::kEntriesPerBlock; n++)
        {
            size_t  at      = ProDosSkeleton::kOffFirstEntry
                            + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte    typeLen = ReadByte (dirBlock, at);

            if ((typeLen & 0xF0) == ProDosSkeleton::kStorageInactive)
            {
                outBlock  = dirBlock;
                outOffset = at;

                return true;
            }
        }

        dirBlock = ReadWord (dirBlock, ProDosSkeleton::kOffNextBlock);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::TryAllocateBlocks
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

bool ProDosVolume::TryAllocateBlocks (
    const VolumeIntegrityReport  & report,
    size_t                         count,
    vector<uint32_t>             & outBlocks) const
{
    uint32_t  block = 0;
    size_t    taken = 0;



    outBlocks.clear();

    for (block = 0; block < (uint32_t) ProDosSkeleton::kTotalBlocks && taken < count; block++)
    {
        bool  isFree  = IsFreeInBitmap (m_sectors, block);
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
//  ProDosVolume::ZeroBlocks
//
//  Every block a new file occupies is cleared first. An index block still
//  holding a previous file's pointers would be read as this file's structure,
//  and the tail of a partly filled last block would hand back bytes nobody
//  ever wrote.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::ZeroBlocks (vector<Byte> & buffer, const vector<uint32_t> & blocks)
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
//  ProDosVolume::PlaceFile
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

HRESULT ProDosVolume::PlaceFile (
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
//  ProDosVolume::WriteDirectoryEntry
//
//  The whole record is cleared before any field is written. A slot being reused
//  after a delete still carries the previous file's dates and version bytes,
//  and leaving them would attach one file's history to another.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::WriteDirectoryEntry (
    vector<Byte>       & buffer,
    int                  dirBlock,
    size_t               entryOffset,
    const std::string  & name,
    Byte                 storage,
    Byte                 fileType,
    Word                 keyBlock,
    Word                 blocksUsed,
    uint32_t             eof,
    Word                 auxType)
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
    WriteByteAt (buffer, dirBlock, entryOffset + ProDosSkeleton::kEntOffHeaderPointer,
                 (Byte) ProDosSkeleton::kDirKeyBlock);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::AdjustFileCount
//
//  The volume header's own tally of active entries. It is not what enumeration
//  reads -- that walks the records -- but ProDOS itself reads it, so a volume
//  whose tally disagrees with its directory is one the guest mounts oddly.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::AdjustFileCount (vector<Byte> & buffer, int delta)
{
    size_t  countAt = ProDosSkeleton::kOffFirstEntry + ProDosSkeleton::kHdrOffFileCount;
    size_t  at      = ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt);
    size_t  atHigh  = ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt + 1);
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

    WriteWordAt (buffer, ProDosSkeleton::kDirKeyBlock, countAt, (Word) count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ResolveAuxType
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

HRESULT ProDosVolume::ResolveAuxType (const FilePayload & payload, Word & outAuxType)
{
    HRESULT  hr       = S_OK;
    bool     isBinary = payload.type == kTypeBinary;
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
//  ProDosVolume::HandBackVerifiedResult
//
//  A write that never inspects what it produced is how a decoder shipped
//  returning success over a buffer it had partly zeroed. Every computed result
//  here is read back through the same integrity pass a user's disk goes
//  through, refused if the edit made the volume worse, and handed over only
//  once it survives that.
//
//  WORSE, not imperfect. Asking whether the result is CLEAN would refuse two
//  correct outcomes: a delete that declines to free a cross-linked block leaves
//  space allocated with nothing claiming it, which is exactly what the leak rule
//  requires, and a volume that already carried a disagreement would become
//  permanently uneditable -- on the degraded disks this feature exists to
//  recover. The comparison is against the input, on the sets that matter.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::HandBackVerifiedResult (
    const VolumeIntegrityReport  & before,
    const vector<Byte>           & result,
    vector<Byte>                 & outBuffer)
{
    HRESULT                hr   = S_OK;
    ProDosVolume           volume (result);
    bool                   safe = false;
    VolumeIntegrityReport  after;



    hr = volume.BuildIntegrityReport (after);
    CHRA (hr);

    safe = after.IsSafeToCommitAfter (before);
    CBRAEx (safe, E_UNEXPECTED);

    outBuffer = result;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::AddFile
//
//  Allocate from the volume bitmap, build whatever index structure the size
//  calls for, write the data blocks, create the directory entry, and leave the
//  bitmap agreeing with what was allocated.
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

HRESULT ProDosVolume::AddFile (
    const std::string   & name,
    Byte                  fileType,
    Word                  auxType,
    const vector<Byte>  & bytes,
    vector<Byte>        & outBuffer) const
{
    HRESULT                hr           = S_OK;
    size_t                 payloadSize  = bytes.size();
    bool                   slotOk       = false;
    bool                   allocated    = false;
    bool                   haveKeyBlock = false;
    int                    slotBlock    = 0;
    size_t                 slotOffset   = 0;
    size_t                 dataBlocks   = 0;
    size_t                 overhead     = 0;
    vector<Byte>           result;
    vector<uint32_t>       blocks;
    VolumeIntegrityReport  report;



    slotOk = TryFindFreeDirectorySlot (slotBlock, slotOffset);
    CBREx (slotOk, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    hr = BuildIntegrityReport (report);
    CHRA (hr);

    dataBlocks = (payloadSize + (size_t) ProDosSkeleton::kBlockByteSize - 1)
               / (size_t) ProDosSkeleton::kBlockByteSize;

    // A file of no bytes still occupies its key block, which is what makes it a
    // file rather than a name.
    if (dataBlocks == 0)
    {
        dataBlocks = 1;
    }

    overhead  = OverheadBlocksFor (dataBlocks);
    allocated = TryAllocateBlocks (report, dataBlocks + overhead, blocks);

    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    haveKeyBlock = !blocks.empty();
    CBRA (haveKeyBlock);

    result = m_sectors;

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
                         StorageTypeFor (dataBlocks),
                         fileType,
                         (Word) blocks[0],
                         (Word) blocks.size(),
                         (uint32_t) payloadSize,
                         auxType);

    AdjustFileCount (result, 1);

    hr = HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Write
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

HRESULT ProDosVolume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    HRESULT                hr           = S_OK;
    size_t                 bufferBytes  = m_sectors.size();
    size_t                 payloadSize  = payload.bytes.size();
    bool                   single       = path.IsSingleComponent();
    bool                   fits         = payloadSize <= kMaxFileBytes;
    bool                   nameOk       = false;
    bool                   exists       = false;
    bool                   isLocked     = false;
    bool                   fullyParsed  = true;
    uint16_t               owner        = 0;
    Word                   auxType      = 0;
    std::string            name;
    vector<Byte>           staged;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    DeleteOutcome          removal;

    //  Binds to `staged` by reference, and is read only once it is filled.
    ProDosVolume           stagedVolume (staged);



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    nameOk = single && TryEncodeDirectoryName (path.GetLeaf(), name);

    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));
    CBREx (fits,   HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

    hr = ResolveAuxType (payload, auxType);
    CHR (hr);

    CollectEntries (entries, damage, fullyParsed);

    exists = TryFindEntry (entries, name, owner);

    if (exists)
    {
        isLocked = (entries[owner].access & kAccessWriteEnable) == 0;
    }

    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (!exists)
    {
        hr = AddFile (name, payload.type, auxType, payload.bytes, outBuffer);
        CHR (hr);
    }
    else
    {
        // The removal answers to the same rules a bare delete follows,
        // including its refusal to hand back a block another entry claims. The
        // replacement simply does not get that space.
        hr = Delete (path, staged, removal);
        CHR (hr);

        hr = stagedVolume.AddFile (name, payload.type, auxType, payload.bytes, outBuffer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::AppendDeleteWarnings
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::AppendDeleteWarnings (DeleteOutcome & inOutOutcome)
{
    size_t  leaked = inOutOutcome.leakedUnits.size();



    if (leaked > 0)
    {
        inOutOutcome.warnings.push_back (
            std::to_string (leaked)
          + " block(s) this file referenced are claimed by another directory entry"
            " as well, and were left allocated rather than freed");
    }

    if (inOutOutcome.chainWasDamaged)
    {
        inOutOutcome.warnings.push_back (
            "this file's index structure could not be followed to its end, so only"
            " the blocks the walk reached were considered");
    }

    if (!inOutOutcome.catalogFullyParsed)
    {
        inOutOutcome.warnings.push_back (
            "the directory did not parse completely, so an entry that could not be"
            " read claims nothing this pass can see -- a block it shares with the"
            " deleted file may have been freed while that entry still uses it");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Delete
//
//  The interface form. Everything it decided is available from the overload
//  below; this one discards it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Delete
//
//  Removes a directory entry and returns ONLY the blocks the integrity report
//  shows this entry alone claims.
//
//  That restriction is the whole point. Freeing a block a second file also
//  references does not fail: the delete succeeds, the volume looks healthy, and
//  the other file is destroyed the next time anything allocates -- possibly
//  weeks later, with nothing connecting the two events. Space left allocated
//  with nothing owning it is recoverable at any time; a block handed out from
//  under a live file is not. So everything not uniquely owned is reported as
//  leaked and left exactly where it is.
//
//  A FILE WHOSE INDEX STRUCTURE IS DAMAGED IS STILL DELETABLE, for the same
//  reason as on DOS 3.3: refusing would strand the volume, and the developer
//  this serves is working with precisely those disks.
//
//  A SUBDIRECTORY IS REFUSED. This layer does not walk into one, so the report
//  credits it with its key block and nothing else -- deleting it would free
//  that one block and silently orphan everything beneath. Refusing is honest
//  until traversal exists.
//
//  The tombstone is ProDOS's own: the storage-type nibble goes to zero and the
//  name length and name stay behind, which is how an undelete tool finds the
//  file again. Nothing may key off the name to decide whether a record is live.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Delete (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    HRESULT                hr          = S_OK;
    size_t                 bufferBytes = m_sectors.size();
    bool                   single      = path.IsSingleComponent();
    bool                   found       = false;
    bool                   isLocked    = false;
    bool                   isDirectory = false;
    bool                   fullyParsed = true;
    uint16_t               owner       = 0;
    Byte                   typeLen     = 0;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    vector<Byte>           result;
    VolumeIntegrityReport  report;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    CollectEntries (entries, damage, fullyParsed);

    found = TryFindEntry (entries, path.GetLeaf(), owner);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    // ProDOS gates removal on destroy-enable, not on write-enable, and the
    // access byte can carry one without the other.
    isLocked    = (entries[owner].access & kAccessDestroyEnable) == 0;
    isDirectory = entries[owner].storage == ProDosSkeleton::kStorageSubdir;

    // Being a directory is reported FIRST, and the order is not arbitrary. The
    // subdirectories on Merlin's own ProDOS disk have destroy-enable clear, so
    // testing the lock first tells the user their file is protected -- sending
    // them off to unlock something that would still be refused afterwards. The
    // capability refusal is the true one and belongs in front of the permission
    // refusal.
    CBREx (!isDirectory, HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED));
    CBREx (!isLocked,    HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = BuildIntegrityReport (report);
    CHRA (hr);

    outOutcome                    = DeleteOutcome();
    outOutcome.catalogFullyParsed = report.IsCatalogFullyParsed();

    result = m_sectors;

    // The report walks the same directory in the same order over the same
    // buffer, so an owner index names the same entry on both sides.
    for (uint32_t block : report.GetClaimsOf (owner))
    {
        bool  mineAlone = report.IsUniquelyOwnedBy (block, owner);

        if (mineAlone)
        {
            SetFreeInBitmap (result, block, true);
            outOutcome.freedUnits.push_back (block);
        }
        else
        {
            outOutcome.leakedUnits.push_back (block);
        }
    }

    for (uint16_t broken : report.GetUnfollowableChains())
    {
        if (broken == owner)
        {
            outOutcome.chainWasDamaged = true;
        }
    }

    typeLen = ReadByte (entries[owner].dirBlock,
                        entries[owner].entryOffset + ProDosSkeleton::kEntOffTypeName);

    WriteByteAt (result,
                 entries[owner].dirBlock,
                 entries[owner].entryOffset + ProDosSkeleton::kEntOffTypeName,
                 (Byte) (typeLen & 0x0F));

    AdjustFileCount (result, -1);
    AppendDeleteWarnings (outOutcome);

    hr = HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
}
