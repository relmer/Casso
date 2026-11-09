#include "Pch.h"

#include "Devices/Disk/Archives/BinaryTwoArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryTwoArchive::IsBinaryTwo
//
//  The three ID bytes, a linefeed, 'G' and 'L', and the ID byte at 18.
//
////////////////////////////////////////////////////////////////////////////////

bool BinaryTwoArchive::IsBinaryTwo (std::span<const Byte> bytes)
{
    return bytes.size() >= kHeaderSize
        && bytes[0] == 0x0A && bytes[1] == 'G' && bytes[2] == 'L'
        && bytes[kOffIdByte] == kIdByte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryTwoArchive::Parse
//
//  Headers until the bytes run out or the next header doesn't start with
//  its ID bytes, which ends the archive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryTwoArchive::Parse (std::span<const Byte> bytes, std::vector<BinaryTwoEntry> & outEntries, std::string & outError)
{
    HRESULT  hr      = S_OK;
    bool     isBny   = IsBinaryTwo (bytes);
    size_t   offset  = 0;



    outEntries.clear();

    CBRFEx (isBny, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The file is not a Binary II archive.");

    while (offset + kHeaderSize <= bytes.size() && IsBinaryTwo (bytes.subspan (offset)))
    {
        std::span<const Byte>  header  = bytes.subspan (offset, kHeaderSize);
        BinaryTwoEntry         entry;
        size_t                 eof     = (size_t) header[kOffEof] | ((size_t) header[kOffEof + 1] << 8) | ((size_t) header[kOffEof + 2] << 16) | ((size_t) header[kOffEofHigh] << 24);
        size_t                 nameLen = std::min ((size_t) header[kOffName], kMaxName);
        size_t                 padded  = (eof + kHeaderSize - 1) / kHeaderSize * kHeaderSize;
        bool                   isDir   = header[kOffFileType] == BinaryTwoEntry::kTypeDirectory;
        bool                   fits    = false;

        entry.access      = header[kOffAccess];
        entry.fileType    = header[kOffFileType];
        entry.auxType     = (Word) (header[kOffAuxType] | (header[kOffAuxType + 1] << 8));
        entry.storageType = header[kOffStorage];
        entry.modDate     = (Word) (header[kOffModDate] | (header[kOffModDate + 1] << 8));
        entry.modTime     = (Word) (header[kOffModTime] | (header[kOffModTime + 1] << 8));
        entry.createDate  = (Word) (header[kOffCreateDate] | (header[kOffCreateDate + 1] << 8));
        entry.createTime  = (Word) (header[kOffCreateTime] | (header[kOffCreateTime + 1] << 8));
        entry.name.assign (header.begin() + kOffName + 1, header.begin() + (ptrdiff_t) (kOffName + 1 + nameLen));

        //  A directory's length is not its data's; some programs copied the
        //  directory's own length, and nothing follows it.
        if (isDir)
        {
            eof    = 0;
            padded = 0;
        }

        offset += kHeaderSize;
        fits    = offset + eof <= bytes.size();
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format ("{} runs past the end of the archive.", entry.name));

        entry.data.assign (bytes.begin() + (ptrdiff_t) offset, bytes.begin() + (ptrdiff_t) (offset + eof));
        offset += std::min (padded, bytes.size() - offset);

        outEntries.push_back (std::move (entry));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryTwoArchive::Build
//
//  Version 1 headers, each with the count of entries after it, and each
//  file's data padded with zeros.
//
////////////////////////////////////////////////////////////////////////////////

void BinaryTwoArchive::Build (const std::vector<BinaryTwoEntry> & entries, std::vector<Byte> & outBytes)
{
    constexpr Byte    kVersion   = 1;
    constexpr size_t  kBlockSize = 512;



    size_t  index = 0;



    outBytes.clear();

    for (index = 0; index < entries.size(); index++)
    {
        const BinaryTwoEntry  & entry   = entries[index];
        std::vector<Byte>       header (kHeaderSize, 0);
        size_t                  eof     = entry.data.size();
        size_t                  blocks  = (eof + kBlockSize - 1) / kBlockSize;
        size_t                  nameLen = std::min (entry.name.size(), kMaxName);
        size_t                  padded  = (eof + kHeaderSize - 1) / kHeaderSize * kHeaderSize;

        header[0]                  = 0x0A;
        header[1]                  = 'G';
        header[2]                  = 'L';
        header[kOffAccess]         = entry.access;
        header[kOffFileType]       = entry.fileType;
        header[kOffAuxType]        = (Byte) (entry.auxType & 0xFF);
        header[kOffAuxType + 1]    = (Byte) (entry.auxType >> 8);
        header[kOffStorage]        = entry.storageType;
        header[kOffBlocks]         = (Byte) (blocks & 0xFF);
        header[kOffBlocks + 1]     = (Byte) (blocks >> 8);
        header[kOffModDate]        = (Byte) (entry.modDate & 0xFF);
        header[kOffModDate + 1]    = (Byte) (entry.modDate >> 8);
        header[kOffModTime]        = (Byte) (entry.modTime & 0xFF);
        header[kOffModTime + 1]    = (Byte) (entry.modTime >> 8);
        header[kOffCreateDate]     = (Byte) (entry.createDate & 0xFF);
        header[kOffCreateDate + 1] = (Byte) (entry.createDate >> 8);
        header[kOffCreateTime]     = (Byte) (entry.createTime & 0xFF);
        header[kOffCreateTime + 1] = (Byte) (entry.createTime >> 8);
        header[kOffIdByte]         = kIdByte;
        header[kOffEof]            = (Byte) (eof & 0xFF);
        header[kOffEof + 1]        = (Byte) ((eof >> 8) & 0xFF);
        header[kOffEof + 2]        = (Byte) ((eof >> 16) & 0xFF);
        header[kOffEofHigh]        = (Byte) ((eof >> 24) & 0xFF);
        header[kOffName]           = (Byte) nameLen;
        header[kOffVersion]        = kVersion;
        header[kOffFollowing]      = (Byte) std::min (entries.size() - index - 1, (size_t) 0xFF);

        std::copy_n (entry.name.begin(), nameLen, header.begin() + kOffName + 1);

        outBytes.insert (outBytes.end(), header.begin(), header.end());
        outBytes.insert (outBytes.end(), entry.data.begin(), entry.data.end());
        outBytes.insert (outBytes.end(), padded - eof, 0);
    }
}
