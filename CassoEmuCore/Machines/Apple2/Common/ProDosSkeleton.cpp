#include "Pch.h"

#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Devices/Disk/ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetBlockByteOffset
//
//  The buffer holds DOS 3.3 logical sectors; a ProDOS block is two 256-byte
//  halves spread across the block's track per the interleave table.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosSkeleton::GetBlockByteOffset (int block, size_t offsetInBlock)
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
//  ProDosSkeleton::IsBlockOrder
//
//  A 143,360-byte buffer is in DOS logical sector order, as every 140K volume
//  is loaded. Any other size is a run of 512-byte blocks in order.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosSkeleton::IsBlockOrder (size_t bufferBytes)
{
    return bufferBytes != (size_t) NibblizationLayer::kImageByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetBlockByteOffset
//
//  The same offset in a buffer of either order, picked by its size.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosSkeleton::GetBlockByteOffset (size_t bufferBytes, int block, size_t offsetInBlock)
{
    if (!IsBlockOrder (bufferBytes))
    {
        return GetBlockByteOffset (block, offsetInBlock);
    }

    return (size_t) block * (size_t) kBlockByteSize + offsetInBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetBlockCount
//
//  The blocks a buffer of this size has, up to the most a volume addresses.
//
////////////////////////////////////////////////////////////////////////////////

int ProDosSkeleton::GetBlockCount (size_t bufferBytes)
{
    size_t  blocks = bufferBytes / (size_t) kBlockByteSize;



    if (!IsBlockOrder (bufferBytes))
    {
        return kTotalBlocks;
    }

    return (int) std::min (blocks, (size_t) kMaxBlocks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::IsValidVolumeSize
//
//  A 140K sector buffer, or whole blocks past the volume directory and its
//  bitmap, up to 32 MB. A 32 MB image has one block more than ProDOS
//  addresses, and the last is left unused.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosSkeleton::IsValidVolumeSize (size_t bufferBytes)
{
    constexpr size_t  kMaxImageBlocks = (size_t) kMaxBlocks + 1;



    size_t  blocks = bufferBytes / (size_t) kBlockByteSize;



    if (!IsBlockOrder (bufferBytes))
    {
        return true;
    }

    return bufferBytes % (size_t) kBlockByteSize == 0
        && blocks > (size_t) kBitmapBlock
        && blocks <= kMaxImageBlocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetBitmapByteOffset
//
//  The bitmap byte that has a block's bit. A 140K volume's bitmap is block 6;
//  a larger volume's starts at the block its header points at and runs over
//  as many blocks as it takes, 4,096 blocks to each.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosSkeleton::GetBitmapByteOffset (const BlockImage & buffer, uint32_t block)
{
    size_t  bufferBytes = buffer.size();
    size_t  byteIndex   = (size_t) (block / 8);
    size_t  pointerAt   = 0;
    int     first       = 0;



    if (!IsBlockOrder (bufferBytes))
    {
        return GetBlockByteOffset (kBitmapBlock, byteIndex);
    }

    pointerAt = GetBlockByteOffset (bufferBytes, kDirKeyBlock, kOffFirstEntry + kHdrOffBitmapPointer);
    first     = buffer[pointerAt] | (buffer[pointerAt + 1] << 8);

    return GetBlockByteOffset (bufferBytes,
                               first + (int) (byteIndex / (size_t) kBlockByteSize),
                               byteIndex % (size_t) kBlockByteSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void ProDosSkeleton::WriteWord (vector<Byte> & buffer, int block, size_t offsetInBlock, Word value)
{
    buffer[GetBlockByteOffset (buffer.size(), block, offsetInBlock)]     = (Byte) (value & 0xFF);
    buffer[GetBlockByteOffset (buffer.size(), block, offsetInBlock + 1)] = (Byte) (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetReservedBlockCount
//
//  The boot blocks, the volume directory and the bitmap's blocks, which a
//  volume's first file follows. One bitmap block covers 4,096 blocks.
//
////////////////////////////////////////////////////////////////////////////////

int ProDosSkeleton::GetReservedBlockCount (size_t bufferBytes)
{
    constexpr int  kBlocksPerBitmapBlock = kBlockByteSize * 8;



    int  blocks = GetBlockCount (bufferBytes);



    return kBitmapBlock + (blocks + kBlocksPerBitmapBlock - 1) / kBlocksPerBitmapBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::WriteBitmap
//
//  The volume bitmap from block 6: the boot blocks, the volume directory and
//  the bitmap's blocks in use, every other block free. One bitmap block covers
//  4,096 blocks, so a 140K volume's bitmap is block 6 alone and starts 0x01,
//  then 0xFF.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosSkeleton::WriteBitmap (vector<Byte> & buffer, int blocks)
{
    constexpr int  kBlocksPerBitmapBlock = kBlockByteSize * 8;



    size_t  bufferBytes = buffer.size();
    int     firstFree   = GetReservedBlockCount (bufferBytes);
    int     block       = 0;
    size_t  at          = 0;



    for (block = firstFree; block < blocks; block++)
    {
        at = GetBlockByteOffset (bufferBytes,
                                 kBitmapBlock + block / kBlocksPerBitmapBlock,
                                 (size_t) ((block % kBlocksPerBitmapBlock) / 8));

        buffer[at] = (Byte) (buffer[at] | (0x80 >> (block % 8)));
    }
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
    int      blocks      = 0;
    bool     nameOk      = false;
    bool     sizeOk      = IsValidVolumeSize (bufferBytes);



    CBRA (sizeOk);

    blocks = GetBlockCount (bufferBytes);

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

    buffer[GetBlockByteOffset (bufferBytes, kDirKeyBlock, header + kHdrOffTypeName)] =
        (Byte) (kStorageVolumeDir | (Byte) nameBytes);

    for (i = 0; i < nameBytes; i++)
    {
        buffer[GetBlockByteOffset (bufferBytes, kDirKeyBlock, header + kHdrOffName + i)] =
            (Byte) toupper ((unsigned char) volumeName[i]);
    }

    buffer[GetBlockByteOffset (bufferBytes, kDirKeyBlock, header + kHdrOffAccess)]          = kAccessDefault;
    buffer[GetBlockByteOffset (bufferBytes, kDirKeyBlock, header + kHdrOffEntryLength)]     = kEntryLength;
    buffer[GetBlockByteOffset (bufferBytes, kDirKeyBlock, header + kHdrOffEntriesPerBlock)] = kEntriesPerBlock;

    WriteWord (buffer, kDirKeyBlock, header + kHdrOffFileCount,     0);
    WriteWord (buffer, kDirKeyBlock, header + kHdrOffBitmapPointer, (Word) kBitmapBlock);
    WriteWord (buffer, kDirKeyBlock, header + kHdrOffTotalBlocks,   (Word) blocks);

    WriteBitmap (buffer, blocks);

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
    bool          sizeValid   = IsValidVolumeSize (bufferBytes);
    int           block       = 0;
    size_t        i           = 0;
    Byte          fileType    = 0;
    Word          auxType     = 0;
    vector<Byte>  fileBytes;



    //  The volume is a 140K disk in DOS order or a larger one in block order,
    //  and the master a 140K disk in DOS order.
    CBRA (sizeValid);
    CBRAEx (usersBytes == (size_t) NibblizationLayer::kImageByteSize, E_INVALIDARG);

    for (block = 0; block < 2; block++)
    {
        for (i = 0; i < (size_t) kBlockByteSize; i++)
        {
            buffer[GetBlockByteOffset (bufferBytes, block, i)] = usersDisk[GetBlockByteOffset (block, i)];
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
//  A file in the volume directory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractFile (
    const BlockImage   & volume,
    const std::string  & fileName,
    vector<Byte>       & outBytes,
    Byte               & outFileType,
    Word               & outAuxType)
{
    return ExtractFileFromDirectory (volume, ProDosSkeleton::kDirKeyBlock, fileName, outBytes, outFileType, outAuxType);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ExtractFileFromDirectory
//
//  Minimal ProDOS volume reader: walks one directory's chain, from its key
//  block, for a
//  case-insensitive name match, then gathers the file's data blocks per its
//  storage type -- seedling (the key block IS the data), sapling (key is an
//  index block of up to 256 data pointers), or tree (key is a master index
//  of index blocks). A zero pointer anywhere means a sparse hole and reads
//  as zeros. Output is truncated to the entry's EOF.
//
//  A record whose storage-type nibble is zero is inactive and is skipped even
//  though its name survives. A deleted file keeps its name on purpose, so
//  matching on the name alone would hand back a file whose blocks have been
//  given to somebody else.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractFileFromDirectory (
    const BlockImage     & volume,
    int                    dirKeyBlock,
    const std::string    & fileName,
    vector<Byte>         & outBytes,
    Byte                 & outFileType,
    Word                 & outAuxType,
    ISectorReadObserver  * observer)
{
    HRESULT      hr = S_OK;
    ProDosForks  forks;



    hr = ExtractForksFromDirectory (volume, dirKeyBlock, fileName, forks, outFileType, outAuxType, observer);
    CHR (hr);

    outBytes = std::move (forks.data);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ExtractForksFromDirectory
//
//  The same search, returning both forks and the Finder information of an
//  extended file, and the data fork alone of any other.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractForksFromDirectory (
    const BlockImage     & volume,
    int                    dirKeyBlock,
    const std::string    & fileName,
    ProDosForks          & outForks,
    Byte                 & outFileType,
    Word                 & outAuxType,
    ISectorReadObserver  * observer)
{
    HRESULT         hr          = S_OK;
    size_t          volumeBytes = volume.size();
    bool            sizeOk      = ProDosSkeleton::IsValidVolumeSize (volumeBytes);
    size_t          nameBytes   = fileName.size();
    int             dirBlock    = 0;
    size_t          entryOffset = 0;
    size_t          entry       = 0;
    Byte            storage     = 0;
    Word            keyPointer  = 0;
    uint32_t        eof         = 0;
    bool            found       = false;
    bool            keyOk       = false;
    int             perBlock    = 0;
    ChainWalkGuard  guard ((uint32_t) ProDosSkeleton::GetBlockCount (volume.size()));



    CBRAEx (sizeOk, E_INVALIDARG);
    CBRAEx (nameBytes >= 1 && nameBytes <= ProDosSkeleton::kVolumeNameBytes, E_INVALIDARG);

    keyOk = dirKeyBlock > 0 && IsBlockInRange (volume, (Word) dirKeyBlock);
    CBRAEx (keyOk, E_INVALIDARG);

    perBlock = ProDosSkeleton::GetEntriesPerBlock (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), dirKeyBlock, ProDosSkeleton::kEntriesPerBlockAt)]);

    // Walk the directory's chain from its key block. The key block's first
    // record is the directory's own header, not a file, in a subdirectory as
    // in the volume directory.
    dirBlock = dirKeyBlock;

    while (!found && dirBlock != 0)
    {
        int   first   = (dirBlock == dirKeyBlock) ? 1 : 0;
        int   n       = 0;
        bool  stepOk  = ProDosReader::IsBlockInRange (volume, (Word) dirBlock)
                     && guard.TryVisit ((uint32_t) dirBlock);

        // A directory chain that loops or leaves the volume stops here. Both
        // are reachable from a damaged disk, and following either reads outside
        // the buffer or never returns.
        if (!stepOk)
        {
            break;
        }

        NoteBlockRead (observer, volume.size(), dirBlock);

        for (n = first; !found && n < perBlock; n++)
        {
            size_t  at      = ProDosSkeleton::kOffFirstEntry
                            + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte    typeLen = volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), dirBlock, at)];
            size_t  len     = (size_t) (typeLen & 0x0F);
            bool    match   = ((typeLen & 0xF0) != ProDosSkeleton::kStorageInactive)
                           && (len == nameBytes);
            size_t  i       = 0;

            for (i = 0; match && i < len; i++)
            {
                Byte  c = volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), 
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
            dirBlock = volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), dirBlock, ProDosSkeleton::kOffNextBlock)]
                     | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), dirBlock, ProDosSkeleton::kOffNextBlock + 1)] << 8);
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    outForks = ProDosForks();

    storage = (Byte) (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset)] & 0xF0);

    keyPointer = (Word)
        (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffKeyPointer)]
       | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffKeyPointer + 1)] << 8));

    eof = (uint32_t)
         (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffEof)]
        | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffEof + 1)] << 8)
        | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffEof + 2)] << 16));

    outFileType = volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffFileType)];

    outAuxType = (Word)
        (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffAuxType)]
       | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), (int) entry, entryOffset + ProDosSkeleton::kEntOffAuxType + 1)] << 8));

    // The key pointer is the root of every storage type's block structure, and
    // is followed before anything else validates it.
    keyOk = IsBlockInRange (volume, keyPointer);
    CBREx (keyOk, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    if (storage == ProDosSkeleton::kStorageExtended)
    {
        hr = ReadExtendedForks (volume, keyPointer, observer, outForks);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReadFork (volume, storage, keyPointer, eof, observer, outForks.data);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ReadFork
//
//  One fork's bytes from its storage type, key block and length. A zero block
//  number in an index is a sparse hole, kept in the list and returned as
//  zeros. A fork whose block structure can't be walked is unreadable rather
//  than short: returning what was gathered would hand back a truncated file
//  that looks complete.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ReadFork (
    const BlockImage     & volume,
    Byte                   storage,
    Word                   keyPointer,
    uint32_t               eof,
    ISectorReadObserver  * observer,
    vector<Byte>         & outBytes)
{
    HRESULT       hr       = S_OK;
    bool          keyOk    = IsBlockInRange (volume, keyPointer);
    bool          gathered = true;
    bool          complete = false;
    vector<Byte>  data;
    vector<Word>  dataBlocks;



    CBREx (keyOk, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    switch (storage)
    {
        case ProDosSkeleton::kStorageSeedling:
            dataBlocks.push_back (keyPointer);
            break;

        case ProDosSkeleton::kStorageSapling:
            gathered = AppendIndexedBlocks (volume, keyPointer, dataBlocks, observer);
            break;

        case ProDosSkeleton::kStorageTree:
        {
            int  i = 0;

            NoteBlockRead (observer, volume.size(), keyPointer);

            for (i = 0; gathered && i < (int) ProDosSkeleton::kPointersPerIndex; i++)
            {
                Word  indexBlock = (Word)
                    (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), keyPointer, (size_t) i)]
                   | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), keyPointer,
                          (size_t) i + ProDosSkeleton::kPointersPerIndex)] << 8));

                if (indexBlock != 0)
                {
                    gathered = AppendIndexedBlocks (volume, indexBlock, dataBlocks, observer);
                }
                else
                {
                    int  hole = 0;

                    for (hole = 0; hole < (int) ProDosSkeleton::kPointersPerIndex; hole++)
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

    CBREx (gathered, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    for (Word block : dataBlocks)
    {
        size_t  i        = 0;
        bool    readable = block != 0 && IsBlockInRange (volume, block);

        if (readable && data.size() < (size_t) eof)
        {
            NoteBlockRead (observer, volume.size(), block);
        }

        for (i = 0; i < (size_t) ProDosSkeleton::kBlockByteSize && data.size() < (size_t) eof; i++)
        {
            data.push_back (readable ? volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), block, i)]
                                     : (Byte) 0);
        }
    }

    //  An end of file past the last block the storage type addresses, as a
    //  seedling or sapling file whose end of file was set to 16 MB, reads as
    //  zeros there, as ProDOS reads it.
    data.resize ((size_t) eof, 0);

    complete = data.size() == (size_t) eof;
    CBREx (complete, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    outBytes = std::move (data);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ReadMiniEntry
//
//  One fork's entry in an extended file's key block: its storage type, as a
//  value from 1 to 3 rather than in the high nibble a directory entry uses,
//  its key block and its length.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosReader::ReadMiniEntry (const BlockImage   & volume, Word keyBlock, size_t at, Byte & outStorage, Word & outKey, uint32_t & outEof)
{
    size_t  bytes = volume.size();



    outStorage = (Byte) ((volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at)] & 0x0F) << 4);
    outKey     = (Word) (volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 1)]
                      | (volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 2)] << 8));
    outEof     = (uint32_t) (volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 5)]
                          | (volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 6)] << 8)
                          | (volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 7)] << 16));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ReadExtendedForks
//
//  An extended file's two forks and its Finder information, from its key
//  block: the data fork's entry at 0, two 18-byte Finder information entries
//  at 8, each a size, a type and 16 bytes, and the resource fork's entry at
//  256.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ReadExtendedForks (
    const BlockImage     & volume,
    Word                   keyBlock,
    ISectorReadObserver  * observer,
    ProDosForks          & outForks)
{
    HRESULT   hr       = S_OK;
    size_t    bytes    = volume.size();
    Byte      storage  = 0;
    Word      key      = 0;
    uint32_t  eof      = 0;
    size_t    entry    = 0;
    size_t    i        = 0;



    NoteBlockRead (observer, bytes, keyBlock);

    outForks.isExtended = true;

    ReadMiniEntry (volume, keyBlock, ProDosSkeleton::kExtOffDataFork, storage, key, eof);

    hr = ReadFork (volume, storage, key, eof, observer, outForks.data);
    CHR (hr);

    ReadMiniEntry (volume, keyBlock, ProDosSkeleton::kExtOffResourceFork, storage, key, eof);

    hr = ReadFork (volume, storage, key, eof, observer, outForks.resource);
    CHR (hr);

    for (entry = 0; entry < ProDosSkeleton::kFinderInfoEntries; entry++)
    {
        size_t  at   = ProDosSkeleton::kExtOffFinderInfo + entry * ProDosSkeleton::kFinderInfoEntryBytes;
        Byte    size = volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at)];

        if (size != (Byte) ProDosSkeleton::kFinderInfoEntryBytes)
        {
            continue;
        }

        outForks.finderInfo.resize (ProDosSkeleton::kFinderInfoBytes, 0);

        for (i = 0; i < ProDosSkeleton::kFinderInfoBytes / ProDosSkeleton::kFinderInfoEntries; i++)
        {
            outForks.finderInfo[entry * (ProDosSkeleton::kFinderInfoBytes / ProDosSkeleton::kFinderInfoEntries) + i] =
                volume[ProDosSkeleton::GetBlockByteOffset (bytes, keyBlock, at + 2 + i)];
        }
    }

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
    const BlockImage     & volume,
    Word                   indexBlock,
    vector<Word>         & dataBlocks,
    ISectorReadObserver  * observer)
{
    int   i     = 0;
    bool  valid = ProDosReader::IsBlockInRange (volume, indexBlock);



    if (!valid)
    {
        return false;
    }

    NoteBlockRead (observer, volume.size(), indexBlock);

    for (i = 0; i < (int) ProDosSkeleton::kPointersPerIndex; i++)
    {
        Word  block = (Word)
            (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), indexBlock, (size_t) i)]
           | (volume[ProDosSkeleton::GetBlockByteOffset (volume.size(), indexBlock,
                  (size_t) i + ProDosSkeleton::kPointersPerIndex)] << 8));

        // Zero is a sparse hole and legitimate; anything past the volume is
        // corruption, and following it would read outside the buffer entirely.
        if (block != 0 && !ProDosReader::IsBlockInRange (volume, block))
        {
            return false;
        }

        dataBlocks.push_back (block);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::NoteBlockRead
//
////////////////////////////////////////////////////////////////////////////////

void ProDosReader::NoteBlockRead (ISectorReadObserver * observer, size_t bufferBytes, int block)
{
    if (observer != nullptr)
    {
        observer->OnSectorRead (ProDosSkeleton::GetBlockByteOffset (bufferBytes, block, 0) / NibblizationLayer::kSectorByteSize);
        observer->OnSectorRead (ProDosSkeleton::GetBlockByteOffset (bufferBytes, block, NibblizationLayer::kSectorByteSize) / NibblizationLayer::kSectorByteSize);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::IsBlockInRange
//
//  Block numbers index straight into the sector buffer through
//  GetBlockByteOffset, which multiplies out to a byte offset without checking
//  anything. A block past the volume therefore does not return wrong data --
//  it reads past the end of the buffer. Every pointer followed here came off
//  a disk that may be damaged or hostile, so none is trusted.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosReader::IsBlockInRange (const BlockImage   & volume, Word block)
{
    return (int) block < ProDosSkeleton::GetBlockCount (volume.size());
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
    HRESULT     hr     = S_OK;
    BlockImage  image (buffer);
    int         blocks = ProDosSkeleton::GetBlockCount (buffer.size());
    int         block  = 0;
    bool        found  = false;



    //  The bitmap of a 140K volume or of a larger one, wherever its volume
    //  header has it.
    for (block = 0; !found && block < blocks; block++)
    {
        size_t  at   = ProDosSkeleton::GetBitmapByteOffset (image, (uint32_t) block);
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
    bool          sizeValid   = ProDosSkeleton::IsValidVolumeSize (bufferBytes);
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
    int           perBlock    = 0;
    vector<Word>  dataBlocks;



    CBRAEx (sizeValid, E_INVALIDARG);
    CBRAEx (nameBytes >= 1 && nameBytes <= ProDosSkeleton::kVolumeNameBytes, E_INVALIDARG);
    CBRAEx (dataBytes > 0, E_INVALIDARG);
    CBRAEx (blockCount <= 256, E_INVALIDARG);

    seedling = (blockCount == 1);
    perBlock = ProDosSkeleton::GetEntriesPerBlock (buffer[ProDosSkeleton::GetBlockByteOffset (bufferBytes, ProDosSkeleton::kDirKeyBlock, ProDosSkeleton::kEntriesPerBlockAt)]);

    // A free directory entry: the first record in the chain whose storage-type
    // nibble is zero, meaning never used or deleted.
    for (dirBlock = ProDosSkeleton::kDirKeyBlock;
         !haveSlot && dirBlock <= ProDosSkeleton::kDirLastBlock;
         dirBlock++)
    {
        int  first = (dirBlock == ProDosSkeleton::kDirKeyBlock) ? 1 : 0;
        int  n     = 0;

        for (n = first; !haveSlot && n < perBlock; n++)
        {
            size_t  at      = ProDosSkeleton::kOffFirstEntry
                            + (size_t) n * ProDosSkeleton::kEntryLength;
            Byte    typeLen = buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, at)];

            if ((typeLen & 0xF0) == ProDosSkeleton::kStorageInactive)
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
            buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), keyBlock, i)]       = (Byte) (dataBlocks[i] & 0xFF);
            buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), keyBlock, i + 256)] = (Byte) (dataBlocks[i] >> 8);
        }
    }

    for (i = 0; i < dataBytes; i++)
    {
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dataBlocks[i / 512], i % 512)] = bytes[i];
    }

    // The directory entry.
    {
        Byte    storage = seedling ? ProDosSkeleton::kStorageSeedling
                                   : ProDosSkeleton::kStorageSapling;
        size_t  base    = entryAt;

        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base)] =
            (Byte) (storage | (Byte) nameBytes);

        for (i = 0; i < nameBytes; i++)
        {
            buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffName + i)] =
                (Byte) toupper ((unsigned char) fileName[i]);
        }

        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffFileType)]       = fileType;
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffKeyPointer)]     = (Byte) (keyBlock & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffKeyPointer + 1)] = (Byte) (keyBlock >> 8);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffBlocksUsed)]     = (Byte) (blocksUsed & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffBlocksUsed + 1)] = (Byte) (blocksUsed >> 8);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffEof)]            = (Byte) (dataBytes & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffEof + 1)]        = (Byte) ((dataBytes >> 8) & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffEof + 2)]        = (Byte) ((dataBytes >> 16) & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffAccess)]         = ProDosSkeleton::kAccessDefault;
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffAuxType)]        = (Byte) (auxType & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffAuxType + 1)]    = (Byte) (auxType >> 8);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), dirBlock, base + ProDosSkeleton::kEntOffHeaderPointer)]  = (Byte) ProDosSkeleton::kDirKeyBlock;
    }

    // The volume header counts one more file.
    {
        size_t  countAt = ProDosSkeleton::kOffFirstEntry + ProDosSkeleton::kHdrOffFileCount;
        Word    count   = (Word)
            (buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), ProDosSkeleton::kDirKeyBlock, countAt)]
           | (buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), ProDosSkeleton::kDirKeyBlock, countAt + 1)] << 8));

        count++;

        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), ProDosSkeleton::kDirKeyBlock, countAt)]     = (Byte) (count & 0xFF);
        buffer[ProDosSkeleton::GetBlockByteOffset (buffer.size(), ProDosSkeleton::kDirKeyBlock, countAt + 1)] = (Byte) (count >> 8);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::IsEntriesPerBlockValid
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosSkeleton::IsEntriesPerBlockValid (Byte headerValue)
{
    return headerValue >= 1 && headerValue <= kEntriesPerBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::GetEntriesPerBlock
//
////////////////////////////////////////////////////////////////////////////////

int ProDosSkeleton::GetEntriesPerBlock (Byte headerValue)
{
    return IsEntriesPerBlockValid (headerValue) ? headerValue : kEntriesPerBlock;
}