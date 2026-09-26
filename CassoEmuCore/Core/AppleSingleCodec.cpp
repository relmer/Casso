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

    isMagic = IsAppleSingle (bytes);
    CBRFEx (isMagic, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is not an AppleSingle container.");

    version        = ReadBigEndian32 (bytes, 4);
    isKnownVersion = version == kVersion1 || version == kVersion2;
    CBRFEx (isKnownVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("AppleSingle version ${:08X} is not supported.", version));

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
        else if (id == kProDosInfoId && length >= kProDosInfoSize)
        {
            file.hasProDosInfo = true;
            file.access        = ReadBigEndian16 (bytes, offset);
            file.fileType      = ReadBigEndian16 (bytes, offset + 2);
            file.auxType       = ReadBigEndian32 (bytes, offset + 4);
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
//  dates when any is set, and the ProDOS info when it is set, in that order.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSingleCodec::Encode (const AppleSingleFile & file, std::vector<Byte> & bytes)
{
    bool    hasDates = file.createDate.has_value() || file.modifyDate.has_value() ||
                       file.backupDate.has_value() || file.accessDate.has_value();
    size_t  count    = 1 + (file.realName.empty() ? 0 : 1) + (hasDates ? 1 : 0) + (file.hasProDosInfo ? 1 : 0);
    size_t  offset   = kHeaderSize + count * kEntrySize;



    bytes.clear();
    WriteBigEndian32 (bytes, kMagic);
    WriteBigEndian32 (bytes, kVersion2);
    bytes.insert (bytes.end(), kFillerSize, 0);
    WriteBigEndian16 (bytes, (Word) count);

    WriteBigEndian32 (bytes, kDataForkId);
    WriteBigEndian32 (bytes, (uint32_t) offset);
    WriteBigEndian32 (bytes, (uint32_t) file.data.size());
    offset += file.data.size();

    if (!file.realName.empty())
    {
        WriteBigEndian32 (bytes, kRealNameId);
        WriteBigEndian32 (bytes, (uint32_t) offset);
        WriteBigEndian32 (bytes, (uint32_t) file.realName.size());
        offset += file.realName.size();
    }

    if (hasDates)
    {
        WriteBigEndian32 (bytes, kFileDatesId);
        WriteBigEndian32 (bytes, (uint32_t) offset);
        WriteBigEndian32 (bytes, (uint32_t) kFileDatesSize);
        offset += kFileDatesSize;
    }

    if (file.hasProDosInfo)
    {
        WriteBigEndian32 (bytes, kProDosInfoId);
        WriteBigEndian32 (bytes, (uint32_t) offset);
        WriteBigEndian32 (bytes, (uint32_t) kProDosInfoSize);
    }

    bytes.insert (bytes.end(), file.data.begin(), file.data.end());
    bytes.insert (bytes.end(), file.realName.begin(), file.realName.end());

    if (hasDates)
    {
        WriteDate (bytes, file.createDate);
        WriteDate (bytes, file.modifyDate);
        WriteDate (bytes, file.backupDate);
        WriteDate (bytes, file.accessDate);
    }

    if (file.hasProDosInfo)
    {
        WriteBigEndian16 (bytes, file.access);
        WriteBigEndian16 (bytes, file.fileType);
        WriteBigEndian32 (bytes, file.auxType);
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
