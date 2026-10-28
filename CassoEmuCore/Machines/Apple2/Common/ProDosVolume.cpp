#include "Pch.h"

#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Utils.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





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

    at = ProDosSkeleton::GetBlockByteOffset (block, offset);

    if (at >= m_sectors.size())
    {
        return 0;
    }

    return AccessBlock (block)[at];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::AccessBlock
//
////////////////////////////////////////////////////////////////////////////////

const vector<Byte> & ProDosVolume::AccessBlock (int block) const
{
    ProDosReader::NoteBlockRead (m_readObserver, block);

    return m_sectors;
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
//  Walks one directory's chain from its key block, collecting every readable
//  entry.
//
//  The first record of the key block is the directory's header rather than a
//  file, in a subdirectory as in the volume directory, so it is skipped. A record whose storage type is zero is an unused slot, not
//  the end of the directory -- ProDOS reuses slots in place, so a deleted file
//  leaves a hole that later entries sit beyond.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::CollectEntries (
    int                   keyBlock,
    vector<RawEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
    int             dirBlock = keyBlock;
    int             n        = 0;
    size_t          i        = 0;



    outFullyParsed = true;

    while (dirBlock != 0)
    {
        bool  inRange = dirBlock > 0 && dirBlock < ProDosSkeleton::kTotalBlocks;
        bool  stepOk  = inRange && guard.TryVisit ((uint32_t) dirBlock);
        int   first   = (dirBlock == keyBlock) ? 1 : 0;

        if (!stepOk)
        {
            outDamage.push_back ("the directory chain loops or leaves the volume; "
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
            entry.modDate     = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffModified);
            entry.modTime     = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffModified + 2);
            entry.caseFlags   = ReadWord (dirBlock, at + ProDosSkeleton::kEntOffCaseFlags);

            entry.eof = (uint32_t)
                (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof)
              | (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof + 1) << 8)
              | (ReadByte (dirBlock, at + ProDosSkeleton::kEntOffEof + 2) << 16));

            for (i = 0; i < len; i++)
            {
                entry.name += (char) ReadByte (dirBlock, at + ProDosSkeleton::kEntOffName + i);
            }

            entry.name = ApplyCaseFlags (entry.name, entry.caseFlags);

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
//  ProDosVolume::ToFileEntry
//
////////////////////////////////////////////////////////////////////////////////

FileEntry ProDosVolume::ToFileEntry (const RawEntry & entry)
{
    FileEntry  listed;



    listed.name        = entry.name;
    listed.type        = entry.fileType;
    listed.isLocked    = (entry.access & kAccessWriteEnable) == 0;
    listed.isDirectory = entry.storage == ProDosSkeleton::kStorageSubdir;
    listed.sizeUnits   = entry.blocksUsed;
    listed.eofBytes    = entry.eof;
    listed.hasEofBytes = true;
    listed.auxType     = entry.auxType;
    listed.hasAuxType  = true;
    listed.hasModified = TryToUnixTime (entry.modDate, entry.modTime, listed.modifiedUnix);

    // For a binary, the auxiliary type IS the load address. Naming it as such
    // saves every caller from knowing that.
    if (entry.fileType == kTypeBinary)
    {
        listed.loadAddress    = entry.auxType;
        listed.hasLoadAddress = true;
    }

    return listed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ResolveDirectory
//
//  Each component must name a subdirectory in the directory above it. The
//  blocks visited on the way down are guarded, so a directory that points back
//  at one it sits inside cannot walk forever.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::ResolveDirectory (const FilePath & directory, int & outKeyBlock) const
{
    HRESULT              hr          = S_OK;
    int                  keyBlock    = ProDosSkeleton::kDirKeyBlock;
    vector<RawEntry>     entries;
    vector<std::string>  damage;
    bool                 fullyParsed = true;
    bool                 found       = false;
    bool                 isDirectory = false;
    bool                 stepOk      = false;
    uint16_t             owner       = 0;
    ChainWalkGuard       guard ((uint32_t) ProDosSkeleton::kTotalBlocks);



    for (const std::string & component : directory.GetComponents())
    {
        entries.clear();
        CollectEntries (keyBlock, entries, damage, fullyParsed);

        found = TryFindEntry (entries, component, owner);
        CBREx (found, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

        isDirectory = entries[owner].storage == ProDosSkeleton::kStorageSubdir;
        CBREx (isDirectory, HRESULT_FROM_WIN32 (ERROR_DIRECTORY));

        keyBlock = (int) entries[owner].keyPointer;
        stepOk   = keyBlock > 0 && keyBlock < ProDosSkeleton::kTotalBlocks && guard.TryVisit ((uint32_t) keyBlock);
        CBREx (stepOk, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));
    }

    outKeyBlock = keyBlock;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::GetParentPath
//
////////////////////////////////////////////////////////////////////////////////

FilePath ProDosVolume::GetParentPath (const FilePath & path)
{
    std::string  text;
    size_t       i    = 0;



    for (i = 0; i + 1 < path.GetComponents().size(); i++)
    {
        text += (text.empty() ? "" : "/") + path.GetComponents()[i];
    }

    return FilePath::Parse (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::EnumerateDirectory
//
//  The volume's name and free space describe the whole volume; only the
//  entries come from the directory the path names.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const
{
    HRESULT           hr          = S_OK;
    int               keyBlock    = ProDosSkeleton::kDirKeyBlock;
    vector<RawEntry>  entries;
    bool              fullyParsed = true;



    hr = Enumerate (outListing);
    CHR (hr);

    if (directory.IsEmpty())
    {
        return hr;
    }

    hr = ResolveDirectory (directory, keyBlock);
    CHR (hr);

    outListing.entries.clear();
    CollectEntries (keyBlock, entries, outListing.damage, fullyParsed);

    for (const RawEntry & entry : entries)
    {
        outListing.entries.push_back (ToFileEntry (entry));
        outListing.entries.back().catalogIndex = outListing.entries.size() - 1;
    }

Error:
    return hr;
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

    CollectEntries (ProDosSkeleton::kDirKeyBlock, entries, outListing.damage, fullyParsed);

    for (const RawEntry & entry : entries)
    {
        outListing.entries.push_back (ToFileEntry (entry));
        outListing.entries.back().catalogIndex = outListing.entries.size() - 1;
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
    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_sectors.size();
    int                  dirKeyBlock = ProDosSkeleton::kDirKeyBlock;
    bool                 hasLeaf     = !path.IsEmpty();
    Byte                 fileType    = 0;
    Word                 auxType     = 0;
    bool                 parsed      = true;
    bool                 found       = false;
    uint16_t             local       = 0;
    vector<RawEntry>     entries;
    vector<std::string>  damage;



    CBREx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBREx (hasLeaf, E_INVALIDARG);

    // A file in a subdirectory is found in that directory's own chain.
    hr = ResolveDirectory (GetParentPath (path), dirKeyBlock);
    CHR (hr);

    outPayload = FilePayload();

    hr = ProDosReader::ExtractFileFromDirectory (m_sectors, dirKeyBlock, path.GetLeaf(), outPayload.bytes,
                                                 fileType, auxType, m_readObserver);
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

    CollectEntries (dirKeyBlock, entries, damage, parsed);
    found = TryFindEntry (entries, path.GetLeaf(), local);

    if (found)
    {
        outPayload.access       = entries[local].access;
        outPayload.hasAccess    = true;
        outPayload.createdDate  = ReadWord (entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffCreated);
        outPayload.createdTime  = ReadWord (entries[local].dirBlock, entries[local].entryOffset + ProDosSkeleton::kEntOffCreated + 2);
        outPayload.modifiedDate = entries[local].modDate;
        outPayload.modifiedTime = entries[local].modTime;
        outPayload.hasDates     = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::CollectDirectoryBlocks
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::CollectDirectoryBlocks (
    int                keyBlock,
    vector<uint32_t> & outBlocks,
    ChainWalkGuard   & guard) const
{
    int  block = keyBlock;



    while (block != 0)
    {
        bool  inRange = block > 0 && block < ProDosSkeleton::kTotalBlocks;
        bool  stepOk  = inRange && guard.TryVisit ((uint32_t) block);

        if (!stepOk)
        {
            break;
        }

        outBlocks.push_back ((uint32_t) block);

        block = ReadWord (block, ProDosSkeleton::kOffNextBlock);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::CollectEntriesBelow
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::CollectEntriesBelow (
    int                   keyBlock,
    ChainWalkGuard      & guard,
    vector<RawEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    vector<RawEntry>  here;
    bool              parsed  = true;
    bool              visitOk = false;
    size_t            i       = 0;



    visitOk = keyBlock > 0 && keyBlock < ProDosSkeleton::kTotalBlocks && guard.TryVisit ((uint32_t) keyBlock);

    if (!visitOk)
    {
        outFullyParsed = false;

        return;
    }

    CollectEntries (keyBlock, here, outDamage, parsed);

    outFullyParsed = outFullyParsed && parsed;

    for (i = 0; i < here.size(); i++)
    {
        outEntries.push_back (here[i]);

        if (here[i].storage == ProDosSkeleton::kStorageSubdir)
        {
            CollectEntriesBelow ((int) here[i].keyPointer, guard, outEntries, outDamage, outFullyParsed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::CollectAllEntries
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolume::CollectAllEntries (
    vector<RawEntry>    & outEntries,
    vector<std::string> & outDamage,
    bool                & outFullyParsed) const
{
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);



    outEntries.clear();
    outFullyParsed = true;

    CollectEntriesBelow (ProDosSkeleton::kDirKeyBlock, guard, outEntries, outDamage, outFullyParsed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::TryFindOwnerIndex
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::TryFindOwnerIndex (
    const vector<RawEntry>  & all,
    const RawEntry          & entry,
    uint16_t                & outOwner)
{
    size_t  i = 0;



    for (i = 0; i < all.size(); i++)
    {
        if (all[i].dirBlock == entry.dirBlock && all[i].entryOffset == entry.entryOffset)
        {
            outOwner = (uint16_t) i;

            return true;
        }
    }

    return false;
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

    CollectAllEntries (entries, damage, fullyParsed);
    outReport.SetCatalogFullyParsed (fullyParsed);

    for (block = 0; block < kReservedBlocks; block++)
    {
        outReport.AddClaim (block, VolumeIntegrityReport::kVolumeOwner);
    }

    for (owner = 0; owner < (uint16_t) entries.size(); owner++)
    {
        ChainWalkGuard    guard ((uint32_t) ProDosSkeleton::kTotalBlocks);
        vector<uint32_t>  blocks;
        bool              isDirectory = entries[owner].storage == ProDosSkeleton::kStorageSubdir;
        bool              walked      = true;

        //  A subdirectory owns its whole chain of directory blocks, not only
        //  the key block the record points at. Leaving the rest unclaimed is
        //  what let the allocator hand out a live directory block.
        if (isDirectory)
        {
            CollectDirectoryBlocks ((int) entries[owner].keyPointer, blocks, guard);
        }
        else
        {
            walked = CollectFileBlocks (entries[owner], blocks, guard);
        }

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

    at = ProDosSkeleton::GetBlockByteOffset (ProDosSkeleton::kBitmapBlock, (size_t) (block / 8));

    return at < buffer.size() && (buffer[at] & mask) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::ApplyCaseFlags
//
//  Bit 15 says the word describes the name at all; without it the entry came
//  from ProDOS 8 and the name is what the directory holds. Bit 14 is the first
//  character and the bits run down from there, a set bit meaning lower case.
//
////////////////////////////////////////////////////////////////////////////////

std::string ProDosVolume::ApplyCaseFlags (const std::string & name, Word caseFlags)
{
    std::string  shown (name);
    size_t       i     = 0;
    int          bit   = 0;



    if ((caseFlags & ProDosSkeleton::kCaseFlagsPresent) == 0)
    {
        return shown;
    }

    for (i = 0; i < shown.size(); i++)
    {
        bit = ProDosSkeleton::kCaseFirstCharBit - (int) i;

        if (bit < 0)
        {
            break;
        }

        if ((caseFlags & (Word) (1u << bit)) != 0 && shown[i] >= 'A' && shown[i] <= 'Z')
        {
            shown[i] = (char) (shown[i] - 'A' + 'a');
        }
    }

    return shown;
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
//  ProDosVolume::IsLaunchableSystemFile
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::IsLaunchableSystemFile (const RawEntry & entry)
{
    bool  isSystem = entry.fileType == kTypeSystem;
    bool  isKernel = _stricmp (entry.name.c_str(), kpszKernelName) == 0;



    return isSystem && !isKernel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::TryToUnixTime
//
//  The directory's packed date and time as Unix seconds. A zero date word is
//  the "no date" ProDOS itself writes and a listing shows as <NO DATE>, and
//  fields outside their ranges are treated the same way rather than being
//  read as a date nobody set.
//
//  The day count uses the proleptic Gregorian arithmetic every civil-date
//  routine reduces to, so no library calendar with a time zone of its own is
//  consulted: the stamp is wall-clock time on a machine that had no zone.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosVolume::TryToUnixTime (Word date, Word time, int64_t & outUnix)
{
    constexpr int      kDaysPerEra       = 146097;
    constexpr int      kYearsPerEra      = 400;
    constexpr int64_t  kEpochDayOffset   = 719468;   // days from 0000-03-01 to 1970-01-01
    constexpr int      kMonthsInYear     = 12;
    constexpr int      kDaysInMonth      = 31;
    constexpr int      kHoursInDay       = 24;
    constexpr int      kMinutesInHour    = 60;
    constexpr int      kSecondsInMinute  = 60;
    constexpr int      kSecondsInDay     = 86400;
    constexpr int      kFirstCentury     = 1900;
    constexpr int      kSecondCentury    = 2000;



    int       year   = (date >> 9) & 0x7F;
    int       month  = (date >> 5) & 0x0F;
    int       day    = date & 0x1F;
    int       hour   = (time >> 8) & 0xFF;
    int       minute = time & 0xFF;
    int       era    = 0;
    int       yoe    = 0;
    int       doy    = 0;
    int       doe    = 0;
    int64_t   days   = 0;



    outUnix = 0;

    if (date == 0 || month < 1 || month > kMonthsInYear || day < 1 || day > kDaysInMonth
     || hour >= kHoursInDay || minute >= kMinutesInHour)
    {
        return false;
    }

    year += (year < kCenturyPivotYear) ? kSecondCentury : kFirstCentury;

    // Howard Hinnant's days-from-civil, with March as the first month so a
    // leap day falls at the end of the year it belongs to.
    if (month <= 2)
    {
        year--;
    }

    era  = year / kYearsPerEra;
    yoe  = year - era * kYearsPerEra;
    doy  = (153 * (month + ((month > 2) ? -3 : 9)) + 2) / 5 + day - 1;
    doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = (int64_t) era * kDaysPerEra + doe - kEpochDayOffset;

    outUnix = days * kSecondsInDay + (int64_t) hour * kMinutesInHour * kSecondsInMinute
            + (int64_t) minute * kSecondsInMinute;

    return true;
}
