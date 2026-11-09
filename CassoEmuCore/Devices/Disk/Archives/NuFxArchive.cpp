#include "Pch.h"

#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Devices/Disk/Archives/NuLzw.h"
#include "Update/Inflate.h"



//  "NuFile" and "NuFx", each in alternating plain and high-bit ASCII.
static constexpr Byte  s_kMasterSignature[] = { 0x4E, 0xF5, 0x46, 0xE9, 0x6C, 0xE5 };
static constexpr Byte  s_kRecordSignature[] = { 0x4E, 0xF5, 0x46, 0xD8 };





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Read16
//
////////////////////////////////////////////////////////////////////////////////

Word NuFxArchive::Read16 (std::span<const Byte> bytes, size_t at)
{
    return (Word) (bytes[at] | (bytes[at + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Read32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t NuFxArchive::Read32 (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) Read16 (bytes, at) | ((uint32_t) Read16 (bytes, at + 2) << 16);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Write16
//
////////////////////////////////////////////////////////////////////////////////

void NuFxArchive::Write16 (std::vector<Byte> & bytes, Word value)
{
    bytes.push_back ((Byte) (value & 0xFF));
    bytes.push_back ((Byte) (value >> 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Write32
//
////////////////////////////////////////////////////////////////////////////////

void NuFxArchive::Write32 (std::vector<Byte> & bytes, uint32_t value)
{
    Write16 (bytes, (Word) (value & 0xFFFF));
    Write16 (bytes, (Word) (value >> 16));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::IsNuFx
//
////////////////////////////////////////////////////////////////////////////////

bool NuFxArchive::IsNuFx (std::span<const Byte> bytes)
{
    return bytes.size() >= kMasterHeaderSize
        && std::equal (std::begin (s_kMasterSignature), std::end (s_kMasterSignature), bytes.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Parse
//
//  The master header's CRC, from a seed of zero over everything after it,
//  must match; a record whose own header CRC doesn't is kept and marked, so
//  the others stay readable.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuFxArchive::Parse (std::span<const Byte> bytes, NuFxArchive & outArchive, std::string & outError)
{
    constexpr size_t  kOffCrc      = 6;
    constexpr size_t  kOffRecords  = 8;
    constexpr size_t  kOffCreated  = 12;
    constexpr size_t  kOffModified = 20;
    constexpr size_t  kOffVersion  = 28;



    HRESULT   hr       = S_OK;
    bool      isNuFx   = IsNuFx (bytes);
    bool      crcOk    = false;
    uint32_t  count    = 0;
    uint32_t  i        = 0;
    size_t    offset   = kMasterHeaderSize;



    outArchive = NuFxArchive();

    CBRFEx (isNuFx, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The file is not a ShrinkIt archive.");

    crcOk = NuLzw::ComputeCrc (bytes.subspan (kOffRecords, kMasterHeaderSize - kOffRecords), 0) == Read16 (bytes, kOffCrc);
    CBRFEx (crcOk, HRESULT_FROM_WIN32 (ERROR_CRC), outError = "The archive's master header doesn't match its CRC.");

    count                    = Read32 (bytes, kOffRecords);
    outArchive.masterVersion = Read16 (bytes, kOffVersion);

    std::copy_n (bytes.begin() + kOffCreated,  8, outArchive.created.begin());
    std::copy_n (bytes.begin() + kOffModified, 8, outArchive.modified.begin());

    for (i = 0; i < count; i++)
    {
        NuFxRecord  record;

        hr = ParseRecord (bytes, offset, record, outError);
        CHR (hr);

        outArchive.records.push_back (std::move (record));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::ParseRecord
//
//  The header's attribute count gives where the option list ends and the
//  filename length is, and the thread headers follow the header's filename.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuFxArchive::ParseRecord (std::span<const Byte> bytes, size_t & ioOffset, NuFxRecord & outRecord, std::string & outError)
{
    constexpr size_t  kOffCrc         = 4;
    constexpr size_t  kOffAttribCount = 6;
    constexpr size_t  kOffVersion     = 8;
    constexpr size_t  kOffThreads     = 10;
    constexpr size_t  kOffFileSystem  = 14;
    constexpr size_t  kOffFsInfo      = 16;
    constexpr size_t  kOffAccess      = 18;
    constexpr size_t  kOffFileType    = 22;
    constexpr size_t  kOffExtraType   = 26;
    constexpr size_t  kOffStorage     = 30;
    constexpr size_t  kOffCreated     = 32;
    constexpr size_t  kOffModified    = 40;
    constexpr size_t  kOffArchived    = 48;
    constexpr size_t  kOffOptionSize  = 56;



    HRESULT   hr          = S_OK;
    size_t    start       = ioOffset;
    bool      hasHeader   = start + kRecordFixedBytes <= bytes.size();
    bool      isRecord    = false;
    bool      fits        = false;
    Word      attribCount = 0;
    uint32_t  threads     = 0;
    size_t    nameLength  = 0;
    size_t    at          = 0;
    size_t    headersEnd  = 0;
    size_t    optionSize  = 0;
    uint32_t  i           = 0;



    CBRFEx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The archive ends partway through a record header.");

    isRecord = std::equal (std::begin (s_kRecordSignature), std::end (s_kRecordSignature), bytes.begin() + (ptrdiff_t) start);
    CBRFEx (isRecord, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A record doesn't start with its signature.");

    attribCount           = Read16 (bytes, start + kOffAttribCount);
    outRecord.version     = Read16 (bytes, start + kOffVersion);
    threads               = Read32 (bytes, start + kOffThreads);
    outRecord.fileSystem  = Read16 (bytes, start + kOffFileSystem);
    outRecord.fsInfo      = Read16 (bytes, start + kOffFsInfo);
    outRecord.access      = Read32 (bytes, start + kOffAccess);
    outRecord.fileType    = Read32 (bytes, start + kOffFileType);
    outRecord.extraType   = Read32 (bytes, start + kOffExtraType);
    outRecord.storageType = Read16 (bytes, start + kOffStorage);

    std::copy_n (bytes.begin() + (ptrdiff_t) (start + kOffCreated),  8, outRecord.created.begin());
    std::copy_n (bytes.begin() + (ptrdiff_t) (start + kOffModified), 8, outRecord.modified.begin());
    std::copy_n (bytes.begin() + (ptrdiff_t) (start + kOffArchived), 8, outRecord.archived.begin());

    fits = attribCount >= kRecordFixedBytes + 2 && start + attribCount <= bytes.size();
    CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A record's header length runs past the archive.");

    if (outRecord.version > 0 && attribCount >= kOffOptionSize + 4)
    {
        optionSize = std::min ((size_t) Read16 (bytes, start + kOffOptionSize), (size_t) attribCount - kOffOptionSize - 4);
        outRecord.optionList.assign (bytes.begin() + (ptrdiff_t) (start + kOffOptionSize + 2),
                                     bytes.begin() + (ptrdiff_t) (start + kOffOptionSize + 2 + optionSize));
    }

    nameLength = Read16 (bytes, start + attribCount - 2);
    at         = start + attribCount;
    headersEnd = at + nameLength + (size_t) threads * kThreadBytes;

    fits = headersEnd <= bytes.size();
    CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A record's thread headers run past the archive.");

    outRecord.headerName.assign (bytes.begin() + (ptrdiff_t) at, bytes.begin() + (ptrdiff_t) (at + nameLength));
    at += nameLength;

    outRecord.headerCrcOk = NuLzw::ComputeCrc (bytes.subspan (start + kOffAttribCount, headersEnd - start - kOffAttribCount), 0)
                         == Read16 (bytes, start + kOffCrc);

    for (i = 0; i < threads; i++, at += kThreadBytes)
    {
        NuFxThread  thread;

        thread.threadClass = Read16 (bytes, at);
        thread.format      = Read16 (bytes, at + 2);
        thread.kind        = Read16 (bytes, at + 4);
        thread.crc         = Read16 (bytes, at + 6);
        thread.eof         = Read32 (bytes, at + 8);
        thread.stored.resize (Read32 (bytes, at + 12));

        outRecord.threads.push_back (std::move (thread));
    }

    at = headersEnd;

    for (NuFxThread & thread : outRecord.threads)
    {
        fits = at + thread.stored.size() <= bytes.size();
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A thread's data runs past the end of the archive.");

        std::copy_n (bytes.begin() + (ptrdiff_t) at, thread.stored.size(), thread.stored.begin());
        at += thread.stored.size();
    }

    ioOffset = at;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::ExpandThread
//
//  A disk thread's length is the record's block count times its block size,
//  since some ShrinkIt versions left the thread's own length at zero.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuFxArchive::ExpandThread (const NuFxRecord & record, const NuFxThread & thread, std::vector<Byte> & outBytes, std::string & outError)
{
    constexpr Word  kVersionWithDataCrc = 3;



    HRESULT  hr      = S_OK;
    bool     isDisk  = thread.threadClass == NuFxThread::kClassData && thread.kind == NuFxThread::kKindDiskImage;
    size_t   blocks  = isDisk ? (size_t) record.extraType * (record.storageType == 0 ? 512u : record.storageType) : 0;
    size_t   length  = blocks != 0 ? blocks : (size_t) thread.eof;
    bool     crcOk   = true;



    outBytes.clear();

    switch (thread.format)
    {
        case NuFxThread::kFormatStored:
            outBytes.assign (thread.stored.begin(), thread.stored.begin() + (ptrdiff_t) std::min (length, thread.stored.size()));
            break;

        case NuFxThread::kFormatLzw1:
            hr = NuLzw::ExpandLzw1 (thread.stored, length, outBytes, outError);
            CHR (hr);
            break;

        case NuFxThread::kFormatLzw2:
            hr = NuLzw::ExpandLzw2 (thread.stored, length, outBytes, outError);
            CHR (hr);
            break;

        case NuFxThread::kFormatDeflate:
            hr = Inflate::Decompress (GetDeflateStream (thread.stored), length, outBytes);
            CHRF (hr, outError = "A Deflate thread doesn't expand.");
            break;

        default:
            CBRFEx (false, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), outError = std::format ("Thread format {} isn't one this reader expands.", thread.format));
            break;
    }

    if (thread.threadClass == NuFxThread::kClassData && record.version >= kVersionWithDataCrc)
    {
        crcOk = NuLzw::ComputeCrc (outBytes, kThreadCrcSeed) == thread.crc;
    }

    CBRFEx (crcOk, HRESULT_FROM_WIN32 (ERROR_CRC), outError = "A thread's data doesn't match its CRC.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::GetDeflateStream
//
//  A Deflate thread as raw Deflate data, after the two-byte zlib header that
//  NufxLib writes ahead of it, when it has one.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> NuFxArchive::GetDeflateStream (std::span<const Byte> stored)
{
    constexpr Byte      kZlibMethod   = 0x08;
    constexpr Byte      kMethodMask   = 0x0F;
    constexpr unsigned  kHeaderModulo = 31;



    bool  isZlib = stored.size() >= 2
                && (stored[0] & kMethodMask) == kZlibMethod
                && (((unsigned) stored[0] << 8) | stored[1]) % kHeaderModulo == 0;



    return isZlib ? stored.subspan (2) : stored;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::MakeDataThread
//
//  LZW/2 when it shrinks the data, stored otherwise.
//
////////////////////////////////////////////////////////////////////////////////

NuFxThread NuFxArchive::MakeDataThread (Word kind, std::span<const Byte> bytes)
{
    NuFxThread         thread;
    std::vector<Byte>  packed;



    thread.threadClass = NuFxThread::kClassData;
    thread.kind        = kind;
    thread.eof         = (uint32_t) bytes.size();
    thread.crc         = NuLzw::ComputeCrc (bytes, kThreadCrcSeed);

    NuLzw::CompressLzw2 (bytes, packed);

    if (packed.size() < bytes.size())
    {
        thread.format = NuFxThread::kFormatLzw2;
        thread.stored = std::move (packed);
    }
    else
    {
        thread.format = NuFxThread::kFormatStored;
        thread.stored.assign (bytes.begin(), bytes.end());
    }

    return thread;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::MakeFilenameThread
//
//  The name with eight bytes of room after it, so a rename that lengthens it
//  a little rewrites only the thread.
//
////////////////////////////////////////////////////////////////////////////////

NuFxThread NuFxArchive::MakeFilenameThread (const std::string & name)
{
    NuFxThread  thread;



    thread.threadClass = NuFxThread::kClassFilename;
    thread.kind        = NuFxThread::kKindFilename;
    thread.eof         = (uint32_t) name.size();
    thread.stored.assign (name.begin(), name.end());
    thread.stored.resize (name.size() + kFilenameSlack, 0);

    return thread;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::FindThread
//
////////////////////////////////////////////////////////////////////////////////

const NuFxThread * NuFxArchive::FindThread (const NuFxRecord & record, Word threadClass, Word kind)
{
    for (const NuFxThread & thread : record.threads)
    {
        if (thread.threadClass == threadClass && thread.kind == kind)
        {
            return &thread;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::string NuFxArchive::GetName (const NuFxRecord & record)
{
    const NuFxThread  * name = FindThread (record, NuFxThread::kClassFilename, NuFxThread::kKindFilename);



    if (name == nullptr)
    {
        return record.headerName;
    }

    return std::string (name->stored.begin(), name->stored.begin() + (ptrdiff_t) std::min ((size_t) name->eof, name->stored.size()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::BuildRecord
//
//  A version 3 header has an empty option list unless the record came with
//  one, and its name in a thread, so the header's own name length is zero.
//
////////////////////////////////////////////////////////////////////////////////

void NuFxArchive::BuildRecord (const NuFxRecord & record, std::vector<Byte> & outBytes)
{
    constexpr size_t  kOffCrc = 4;



    size_t             start      = outBytes.size();
    size_t             optionSize = record.optionList.size();
    size_t             optionPad  = optionSize % 2;
    size_t             attrib     = kRecordFixedBytes + 2 + optionSize + optionPad + 2;
    Word               crc        = 0;



    outBytes.insert (outBytes.end(), std::begin (s_kRecordSignature), std::end (s_kRecordSignature));
    Write16 (outBytes, 0);
    Write16 (outBytes, (Word) attrib);
    Write16 (outBytes, record.version);
    Write32 (outBytes, (uint32_t) record.threads.size());
    Write16 (outBytes, record.fileSystem);
    Write16 (outBytes, record.fsInfo);
    Write32 (outBytes, record.access);
    Write32 (outBytes, record.fileType);
    Write32 (outBytes, record.extraType);
    Write16 (outBytes, record.storageType);
    outBytes.insert (outBytes.end(), record.created.begin(),  record.created.end());
    outBytes.insert (outBytes.end(), record.modified.begin(), record.modified.end());
    outBytes.insert (outBytes.end(), record.archived.begin(), record.archived.end());
    Write16 (outBytes, (Word) optionSize);
    outBytes.insert (outBytes.end(), record.optionList.begin(), record.optionList.end());
    outBytes.insert (outBytes.end(), optionPad, 0);
    Write16 (outBytes, 0);

    for (const NuFxThread & thread : record.threads)
    {
        Write16 (outBytes, thread.threadClass);
        Write16 (outBytes, thread.format);
        Write16 (outBytes, thread.kind);
        Write16 (outBytes, thread.crc);
        Write32 (outBytes, thread.eof);
        Write32 (outBytes, (uint32_t) thread.stored.size());
    }

    crc = NuLzw::ComputeCrc (std::span<const Byte> (outBytes).subspan (start + kOffCrc + 2), 0);
    outBytes[start + kOffCrc]     = (Byte) (crc & 0xFF);
    outBytes[start + kOffCrc + 1] = (Byte) (crc >> 8);

    for (const NuFxThread & thread : record.threads)
    {
        outBytes.insert (outBytes.end(), thread.stored.begin(), thread.stored.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive::Build
//
////////////////////////////////////////////////////////////////////////////////

void NuFxArchive::Build (std::vector<Byte> & outBytes) const
{
    constexpr size_t  kOffCrc      = 6;
    constexpr size_t  kOffRecords  = 8;
    constexpr size_t  kOffEof      = 38;
    constexpr size_t  kReserved1   = 8;
    constexpr size_t  kReserved2   = 6;



    Word      crc = 0;
    uint32_t  eof = 0;



    outBytes.clear();
    outBytes.insert (outBytes.end(), std::begin (s_kMasterSignature), std::end (s_kMasterSignature));
    Write16 (outBytes, 0);
    Write32 (outBytes, (uint32_t) records.size());
    outBytes.insert (outBytes.end(), created.begin(),  created.end());
    outBytes.insert (outBytes.end(), modified.begin(), modified.end());
    Write16 (outBytes, masterVersion);
    outBytes.insert (outBytes.end(), kReserved1, 0);
    Write32 (outBytes, 0);
    outBytes.insert (outBytes.end(), kReserved2, 0);

    for (const NuFxRecord & record : records)
    {
        BuildRecord (record, outBytes);
    }

    eof = (uint32_t) outBytes.size();
    outBytes[kOffEof]     = (Byte) (eof & 0xFF);
    outBytes[kOffEof + 1] = (Byte) ((eof >> 8) & 0xFF);
    outBytes[kOffEof + 2] = (Byte) ((eof >> 16) & 0xFF);
    outBytes[kOffEof + 3] = (Byte) (eof >> 24);

    crc = NuLzw::ComputeCrc (std::span<const Byte> (outBytes).subspan (kOffRecords, kMasterHeaderSize - kOffRecords), 0);
    outBytes[kOffCrc]     = (Byte) (crc & 0xFF);
    outBytes[kOffCrc + 1] = (Byte) (crc >> 8);
}
