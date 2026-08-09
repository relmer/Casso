#include "Pch.h"

#include "ProDosSkeleton.h"





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
//  Boot-payload install; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosSkeleton::InstallBoot (vector<Byte> & buffer, const vector<Byte> & usersDisk)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (usersDisk);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ExtractFile
//
//  Master-image file extraction; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractFile (
    const vector<Byte> & volume,
    const std::string  & fileName,
    vector<Byte>       & outBytes,
    Byte               & outFileType,
    Word               & outAuxType)
{
    UNREFERENCED_PARAMETER (volume);
    UNREFERENCED_PARAMETER (fileName);
    UNREFERENCED_PARAMETER (outBytes);
    UNREFERENCED_PARAMETER (outFileType);
    UNREFERENCED_PARAMETER (outAuxType);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter::WriteFile
//
//  Volume file writer; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosFileWriter::WriteFile (
    vector<Byte>       & buffer,
    const std::string  & fileName,
    Byte                 fileType,
    Word                 auxType,
    const vector<Byte> & bytes)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (fileName);
    UNREFERENCED_PARAMETER (fileType);
    UNREFERENCED_PARAMETER (auxType);
    UNREFERENCED_PARAMETER (bytes);

    return E_NOTIMPL;
}
