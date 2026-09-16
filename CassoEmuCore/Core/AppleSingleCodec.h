#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleFile
//
//  What an AppleSingle container carries that matters here: the data fork,
//  the real name, the file dates, and the ProDOS file info entry when the file
//  has one. The resource fork and the other entries are read past and not
//  kept.
//
//  A date is seconds from 2000-01-01 00:00 UTC, negative before it. Each is
//  empty when the file has no dates entry or the entry marks that date
//  unknown, and Encode writes the entry only when at least one is set.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleSingleFile
{
    std::vector<Byte>  data;
    std::string        realName;
    bool               hasProDosInfo = false;
    Word               access        = 0;
    Word               fileType      = 0;
    uint32_t           auxType       = 0;

    std::optional<int32_t>  createDate;
    std::optional<int32_t>  modifyDate;
    std::optional<int32_t>  backupDate;
    std::optional<int32_t>  accessDate;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodec
//
//  Reads and writes the AppleSingle container (Apple Computer, "AppleSingle/
//  AppleDouble Formats for Foreign Files", also RFC 1740): a big-endian
//  header of magic, version and entry count, then entry descriptors of id,
//  offset and length. Version 1 and 2 files are read; version 2 is written.
//  The codec knows nothing about memory, disks or the debugger.
//
////////////////////////////////////////////////////////////////////////////////

class AppleSingleCodec
{
public:
    static constexpr uint32_t  kMagic          = 0x00051600;
    static constexpr uint32_t  kVersion1       = 0x00010000;
    static constexpr uint32_t  kVersion2       = 0x00020000;

    static bool     IsAppleSingle (std::span<const Byte> bytes);
    static HRESULT  Decode        (std::span<const Byte> bytes, AppleSingleFile & file, std::string & error);
    static void     Encode        (const AppleSingleFile & file, std::vector<Byte> & bytes);

private:
    static constexpr size_t    kHeaderSize     = 26;      // magic, version, 16 filler bytes, entry count
    static constexpr size_t    kEntrySize      = 12;
    static constexpr size_t    kFillerSize     = 16;
    static constexpr size_t    kProDosInfoSize = 8;
    static constexpr size_t    kFileDatesSize  = 16;      // create, modify, backup, access
    static constexpr uint32_t  kDataForkId     = 1;
    static constexpr uint32_t  kRealNameId     = 3;
    static constexpr uint32_t  kFileDatesId    = 8;
    static constexpr uint32_t  kProDosInfoId   = 11;
    static constexpr uint32_t  kUnknownDate    = 0x80000000;

    static uint32_t  ReadBigEndian32 (std::span<const Byte> bytes, size_t at);
    static Word      ReadBigEndian16 (std::span<const Byte> bytes, size_t at);
    static void      WriteBigEndian32 (std::vector<Byte> & bytes, uint32_t value);
    static void      WriteBigEndian16 (std::vector<Byte> & bytes, Word value);

    static std::optional<int32_t>  ReadDate  (std::span<const Byte> bytes, size_t at);
    static void                    WriteDate (std::vector<Byte> & bytes, std::optional<int32_t> date);
};
