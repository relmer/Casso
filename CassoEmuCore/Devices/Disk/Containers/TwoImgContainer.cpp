#include "Pch.h"

#include "Devices/Disk/Containers/TwoImgContainer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::IsTwoImg
//
////////////////////////////////////////////////////////////////////////////////

bool TwoImgContainer::IsTwoImg (std::span<const Byte> bytes)
{
    return bytes.size() >= kHeaderSize
        && bytes[0] == '2' && bytes[1] == 'I' && bytes[2] == 'M' && bytes[3] == 'G';
}





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::Parse
//
//  The payload, comment and creator data must each lie inside the file. A
//  header shorter than 64 bytes, or a format other than the three the
//  container defines, is refused.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TwoImgContainer::Parse (
    std::span<const Byte>   bytes,
    TwoImgHeader          & outHeader,
    std::vector<Byte>     & outPayload,
    std::string           & outError)
{
    HRESULT   hr            = S_OK;
    bool      isTwoImg      = IsTwoImg (bytes);
    uint32_t  headerSize    = 0;
    uint32_t  dataOffset    = 0;
    uint32_t  dataLength    = 0;
    uint32_t  commentOffset = 0;
    uint32_t  commentLength = 0;
    uint32_t  creatorOffset = 0;
    uint32_t  creatorLength = 0;
    bool      sizeOk        = false;
    bool      formatOk      = false;
    bool      dataInside    = false;
    bool      commentInside = false;
    bool      creatorInside = false;



    outHeader = TwoImgHeader();
    outPayload.clear();

    CBRFEx (isTwoImg, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The file is not a 2IMG image.");

    headerSize = ReadLittle32 (bytes, kOffHeaderSize) & 0xFFFF;
    sizeOk     = headerSize >= kHeaderSize;
    CBRFEx (sizeOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The 2IMG header is shorter than 64 bytes.");

    std::copy_n (bytes.begin() + kOffCreator, outHeader.creator.size(), outHeader.creator.begin());

    outHeader.format     = ReadLittle32 (bytes, kOffFormat);
    outHeader.flags      = ReadLittle32 (bytes, kOffFlags);
    outHeader.blockCount = ReadLittle32 (bytes, kOffBlocks);

    formatOk = outHeader.format <= TwoImgHeader::kFormatNibbles;
    CBRFEx (formatOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format ("2IMG format {} is not one the container defines.", outHeader.format));

    dataOffset    = ReadLittle32 (bytes, kOffData);
    dataLength    = ReadLittle32 (bytes, kOffDataLength);
    commentOffset = ReadLittle32 (bytes, kOffComment);
    commentLength = ReadLittle32 (bytes, kOffCommentLen);
    creatorOffset = ReadLittle32 (bytes, kOffCreator2);
    creatorLength = ReadLittle32 (bytes, kOffCreatorLen);

    dataInside    = IsInside (bytes, dataOffset, dataLength);
    commentInside = commentLength == 0 || IsInside (bytes, commentOffset, commentLength);
    creatorInside = creatorLength == 0 || IsInside (bytes, creatorOffset, creatorLength);

    CBRFEx (dataInside,    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The 2IMG image's data runs past the end of the file.");
    CBRFEx (commentInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The 2IMG comment runs past the end of the file.");
    CBRFEx (creatorInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The 2IMG creator data runs past the end of the file.");

    outPayload.assign (bytes.begin() + dataOffset, bytes.begin() + dataOffset + dataLength);
    outHeader.comment.assign (bytes.begin() + commentOffset, bytes.begin() + commentOffset + commentLength);
    outHeader.creatorData.assign (bytes.begin() + creatorOffset, bytes.begin() + creatorOffset + creatorLength);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::Build
//
//  The header, then the payload, the comment and the creator data in that
//  order, each offset written to match. The block count is the payload's for
//  ProDOS order and zero otherwise, as the container defines it.
//
////////////////////////////////////////////////////////////////////////////////

void TwoImgContainer::Build (const TwoImgHeader & header, std::span<const Byte> payload, std::vector<Byte> & outBytes)
{
    constexpr size_t  kBlockBytes = 512;



    uint32_t  dataOffset    = (uint32_t) kHeaderSize;
    uint32_t  dataLength    = (uint32_t) payload.size();
    uint32_t  commentOffset = dataOffset + dataLength;
    uint32_t  commentLength = (uint32_t) header.comment.size();
    uint32_t  creatorOffset = commentOffset + commentLength;
    uint32_t  creatorLength = (uint32_t) header.creatorData.size();
    uint32_t  blocks        = header.format == TwoImgHeader::kFormatProDosOrder ? (uint32_t) (payload.size() / kBlockBytes) : 0;



    outBytes.assign (kHeaderSize, 0);

    outBytes[0] = '2';
    outBytes[1] = 'I';
    outBytes[2] = 'M';
    outBytes[3] = 'G';

    std::copy (header.creator.begin(), header.creator.end(), outBytes.begin() + kOffCreator);

    outBytes[kOffHeaderSize]     = (Byte) kHeaderSize;
    outBytes[kOffVersion]        = (Byte) kVersion;

    WriteLittle32 (outBytes, kOffFormat,     header.format);
    WriteLittle32 (outBytes, kOffFlags,      header.flags);
    WriteLittle32 (outBytes, kOffBlocks,     blocks);
    WriteLittle32 (outBytes, kOffData,       dataOffset);
    WriteLittle32 (outBytes, kOffDataLength, dataLength);
    WriteLittle32 (outBytes, kOffComment,    commentLength == 0 ? 0 : commentOffset);
    WriteLittle32 (outBytes, kOffCommentLen, commentLength);
    WriteLittle32 (outBytes, kOffCreator2,   creatorLength == 0 ? 0 : creatorOffset);
    WriteLittle32 (outBytes, kOffCreatorLen, creatorLength);

    outBytes.insert (outBytes.end(), payload.begin(), payload.end());
    outBytes.insert (outBytes.end(), header.comment.begin(), header.comment.end());
    outBytes.insert (outBytes.end(), header.creatorData.begin(), header.creatorData.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::ReadLittle32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t TwoImgContainer::ReadLittle32 (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) bytes[at]
         | ((uint32_t) bytes[at + 1] << 8)
         | ((uint32_t) bytes[at + 2] << 16)
         | ((uint32_t) bytes[at + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::WriteLittle32
//
////////////////////////////////////////////////////////////////////////////////

void TwoImgContainer::WriteLittle32 (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    bytes[at]     = (Byte) (value & 0xFF);
    bytes[at + 1] = (Byte) ((value >> 8) & 0xFF);
    bytes[at + 2] = (Byte) ((value >> 16) & 0xFF);
    bytes[at + 3] = (Byte) (value >> 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool TwoImgContainer::IsInside (std::span<const Byte> bytes, uint32_t offset, uint32_t length)
{
    return (uint64_t) offset + length <= bytes.size();
}
