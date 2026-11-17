#include "Pch.h"

#include "Update/ZipArchive.h"
#include "Update/Inflate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::CentralRecord
//
//  The fields of one central directory file header that extraction needs.
//
////////////////////////////////////////////////////////////////////////////////

struct ZipArchive::CentralRecord
{
    std::uint16_t  flags            = 0;
    std::uint16_t  method           = 0;
    std::uint32_t  crc32            = 0;
    std::uint32_t  compressedSize   = 0;
    std::uint32_t  uncompressedSize = 0;
    std::uint32_t  localOffset      = 0;
    std::string    path;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::Extract
//
//  Every entry of the archive, in central directory order. The central
//  directory is the authority: a local header is read only to find where
//  its data starts. On any failure `outEntries` is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::Extract (
    std::span<const Byte>    archive,
    std::vector<ZipEntry>  & outEntries)
{
    static constexpr size_t         kDiskNumberOffset = 4;
    static constexpr size_t         kCdDiskOffset     = 6;
    static constexpr size_t         kEntryCountOffset = 10;
    static constexpr size_t         kCdSizeOffset     = 12;
    static constexpr size_t         kCdOffsetOffset   = 16;
    static constexpr std::uint16_t  kZip64Count       = 0xFFFF;
    static constexpr std::uint32_t  kZip64Offset      = 0xFFFFFFFF;
    HRESULT                         hr                = S_OK;
    size_t                          endOffset         = 0;
    std::uint16_t                   entryCount        = 0;
    std::uint32_t                   cdSize            = 0;
    std::uint32_t                   cdOffset          = 0;
    size_t                          offset            = 0;
    bool                            isOneDisk         = false;
    bool                            isZip64           = false;
    bool                            isCdInside        = false;
    CentralRecord                   record;
    ZipEntry                        entry;



    outEntries.clear();

    hr = FindEndRecord (archive, endOffset);
    CHR (hr);

    isOneDisk  = ReadU16 (archive, endOffset + kDiskNumberOffset) == 0 &&
                 ReadU16 (archive, endOffset + kCdDiskOffset)     == 0;
    entryCount = ReadU16 (archive, endOffset + kEntryCountOffset);
    cdSize     = ReadU32 (archive, endOffset + kCdSizeOffset);
    cdOffset   = ReadU32 (archive, endOffset + kCdOffsetOffset);
    isZip64    = entryCount == kZip64Count || cdSize == kZip64Offset || cdOffset == kZip64Offset;
    isCdInside = (std::uint64_t) cdOffset + cdSize <= endOffset;

    CBREx (isOneDisk,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (!isZip64,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isCdInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    offset = cdOffset;

    for (std::uint16_t i = 0; i < entryCount; i++)
    {
        hr = ReadCentralRecord (archive.first (endOffset), offset, record, offset);
        CHR (hr);

        hr = ExtractEntry (archive.first (cdOffset), record, entry);
        CHR (hr);

        outEntries.push_back (std::move (entry));
    }

Error:
    if (FAILED (hr))
    {
        outEntries.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::FindEndRecord
//
//  The end of central directory record: its signature, searched backward
//  from the last place it can start, across at most the longest archive
//  comment the format allows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::FindEndRecord (std::span<const Byte> archive, size_t & outOffset)
{
    static constexpr size_t         kEndRecordSize  = 22;
    static constexpr size_t         kMaxCommentSize = 0xFFFF;
    static constexpr std::uint32_t  kSignature      = 0x06054B50;
    HRESULT                         hr              = S_OK;
    size_t                          lowest          = 0;
    size_t                          pos             = 0;
    bool                            isFound         = false;
    bool                            isBigEnough     = archive.size() >= kEndRecordSize;



    outOffset = 0;

    CBREx (isBigEnough, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    pos    = archive.size() - kEndRecordSize;
    lowest = (pos > kMaxCommentSize) ? pos - kMaxCommentSize : 0;

    while (!isFound)
    {
        isFound = ReadU32 (archive, pos) == kSignature;

        if (isFound || pos == lowest)
        {
            break;
        }

        pos--;
    }

    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outOffset = pos;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::ReadCentralRecord
//
//  The central directory file header at `offset`, which must lie wholly
//  inside `archive`; `outNext` is where the following header starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::ReadCentralRecord (
    std::span<const Byte>   archive,
    size_t                  offset,
    CentralRecord         & outRecord,
    size_t                & outNext)
{
    static constexpr std::uint32_t  kSignature        = 0x02014B50;
    static constexpr size_t         kHeaderSize       = 46;
    static constexpr size_t         kFlagsOffset      = 8;
    static constexpr size_t         kMethodOffset     = 10;
    static constexpr size_t         kCrcOffset        = 16;
    static constexpr size_t         kCompSizeOffset   = 20;
    static constexpr size_t         kUncompSizeOffset = 24;
    static constexpr size_t         kNameLenOffset    = 28;
    static constexpr size_t         kExtraLenOffset   = 30;
    static constexpr size_t         kCommentLenOffset = 32;
    static constexpr size_t         kLocalOffset      = 42;
    HRESULT                         hr                = S_OK;
    size_t                          nameLength        = 0;
    size_t                          recordSize        = 0;
    bool                            hasHeader         = archive.size() >= kHeaderSize && offset <= archive.size() - kHeaderSize;
    bool                            hasRecord         = false;
    bool                            isSigned          = false;



    outRecord = {};
    outNext   = offset;

    CBREx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isSigned = ReadU32 (archive, offset) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    nameLength = ReadU16 (archive, offset + kNameLenOffset);
    recordSize = kHeaderSize + nameLength +
                 ReadU16 (archive, offset + kExtraLenOffset) +
                 ReadU16 (archive, offset + kCommentLenOffset);
    hasRecord  = recordSize <= archive.size() - offset;
    CBREx (hasRecord, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outRecord.flags            = ReadU16 (archive, offset + kFlagsOffset);
    outRecord.method           = ReadU16 (archive, offset + kMethodOffset);
    outRecord.crc32            = ReadU32 (archive, offset + kCrcOffset);
    outRecord.compressedSize   = ReadU32 (archive, offset + kCompSizeOffset);
    outRecord.uncompressedSize = ReadU32 (archive, offset + kUncompSizeOffset);
    outRecord.localOffset      = ReadU32 (archive, offset + kLocalOffset);
    outRecord.path.assign ((const char *) archive.data() + offset + kHeaderSize, nameLength);

    outNext = offset + recordSize;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::ExtractEntry
//
//  One entry's bytes, located through its local header and checked against
//  the central directory's CRC-32 and size. `archive` ends where the central
//  directory starts, so entry data cannot overlap it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::ExtractEntry (
    std::span<const Byte>    archive,
    const CentralRecord    & record,
    ZipEntry               & outEntry)
{
    static constexpr std::uint16_t  kEncryptedFlag  = 0x0001;
    static constexpr std::uint16_t  kMethodStored   = 0;
    static constexpr std::uint16_t  kMethodDeflated = 8;
    HRESULT                         hr              = S_OK;
    std::span<const Byte>           data;
    bool                            isSafePath      = false;
    bool                            isEncrypted     = (record.flags & kEncryptedFlag) != 0;
    bool                            isKnown         = record.method == kMethodStored || record.method == kMethodDeflated;
    bool                            isSmall         = record.uncompressedSize <= kMaxEntryBytes;
    bool                            isSizeRight     = false;
    bool                            isCrcRight      = false;



    outEntry = {};

    isSafePath = IsSafeEntryPath (record.path);

    CBREx (isSafePath,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (!isEncrypted, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isKnown,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isSmall,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = LocateData (archive, record, data);
    CHR (hr);

    outEntry.path        = record.path;
    outEntry.isDirectory = record.path.ends_with ('/');

    if (record.method == kMethodStored)
    {
        outEntry.data.assign (data.begin(), data.end());
    }
    else
    {
        hr = Inflate::Decompress (data, record.uncompressedSize, outEntry.data);
        CHR (hr);
    }

    isSizeRight = outEntry.data.size() == record.uncompressedSize;
    CBREx (isSizeRight, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isCrcRight = ComputeCrc32 (outEntry.data) == record.crc32;
    CBREx (isCrcRight, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    if (FAILED (hr))
    {
        outEntry = {};
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::LocateData
//
//  An entry's compressed bytes, after its local header's name and extra
//  field. `archive` ends where the central directory starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::LocateData (
    std::span<const Byte>    archive,
    const CentralRecord    & record,
    std::span<const Byte>  & outData)
{
    static constexpr std::uint32_t  kSignature      = 0x04034B50;
    static constexpr size_t         kHeaderSize     = 30;
    static constexpr size_t         kNameLenOffset  = 26;
    static constexpr size_t         kExtraLenOffset = 28;
    HRESULT                         hr              = S_OK;
    size_t                          dataOffset      = 0;
    bool                            hasHeader       = archive.size() >= kHeaderSize && record.localOffset <= archive.size() - kHeaderSize;
    bool                            hasData         = false;
    bool                            isSigned        = false;



    outData = {};

    CBREx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isSigned = ReadU32 (archive, record.localOffset) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    dataOffset = record.localOffset + kHeaderSize +
                 ReadU16 (archive, record.localOffset + kNameLenOffset) +
                 ReadU16 (archive, record.localOffset + kExtraLenOffset);
    hasData    = dataOffset <= archive.size() && record.compressedSize <= archive.size() - dataOffset;
    CBREx (hasData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outData = archive.subspan (dataOffset, record.compressedSize);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::Replace
//
//  Each local header is written again from its central record, without a
//  data descriptor, so the copied entries' offsets and sizes are exact. The
//  archive comment isn't copied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipArchive::Replace (
    std::span<const Byte>    archive,
    const std::string      & path,
    std::span<const Byte>    data,
    std::vector<Byte>      & outArchive)
{
    static constexpr std::uint32_t  kEndSignature     = 0x06054B50;
    static constexpr std::uint32_t  kCentralSignature = 0x02014B50;
    static constexpr std::uint16_t  kVersion          = 20;
    static constexpr std::uint16_t  kMethodStored     = 0;
    static constexpr std::uint16_t  kDosDate1980      = 0x0021;
    static constexpr size_t         kCentralSize      = 46;
    static constexpr size_t         kMadeByOffset     = 4;
    static constexpr size_t         kNeededOffset     = 6;
    static constexpr size_t         kDateOffset       = 14;
    static constexpr size_t         kNameLenOffset    = 28;
    static constexpr size_t         kEntryCountOffset = 10;
    static constexpr size_t         kCdOffsetOffset   = 16;
    HRESULT                         hr                = S_OK;
    size_t                          endOffset         = 0;
    std::uint16_t                   entryCount        = 0;
    std::uint32_t                   cdOffset          = 0;
    size_t                          offset            = 0;
    size_t                          next              = 0;
    std::uint16_t                   written           = 0;
    std::uint32_t                   directoryOffset   = 0;
    bool                            found             = false;
    bool                            isCdInside        = false;
    std::uint32_t                   dataCrc           = ComputeCrc32 (data);
    std::span<const Byte>           stored;
    std::vector<Byte>               directory;
    std::vector<Byte>               added;
    CentralRecord                   record;



    outArchive.clear();

    hr = FindEndRecord (archive, endOffset);
    CHR (hr);

    entryCount = ReadU16 (archive, endOffset + kEntryCountOffset);
    cdOffset   = ReadU32 (archive, endOffset + kCdOffsetOffset);
    isCdInside = cdOffset <= endOffset;
    CBREx (isCdInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    offset = cdOffset;

    for (std::uint16_t i = 0; i < entryCount; i++, written++)
    {
        hr = ReadCentralRecord (archive.first (endOffset), offset, record, next);
        CHR (hr);

        if (record.path == path)
        {
            found = true;
            AppendEntry (archive.subspan (offset, next - offset), kMethodStored, dataCrc, data, (std::uint32_t) data.size(), outArchive, directory);
        }
        else
        {
            hr = LocateData (archive.first (cdOffset), record, stored);
            CHR (hr);

            AppendEntry (archive.subspan (offset, next - offset), record.method, record.crc32, stored, record.uncompressedSize, outArchive, directory);
        }

        offset = next;
    }

    if (!found)
    {
        added.assign (kCentralSize, 0);
        SetU32 (added, 0,              kCentralSignature);
        SetU16 (added, kMadeByOffset,  kVersion);
        SetU16 (added, kNeededOffset,  kVersion);
        SetU16 (added, kDateOffset,    kDosDate1980);
        SetU16 (added, kNameLenOffset, (std::uint16_t) path.size());
        added.insert (added.end(), path.begin(), path.end());

        AppendEntry (added, kMethodStored, dataCrc, data, (std::uint32_t) data.size(), outArchive, directory);
        written++;
    }

    directoryOffset = (std::uint32_t) outArchive.size();

    outArchive.insert (outArchive.end(), directory.begin(), directory.end());
    PutU32 (outArchive, kEndSignature);
    PutU16 (outArchive, 0);
    PutU16 (outArchive, 0);
    PutU16 (outArchive, written);
    PutU16 (outArchive, written);
    PutU32 (outArchive, (std::uint32_t) directory.size());
    PutU32 (outArchive, directoryOffset);
    PutU16 (outArchive, 0);

Error:
    if (FAILED (hr))
    {
        outArchive.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::AppendEntry
//
//  A local header and the entry's bytes onto the archive, and its central
//  record onto the directory, both with the method, CRC, sizes and offset
//  given. The data descriptor flag is cleared in both.
//
////////////////////////////////////////////////////////////////////////////////

void ZipArchive::AppendEntry (
    std::span<const Byte>    central,
    std::uint16_t            method,
    std::uint32_t            crc32,
    std::span<const Byte>    stored,
    std::uint32_t            uncompressedSize,
    std::vector<Byte>      & outArchive,
    std::vector<Byte>      & outDirectory)
{
    static constexpr std::uint32_t  kLocalSignature   = 0x04034B50;
    static constexpr std::uint16_t  kDescriptorFlag   = 0x0008;
    static constexpr size_t         kNeededOffset     = 6;
    static constexpr size_t         kFlagsOffset      = 8;
    static constexpr size_t         kMethodOffset     = 10;
    static constexpr size_t         kTimeOffset       = 12;
    static constexpr size_t         kDateOffset       = 14;
    static constexpr size_t         kCrcOffset        = 16;
    static constexpr size_t         kCompSizeOffset   = 20;
    static constexpr size_t         kUncompSizeOffset = 24;
    static constexpr size_t         kNameLenOffset    = 28;
    static constexpr size_t         kLocalOffset      = 42;
    static constexpr size_t         kHeaderSize       = 46;
    std::uint16_t                   flags             = (std::uint16_t) (ReadU16 (central, kFlagsOffset) & ~kDescriptorFlag);
    std::uint16_t                   nameLength        = ReadU16 (central, kNameLenOffset);
    std::uint32_t                   localOffset       = (std::uint32_t) outArchive.size();
    size_t                          directoryAt       = outDirectory.size();



    PutU32 (outArchive, kLocalSignature);
    PutU16 (outArchive, ReadU16 (central, kNeededOffset));
    PutU16 (outArchive, flags);
    PutU16 (outArchive, method);
    PutU16 (outArchive, ReadU16 (central, kTimeOffset));
    PutU16 (outArchive, ReadU16 (central, kDateOffset));
    PutU32 (outArchive, crc32);
    PutU32 (outArchive, (std::uint32_t) stored.size());
    PutU32 (outArchive, uncompressedSize);
    PutU16 (outArchive, nameLength);
    PutU16 (outArchive, 0);
    outArchive.insert (outArchive.end(), central.begin() + kHeaderSize, central.begin() + kHeaderSize + nameLength);
    outArchive.insert (outArchive.end(), stored.begin(), stored.end());

    outDirectory.insert (outDirectory.end(), central.begin(), central.end());
    SetU16 (outDirectory, directoryAt + kFlagsOffset,      flags);
    SetU16 (outDirectory, directoryAt + kMethodOffset,     method);
    SetU32 (outDirectory, directoryAt + kCrcOffset,        crc32);
    SetU32 (outDirectory, directoryAt + kCompSizeOffset,   (std::uint32_t) stored.size());
    SetU32 (outDirectory, directoryAt + kUncompSizeOffset, uncompressedSize);
    SetU32 (outDirectory, directoryAt + kLocalOffset,      localOffset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::PutU16
//
//  Little-endian, appended.
//
////////////////////////////////////////////////////////////////////////////////

void ZipArchive::PutU16 (std::vector<Byte> & bytes, std::uint16_t value)
{
    bytes.push_back ((Byte) value);
    bytes.push_back ((Byte) (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::PutU32
//
////////////////////////////////////////////////////////////////////////////////

void ZipArchive::PutU32 (std::vector<Byte> & bytes, std::uint32_t value)
{
    PutU16 (bytes, (std::uint16_t) value);
    PutU16 (bytes, (std::uint16_t) (value >> 16));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::SetU16
//
//  Little-endian, over bytes already there.
//
////////////////////////////////////////////////////////////////////////////////

void ZipArchive::SetU16 (std::vector<Byte> & bytes, size_t offset, std::uint16_t value)
{
    bytes[offset]     = (Byte) value;
    bytes[offset + 1] = (Byte) (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::SetU32
//
////////////////////////////////////////////////////////////////////////////////

void ZipArchive::SetU32 (std::vector<Byte> & bytes, size_t offset, std::uint32_t value)
{
    SetU16 (bytes, offset,     (std::uint16_t) value);
    SetU16 (bytes, offset + 2, (std::uint16_t) (value >> 16));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::IsSafeEntryPath
//
//  Whether an entry path stays inside the folder it is extracted to: not
//  empty, not rooted, no drive or stream colon, and no ".." segment under
//  either separator.
//
////////////////////////////////////////////////////////////////////////////////

bool ZipArchive::IsSafeEntryPath (std::string_view path)
{
    std::string_view  rest    = path;
    std::string_view  segment;
    size_t            slash   = 0;
    bool              isSafe  = !path.empty()                          &&
                                path.front() != '/'                    &&
                                path.front() != '\\'                   &&
                                path.find (':') == std::string_view::npos;



    while (isSafe && !rest.empty())
    {
        slash   = rest.find_first_of ("/\\");
        segment = rest.substr (0, slash);
        rest    = (slash == std::string_view::npos) ? std::string_view() : rest.substr (slash + 1);
        isSafe  = segment != "..";
    }

    return isSafe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::ComputeCrc32
//
//  The CRC-32 zip uses: the reflected polynomial 0xEDB88320, initial and
//  final value all ones, a bit at a time.
//
////////////////////////////////////////////////////////////////////////////////

std::uint32_t ZipArchive::ComputeCrc32 (std::span<const Byte> bytes)
{
    static constexpr std::uint32_t  kPolynomial  = 0xEDB88320;
    static constexpr std::uint32_t  kAllOnes     = 0xFFFFFFFF;
    static constexpr int            kBitsPerByte = 8;
    std::uint32_t                   crc          = kAllOnes;



    for (Byte value : bytes)
    {
        crc ^= value;

        for (int bit = 0; bit < kBitsPerByte; bit++)
        {
            crc = (crc & 1) ? (crc >> 1) ^ kPolynomial : crc >> 1;
        }
    }

    return crc ^ kAllOnes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::ReadU16
//
//  Little-endian; the caller has checked the bytes are there.
//
////////////////////////////////////////////////////////////////////////////////

std::uint16_t ZipArchive::ReadU16 (std::span<const Byte> bytes, size_t offset)
{
    static constexpr int  kByteShift = 8;



    return (std::uint16_t) (bytes[offset] | (bytes[offset + 1] << kByteShift));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive::ReadU32
//
////////////////////////////////////////////////////////////////////////////////

std::uint32_t ZipArchive::ReadU32 (std::span<const Byte> bytes, size_t offset)
{
    static constexpr int  kWordShift = 16;



    return (std::uint32_t) ReadU16 (bytes, offset) | ((std::uint32_t) ReadU16 (bytes, offset + sizeof (std::uint16_t)) << kWordShift);
}
