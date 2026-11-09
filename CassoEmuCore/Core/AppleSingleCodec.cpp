#include "Pch.h"

#include "Core/AppleSingleCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::IsAppleSingle
//
////////////////////////////////////////////////////////////////////////////////

bool AppleSingleCodec::IsAppleSingle (std::span<const Byte> bytes)
{
    return bytes.size() >= kHeaderSize && ReadBigEndian32 (bytes, 0) == kMagic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::IsAppleDouble
//
////////////////////////////////////////////////////////////////////////////////

bool AppleSingleCodec::IsAppleDouble (std::span<const Byte> bytes)
{
    return bytes.size() >= kHeaderSize && ReadBigEndian32 (bytes, 0) == kDoubleMagic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::Decode
//
//  Every entry's offset and length must lie inside the file; a version other
//  than 1 or 2 is refused, since its entry layout is not known. A dates entry
//  shorter than its four dates is an error rather than a partial read, since
//  which dates it holds cannot be known.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleSingleCodec::Decode (std::span<const Byte> bytes, AppleSingleFile & file, std::string & error)
{
    HRESULT   hr             = S_OK;
    uint32_t  version        = 0;
    size_t    count          = 0;
    bool      isMagic        = false;
    bool      isKnownVersion = false;
    bool      hasEntries     = false;



    file = AppleSingleFile();

    isMagic = IsAppleSingle (bytes) || IsAppleDouble (bytes);
    CBRFEx (isMagic, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is not an AppleSingle or AppleDouble container.");

    version        = ReadBigEndian32 (bytes, 4);
    isKnownVersion = version == kVersion1 || version == kVersion2;
    CBRFEx (isKnownVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("AppleSingle version ${:08X} is not one this reader knows.", version));

    count      = ReadBigEndian16 (bytes, kHeaderSize - 2);
    hasEntries = bytes.size() >= kHeaderSize + count * kEntrySize;
    CBRFEx (hasEntries, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The AppleSingle entry table runs past the end of the file.");

    for (size_t i = 0; i < count; ++i)
    {
        size_t    at       = kHeaderSize + i * kEntrySize;
        uint32_t  id       = ReadBigEndian32 (bytes, at);
        uint32_t  offset   = ReadBigEndian32 (bytes, at + 4);
        uint32_t  length   = ReadBigEndian32 (bytes, at + 8);
        bool      isInside = (uint64_t) offset + length <= bytes.size();



        CBRFEx (isInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("AppleSingle entry {} runs past the end of the file.", id));

        if (id == kDataForkId)
        {
            file.data.assign (bytes.begin() + offset, bytes.begin() + offset + length);
        }
        else if (id == kResourceForkId)
        {
            file.resourceFork.assign (bytes.begin() + offset, bytes.begin() + offset + length);
            file.hasResourceFork = true;
        }
        else if (id == kFinderInfoId && length >= kFinderInfoSize)
        {
            file.finderInfo.assign (bytes.begin() + offset, bytes.begin() + offset + kFinderInfoSize);
        }
        else if (id == kRealNameId)
        {
            file.realName.assign (bytes.begin() + offset, bytes.begin() + offset + length);
        }
        else if (id == kFileDatesId)
        {
            CBRFEx (length >= kFileDatesSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
                    error = std::format ("The AppleSingle dates entry is {} bytes; it needs {}.", length, kFileDatesSize));

            file.createDate = ReadDate (bytes, offset);
            file.modifyDate = ReadDate (bytes, offset + 4);
            file.backupDate = ReadDate (bytes, offset + 8);
            file.accessDate = ReadDate (bytes, offset + 12);
        }
        else if (id == kCassoNameSetId)
        {
            file.nameSetByUser = true;
        }
        else if (id == kProDosInfoId && length >= kProDosInfoSize)
        {
            file.hasProDosInfo = true;
            file.access        = ReadBigEndian16 (bytes, offset);
            file.fileType      = ReadBigEndian16 (bytes, offset + 2);
            file.auxType       = ReadBigEndian32 (bytes, offset + 4);
        }
        else if (length > 0)
        {
            file.hasOtherEntries = true;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::Encode
//
//  A version 2 file with the data fork, the real name when there is one, the
//  dates when any is set, the ProDOS info when it is set, and Casso's empty
//  name marker when the name was set by the user, in that order.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::Encode (const AppleSingleFile & file, std::vector<Byte> & bytes)
{
    EncodeEntries (file, kMagic, true, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::EncodeDouble
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::EncodeDouble (const AppleSingleFile & file, std::vector<Byte> & bytes)
{
    EncodeEntries (file, kDoubleMagic, false, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::EncodeEntries
//
//  The header and an entry descriptor for each entry the file has, then the
//  entries' bytes in the same order. An entry of no bytes, as the name
//  marker is, takes a descriptor and nothing after it.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::EncodeEntries (const AppleSingleFile & file, uint32_t magic, bool withData, std::vector<Byte> & bytes)
{
    bool                                               hasDates = file.createDate.has_value() || file.modifyDate.has_value() ||
                                                                  file.backupDate.has_value() || file.accessDate.has_value();
    std::vector<std::pair<uint32_t, std::vector<Byte>>>  entries;
    std::vector<Byte>                                  dates;
    std::vector<Byte>                                  info;
    size_t                                             offset   = 0;



    if (withData)
    {
        entries.push_back ({ kDataForkId, file.data });
    }

    if (file.hasResourceFork)
    {
        entries.push_back ({ kResourceForkId, file.resourceFork });
    }

    if (!file.realName.empty())
    {
        entries.push_back ({ kRealNameId, std::vector<Byte> (file.realName.begin(), file.realName.end()) });
    }

    if (hasDates)
    {
        WriteDate (dates, file.createDate);
        WriteDate (dates, file.modifyDate);
        WriteDate (dates, file.backupDate);
        WriteDate (dates, file.accessDate);
        entries.push_back ({ kFileDatesId, dates });
    }

    if (file.finderInfo.size() == kFinderInfoSize)
    {
        entries.push_back ({ kFinderInfoId, file.finderInfo });
    }

    if (file.hasProDosInfo)
    {
        WriteBigEndian16 (info, file.access);
        WriteBigEndian16 (info, file.fileType);
        WriteBigEndian32 (info, file.auxType);
        entries.push_back ({ kProDosInfoId, info });
    }

    if (file.nameSetByUser)
    {
        entries.push_back ({ kCassoNameSetId, std::vector<Byte>() });
    }

    bytes.clear();
    WriteBigEndian32 (bytes, magic);
    WriteBigEndian32 (bytes, kVersion2);
    bytes.insert (bytes.end(), kFillerSize, 0);
    WriteBigEndian16 (bytes, (Word) entries.size());

    offset = kHeaderSize + entries.size() * kEntrySize;

    for (const auto & entry : entries)
    {
        WriteBigEndian32 (bytes, entry.first);
        WriteBigEndian32 (bytes, (uint32_t) offset);
        WriteBigEndian32 (bytes, (uint32_t) entry.second.size());
        offset += entry.second.size();
    }

    for (const auto & entry : entries)
    {
        bytes.insert (bytes.end(), entry.second.begin(), entry.second.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::ReadBigEndian32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AppleSingleCodec::ReadBigEndian32 (std::span<const Byte> bytes, size_t at)
{
    static constexpr int  kByteBits = 8;



    return ((uint32_t) bytes[at] << (kByteBits * 3)) | ((uint32_t) bytes[at + 1] << (kByteBits * 2)) |
           ((uint32_t) bytes[at + 2] << kByteBits)   |  (uint32_t) bytes[at + 3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::ReadBigEndian16
//
////////////////////////////////////////////////////////////////////////////////

Word AppleSingleCodec::ReadBigEndian16 (std::span<const Byte> bytes, size_t at)
{
    static constexpr int  kByteBits = 8;



    return (Word) (((Word) bytes[at] << kByteBits) | bytes[at + 1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::WriteBigEndian32
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::WriteBigEndian32 (std::vector<Byte> & bytes, uint32_t value)
{
    static constexpr int  kByteBits = 8;



    bytes.push_back ((Byte) (value >> (kByteBits * 3)));
    bytes.push_back ((Byte) (value >> (kByteBits * 2)));
    bytes.push_back ((Byte) (value >> kByteBits));
    bytes.push_back ((Byte) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::WriteBigEndian16
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::WriteBigEndian16 (std::vector<Byte> & bytes, Word value)
{
    static constexpr int  kByteBits = 8;



    bytes.push_back ((Byte) (value >> kByteBits));
    bytes.push_back ((Byte) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::ReadDate
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int32_t> AppleSingleCodec::ReadDate (std::span<const Byte> bytes, size_t at)
{
    uint32_t  raw = ReadBigEndian32 (bytes, at);



    if (raw == kUnknownDate)
    {
        return std::nullopt;
    }

    return (int32_t) raw;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec::WriteDate
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::WriteDate (std::vector<Byte> & bytes, std::optional<int32_t> date)
{
    WriteBigEndian32 (bytes, date.has_value() ? (uint32_t) *date : kUnknownDate);
}
