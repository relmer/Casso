#include "Pch.h"

#include "ProDosSkeleton.h"
#include "ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::BlockByteOffset
//
//  The buffer holds DOS 3.3 logical sectors; a ProDOS block is two 256-byte
//  halves spread across the block's track per the interleave table.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosSkeleton::BlockByteOffset (int block, size_t offsetInBlock)
{
    int     track  = block / kBlocksPerTrack;
    int     sub    = block % kBlocksPerTrack;
    int     half   = (offsetInBlock >= (size_t) NibblizationLayer::kSectorByteSize) ? 1 : 0;
    int     sector = kBlockHalfSector[sub][half];



    return ((size_t) track * NibblizationLayer::kSectorsPerTrack + (size_t) sector)
         * (size_t) NibblizationLayer::kSectorByteSize
         + (offsetInBlock % (size_t) NibblizationLayer::kSectorByteSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void ProDosSkeleton::WriteWord (vector<Byte> & buffer, int block, size_t offsetInBlock, Word value)
{
    buffer[BlockByteOffset (block, offsetInBlock)]     = (Byte) (value & 0xFF);
    buffer[BlockByteOffset (block, offsetInBlock + 1)] = (Byte) (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::Write
//
//  Canonical minimal 280-block volume: boot blocks 0-1 zeroed, volume
//  directory key block 2 chained through 5, volume bitmap at block 6 with
//  blocks 0-6 marked used and everything else free. ProDOS mounts and CATs
//  it clean regardless of boot-block content.
//
//  Bitmap encoding: one bit per block, MSB of byte 0 is block 0, set = free.
//  280 blocks fill exactly 35 bytes: 0x01 (blocks 0-6 used, 7 free) then
//  34 x 0xFF.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosSkeleton::Write (vector<Byte> & buffer, const std::string & volumeName)
{
    HRESULT  hr          = S_OK;
    size_t   bufferBytes = buffer.size();
    size_t   nameBytes   = volumeName.size();
    size_t   i           = 0;
    size_t   header      = 0;
    int      block       = 0;
    bool     nameOk      = false;



    CBRA (bufferBytes == (size_t) NibblizationLayer::kImageByteSize);

    // Volume-name rules are the builder's user-facing contract; by the time
    // Write runs, a bad name is a programming error.
    nameOk = (nameBytes >= 1 && nameBytes <= kVolumeNameBytes) &&
             isalpha ((unsigned char) volumeName[0]);

    for (i = 1; nameOk && i < nameBytes; i++)
    {
        nameOk = isalnum ((unsigned char) volumeName[i]) || volumeName[i] == '.';
    }

    CBRAEx (nameOk, E_INVALIDARG);

    std::fill (buffer.begin(), buffer.end(), (Byte) 0);

    // Directory chain: key block 2 -> 3 -> 4 -> 5, back-links mirrored,
    // ends-of-chain zero.
    for (block = kDirKeyBlock; block <= kDirLastBlock; block++)
    {
        WriteWord (buffer, block, kOffPrevBlock,
                   (Word) ((block > kDirKeyBlock) ? block - 1 : 0));
        WriteWord (buffer, block, kOffNextBlock,
                   (Word) ((block < kDirLastBlock) ? block + 1 : 0));
    }

    // Volume-directory header (first entry of the key block).
    header = kOffFirstEntry;

    buffer[BlockByteOffset (kDirKeyBlock, header + kHdrOffTypeName)] =
        (Byte) (kStorageVolumeDir | (Byte) nameBytes);

    for (i = 0; i < nameBytes; i++)
    {
        buffer[BlockByteOffset (kDirKeyBlock, header + kHdrOffName + i)] =
            (Byte) toupper ((unsigned char) volumeName[i]);
    }

    buffer[BlockByteOffset (kDirKeyBlock, header + kHdrOffAccess)]          = kAccessDefault;
    buffer[BlockByteOffset (kDirKeyBlock, header + kHdrOffEntryLength)]     = kEntryLength;
    buffer[BlockByteOffset (kDirKeyBlock, header + kHdrOffEntriesPerBlock)] = kEntriesPerBlock;

    WriteWord (buffer, kDirKeyBlock, header + kHdrOffFileCount,     0);
    WriteWord (buffer, kDirKeyBlock, header + kHdrOffBitmapPointer, (Word) kBitmapBlock);
    WriteWord (buffer, kDirKeyBlock, header + kHdrOffTotalBlocks,   (Word) kTotalBlocks);

    // Volume bitmap: blocks 0-6 in use, 7-279 free.
    buffer[BlockByteOffset (kBitmapBlock, 0)] = 0x01;

    for (i = 1; i < (size_t) (kTotalBlocks / 8); i++)
    {
        buffer[BlockByteOffset (kBitmapBlock, i)] = 0xFF;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::InstallBoot
//
//  Makes the volume boot the way a real ProDOS disk does: boot blocks 0-1
//  copied verbatim from the Users Disk, then PRODOS and BASIC.SYSTEM
//  extracted from it and written as real files (the boot code loads PRODOS
//  by name, which hands off to BASIC.SYSTEM). Blocks 0-1 are already
//  marked used by the skeleton, so the bitmap stays honest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosSkeleton::InstallBoot (vector<Byte> & buffer, const vector<Byte> & usersDisk)
{
    HRESULT       hr          = S_OK;
    size_t        bufferBytes = buffer.size();
    size_t        usersBytes  = usersDisk.size();
    int           block       = 0;
    size_t        i           = 0;
    Byte          fileType    = 0;
    Word          auxType     = 0;
    vector<Byte>  fileBytes;



    CBRA (bufferBytes == (size_t) NibblizationLayer::kImageByteSize);
    CBRAEx (usersBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    for (block = 0; block < 2; block++)
    {
        for (i = 0; i < (size_t) kBlockByteSize; i++)
        {
            buffer[BlockByteOffset (block, i)] = usersDisk[BlockByteOffset (block, i)];
        }
    }

    hr = ProDosReader::ExtractFile (usersDisk, "PRODOS", fileBytes, fileType, auxType);
    CHR (hr);

    hr = ProDosFileWriter::WriteFile (buffer, "PRODOS", fileType, auxType, fileBytes);
    CHR (hr);

    hr = ProDosReader::ExtractFile (usersDisk, "BASIC.SYSTEM", fileBytes, fileType, auxType);
    CHR (hr);

    hr = ProDosFileWriter::WriteFile (buffer, "BASIC.SYSTEM", fileType, auxType, fileBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ExtractFile
//
//  Minimal ProDOS volume reader: walks the volume directory chain for a
//  case-insensitive name match, then gathers the file's data blocks per its
//  storage type -- seedling (the key block IS the data), sapling (key is an
//  index block of up to 256 data pointers), or tree (key is a master index
//  of index blocks). A zero pointer anywhere means a sparse hole and reads
//  as zeros. Output is truncated to the entry's EOF.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractFile (
    const vector<Byte> & volume,
    const std::string  & fileName,
    vector<Byte>       & outBytes,
    Byte               & outFileType,
    Word               & outAuxType)
{
    HRESULT         hr          = S_OK;
    size_t          volumeBytes = volume.size();
    size_t          nameBytes   = fileName.size();
    int             dirBlock    = 0;
    size_t          entryOffset = 0;
    size_t          entry       = 0;
    Byte            storage     = 0;
    Word            keyPointer  = 0;
    uint32_t        eof         = 0;
    bool            found       = false;
    bool            keyOk       = false;
    bool            gathered    = true;
    vector<Byte>    data;
    vector<Word>    dataBlocks;
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::kTotalBlocks);



    CBRAEx (volumeBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBRAEx (nameBytes >= 1 && nameBytes <= ProDosSkeleton::kVolumeNameBytes, E_INVALIDARG);

    // Walk the volume directory chain from the key block.
    dirBlock = ProDosSkeleton::kDirKeyBlock;

    while (!found && dirBlock != 0)
    {
        int   first   = (dirBlock == ProDosSkeleton::kDirKeyBlock) ? 1 : 0;
        int   n       = 0;
        bool  stepOk  = ProDosReader::IsBlockInRange ((Word) dirBlock)
                     && guard.TryVisit ((uint32_t) dirBlock);

        // A directory chain that loops or leaves the volume stops here. Both
        // are reachable from a damaged disk, and following either reads outside
        // the buffer or never returns.
        if (!stepOk)
        {
            break;
        }

        for (n = first; !found && n < (int) ProDosSkeleton::kEntriesPerBlock; n++)
        {
            size_t  at      = ProDosSkeleton::kOffFirstEntry
                            + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte    typeLen = volume[ProDosSkeleton::BlockByteOffset (dirBlock, at)];
            size_t  len     = (size_t) (typeLen & 0x0F);
            bool    match   = (typeLen != 0) && (len == nameBytes);
            size_t  i       = 0;

            for (i = 0; match && i < len; i++)
            {
                Byte  c = volume[ProDosSkeleton::BlockByteOffset (
                              dirBlock, at + ProDosSkeleton::kEntOffName + i)];

                match = toupper ((unsigned char) c)
                     == toupper ((unsigned char) fileName[i]);
            }

            if (match)
            {
                found       = true;
                entryOffset = at;
                entry       = (size_t) dirBlock;
            }
        }

        if (!found)
        {
            dirBlock = volume[ProDosSkeleton::BlockByteOffset (dirBlock, ProDosSkeleton::kOffNextBlock)]
                     | (volume[ProDosSkeleton::BlockByteOffset (dirBlock, ProDosSkeleton::kOffNextBlock + 1)] << 8);
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    storage = (Byte) (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset)] & 0xF0);

    keyPointer = (Word)
        (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffKeyPointer)]
       | (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffKeyPointer + 1)] << 8));

    eof = (uint32_t)
         (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffEof)]
        | (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffEof + 1)] << 8)
        | (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffEof + 2)] << 16));

    outFileType = volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffFileType)];

    outAuxType = (Word)
        (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffAuxType)]
       | (volume[ProDosSkeleton::BlockByteOffset ((int) entry, entryOffset + ProDosSkeleton::kEntOffAuxType + 1)] << 8));

    // Resolve the data-block list per storage type. A zero block number in
    // an index is a sparse hole, kept in the list and emitted as zeros.
    // The key pointer is the root of every storage type's block structure, and
    // is followed before anything else validates it.
    keyOk = IsBlockInRange (keyPointer);
    CBREx (keyOk, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    switch (storage)
    {
        case ProDosSkeleton::kStorageSeedling:
            dataBlocks.push_back (keyPointer);
            break;

        case ProDosSkeleton::kStorageSapling:
            gathered = AppendIndexedBlocks (volume, keyPointer, dataBlocks);
            break;

        case ProDosSkeleton::kStorageTree:
        {
            int  i = 0;

            for (i = 0; gathered && i < (int) NibblizationLayer::kSectorByteSize; i++)
            {
                Word  indexBlock = (Word)
                    (volume[ProDosSkeleton::BlockByteOffset (keyPointer, (size_t) i)]
                   | (volume[ProDosSkeleton::BlockByteOffset (keyPointer, (size_t) i + 256)] << 8));

                if (indexBlock != 0)
                {
                    gathered = AppendIndexedBlocks (volume, indexBlock, dataBlocks);
                }
                else
                {
                    int  hole = 0;

                    for (hole = 0; hole < 256; hole++)
                    {
                        dataBlocks.push_back (0);
                    }
                }
            }

            break;
        }

        default:
            CBREx (false, E_UNEXPECTED);
            break;
    }

    // A file whose block structure could not be walked is unreadable, not
    // short. Returning what was gathered would hand back a truncated file that
    // looks complete.
    CBREx (gathered, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    for (Word block : dataBlocks)
    {
        size_t  i        = 0;
        bool    readable = block != 0 && IsBlockInRange (block);

        for (i = 0; i < 512 && data.size() < (size_t) eof; i++)
        {
            data.push_back (readable ? volume[ProDosSkeleton::BlockByteOffset (block, i)]
                                     : (Byte) 0);
        }
    }

    {
        bool  complete = data.size() == (size_t) eof;

        CBREx (complete, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));
    }

    outBytes = std::move (data);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::AppendIndexedBlocks
//
//  Appends every data-block number in one index block, zero holes included.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosReader::AppendIndexedBlocks (
    const vector<Byte> & volume,
    Word                 indexBlock,
    vector<Word>       & dataBlocks)
{
    int   i     = 0;
    bool  valid = ProDosReader::IsBlockInRange (indexBlock);



    if (!valid)
    {
        return false;
    }

    for (i = 0; i < (int) NibblizationLayer::kSectorByteSize; i++)
    {
        Word  block = (Word)
            (volume[ProDosSkeleton::BlockByteOffset (indexBlock, (size_t) i)]
           | (volume[ProDosSkeleton::BlockByteOffset (indexBlock, (size_t) i + 256)] << 8));

        // Zero is a sparse hole and legitimate; anything past the volume is
        // corruption, and following it would read outside the buffer entirely.
        if (block != 0 && !ProDosReader::IsBlockInRange (block))
        {
            return false;
        }

        dataBlocks.push_back (block);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::IsBlockInRange
//
//  Block numbers index straight into the sector buffer through
//  BlockByteOffset, which multiplies out to a byte offset without checking
//  anything. A block past the volume therefore does not return wrong data --
//  it reads past the end of the buffer. Every pointer followed here came off
//  a disk that may be damaged or hostile, so none is trusted.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosReader::IsBlockInRange (Word block)
{
    return block < (Word) ProDosSkeleton::kTotalBlocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter::AllocateBlock
//
//  First-fit scan of the volume bitmap: returns the lowest free block and
//  marks it used. Set bit = free, MSB of byte 0 = block 0.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosFileWriter::AllocateBlock (vector<Byte> & buffer, Word & outBlock)
{
    HRESULT  hr    = S_OK;
    int      block = 0;
    bool     found = false;



    for (block = 0; !found && block < ProDosSkeleton::kTotalBlocks; block++)
    {
        size_t  at   = ProDosSkeleton::BlockByteOffset (
                           ProDosSkeleton::kBitmapBlock, (size_t) (block / 8));
        Byte    mask = (Byte) (0x80 >> (block % 8));

        if ((buffer[at] & mask) != 0)
        {
            buffer[at] = (Byte) (buffer[at] & ~mask);
            outBlock   = (Word) block;
            found      = true;
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter::WriteFile
//
//  Writes one real file into a ProDosSkeleton-formatted buffer: allocates
//  data blocks (plus an index block past the seedling size) from the volume
//  bitmap, lays the bytes down, fills a free directory entry, and bumps the
//  volume header's file count. Seedling (<= 512 bytes) and sapling
//  (<= 256 blocks) layouts cover every file this feature installs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosFileWriter::WriteFile (
    vector<Byte>       & buffer,
    const std::string  & fileName,
    Byte                 fileType,
    Word                 auxType,
    const vector<Byte> & bytes)
{
    HRESULT       hr          = S_OK;
    size_t        bufferBytes = buffer.size();
    size_t        nameBytes   = fileName.size();
    size_t        dataBytes   = bytes.size();
    size_t        blockCount  = (dataBytes + 511) / 512;
    int           dirBlock    = 0;
    size_t        entryAt     = 0;
    bool          haveSlot    = false;
    bool          seedling    = false;
    Word          keyBlock    = 0;
    Word          blocksUsed  = 0;
    size_t        i           = 0;
    vector<Word>  dataBlocks;



    CBRAEx (bufferBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);
    CBRAEx (nameBytes >= 1 && nameBytes <= ProDosSkeleton::kVolumeNameBytes, E_INVALIDARG);
    CBRAEx (dataBytes > 0, E_INVALIDARG);
    CBRAEx (blockCount <= 256, E_INVALIDARG);

    seedling = (blockCount == 1);

    // A free directory entry: the first zero type/name byte in the chain.
    for (dirBlock = ProDosSkeleton::kDirKeyBlock;
         !haveSlot && dirBlock <= ProDosSkeleton::kDirLastBlock;
         dirBlock++)
    {
        int  first = (dirBlock == ProDosSkeleton::kDirKeyBlock) ? 1 : 0;
        int  n     = 0;

        for (n = first; !haveSlot && n < (int) ProDosSkeleton::kEntriesPerBlock; n++)
        {
            size_t  at = ProDosSkeleton::kOffFirstEntry
                       + (size_t) n * ProDosSkeleton::kEntryLength;

            if (buffer[ProDosSkeleton::BlockByteOffset (dirBlock, at)] == 0)
            {
                haveSlot = true;
                entryAt  = at;
            }
        }
    }

    dirBlock--;   // the loop over-advances past the block that matched

    CBREx (haveSlot, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    // Allocate the data blocks (and the index block first for a sapling).
    if (!seedling)
    {
        hr = AllocateBlock (buffer, keyBlock);
        CHR (hr);

        blocksUsed++;
    }

    for (i = 0; i < blockCount; i++)
    {
        Word  block = 0;

        hr = AllocateBlock (buffer, block);
        CHR (hr);

        dataBlocks.push_back (block);
        blocksUsed++;
    }

    if (seedling)
    {
        keyBlock = dataBlocks[0];
    }
    else
    {
        for (i = 0; i < dataBlocks.size(); i++)
        {
            buffer[ProDosSkeleton::BlockByteOffset (keyBlock, i)]       = (Byte) (dataBlocks[i] & 0xFF);
            buffer[ProDosSkeleton::BlockByteOffset (keyBlock, i + 256)] = (Byte) (dataBlocks[i] >> 8);
        }
    }

    for (i = 0; i < dataBytes; i++)
    {
        buffer[ProDosSkeleton::BlockByteOffset (dataBlocks[i / 512], i % 512)] = bytes[i];
    }

    // The directory entry.
    {
        Byte    storage = seedling ? ProDosSkeleton::kStorageSeedling
                                   : ProDosSkeleton::kStorageSapling;
        size_t  base    = entryAt;

        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base)] =
            (Byte) (storage | (Byte) nameBytes);

        for (i = 0; i < nameBytes; i++)
        {
            buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffName + i)] =
                (Byte) toupper ((unsigned char) fileName[i]);
        }

        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffFileType)]       = fileType;
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffKeyPointer)]     = (Byte) (keyBlock & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffKeyPointer + 1)] = (Byte) (keyBlock >> 8);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffBlocksUsed)]     = (Byte) (blocksUsed & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffBlocksUsed + 1)] = (Byte) (blocksUsed >> 8);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffEof)]            = (Byte) (dataBytes & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffEof + 1)]        = (Byte) ((dataBytes >> 8) & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffEof + 2)]        = (Byte) ((dataBytes >> 16) & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffAccess)]         = ProDosSkeleton::kAccessDefault;
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffAuxType)]        = (Byte) (auxType & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffAuxType + 1)]    = (Byte) (auxType >> 8);
        buffer[ProDosSkeleton::BlockByteOffset (dirBlock, base + ProDosSkeleton::kEntOffHeaderPointer)]  = (Byte) ProDosSkeleton::kDirKeyBlock;
    }

    // The volume header counts one more file.
    {
        size_t  countAt = ProDosSkeleton::kOffFirstEntry + ProDosSkeleton::kHdrOffFileCount;
        Word    count   = (Word)
            (buffer[ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt)]
           | (buffer[ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt + 1)] << 8));

        count++;

        buffer[ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt)]     = (Byte) (count & 0xFF);
        buffer[ProDosSkeleton::BlockByteOffset (ProDosSkeleton::kDirKeyBlock, countAt + 1)] = (Byte) (count >> 8);
    }

Error:
    return hr;
}
