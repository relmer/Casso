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

            if (typeLen == 0)
            {
                continue;   // an unused slot, not the end of the directory
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
        for (i = 0; ok && i < (int) NibblizationLayer::kSectorByteSize; i++)
        {
            Word  block = (Word) (ReadByte (entry.keyPointer, (size_t) i)
                               | (ReadByte (entry.keyPointer, (size_t) i + 256) << 8));

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
        for (i = 0; ok && i < (int) NibblizationLayer::kSectorByteSize; i++)
        {
            Word  indexBlock = (Word) (ReadByte (entry.keyPointer, (size_t) i)
                                    | (ReadByte (entry.keyPointer, (size_t) i + 256) << 8));
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

            for (j = 0; ok && j < (int) NibblizationLayer::kSectorByteSize; j++)
            {
                Word  block = (Word) (ReadByte (indexBlock, (size_t) j)
                                   | (ReadByte (indexBlock, (size_t) j + 256) << 8));

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
        listed.isDirectory    = entry.storage == 0xD0;
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
    constexpr uint16_t  kVolumeOwner    = 0xFFFE;
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
        outReport.AddClaim (block, kVolumeOwner);
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
//  ProDosVolume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return E_NOTIMPL;
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
