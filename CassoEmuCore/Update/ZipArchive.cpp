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
    static constexpr std::uint32_t  kSignature      = 0x04034B50;
    static constexpr size_t         kHeaderSize     = 30;
    static constexpr size_t         kNameLenOffset  = 26;
    static constexpr size_t         kExtraLenOffset = 28;
    static constexpr std::uint16_t  kEncryptedFlag  = 0x0001;
    static constexpr std::uint16_t  kMethodStored   = 0;
    static constexpr std::uint16_t  kMethodDeflated = 8;
    HRESULT                         hr              = S_OK;
    size_t                          dataOffset      = 0;
    std::span<const Byte>           data;
    bool                            isSafePath      = false;
    bool                            isEncrypted     = (record.flags & kEncryptedFlag) != 0;
    bool                            isKnown         = record.method == kMethodStored || record.method == kMethodDeflated;
    bool                            isSmall         = record.uncompressedSize <= kMaxEntryBytes;
    bool                            hasHeader       = archive.size() >= kHeaderSize && record.localOffset <= archive.size() - kHeaderSize;
    bool                            hasData         = false;
    bool                            isSigned        = false;
    bool                            isSizeRight     = false;
    bool                            isCrcRight      = false;



    outEntry = {};

    isSafePath = IsSafeEntryPath (record.path);

    CBREx (isSafePath,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (!isEncrypted, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isKnown,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isSmall,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (hasHeader,    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isSigned = ReadU32 (archive, record.localOffset) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    dataOffset = record.localOffset + kHeaderSize +
                 ReadU16 (archive, record.localOffset + kNameLenOffset) +
                 ReadU16 (archive, record.localOffset + kExtraLenOffset);
    hasData    = dataOffset <= archive.size() && record.compressedSize <= archive.size() - dataOffset;
    CBREx (hasData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    data                 = archive.subspan (dataOffset, record.compressedSize);
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
