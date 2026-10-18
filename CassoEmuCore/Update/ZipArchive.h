#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZipEntry
//
//  One extracted entry. `path` is the entry's name as stored, with '/'
//  separators; a directory entry ends in '/' and has no data.
//
////////////////////////////////////////////////////////////////////////////////

struct ZipEntry
{
    std::string         path;
    bool                isDirectory = false;
    std::vector<Byte>   data;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchive
//
//  Reads a whole zip held in memory, written from PKWARE's APPNOTE: the
//  central directory, stored (0) and deflated (8) entries, and a CRC-32
//  check of every entry. Encrypted entries, other methods, ZIP64, and any
//  entry path that is absolute or climbs with ".." are ERROR_INVALID_DATA,
//  so nothing extracted can land outside the folder it is written to.
//
////////////////////////////////////////////////////////////////////////////////

class ZipArchive
{
public:
    static constexpr size_t  kMaxEntryBytes = 0x10000000;     // 256 MB

    static HRESULT        Extract           (std::span<const Byte>    archive,
                                             std::vector<ZipEntry>  & outEntries);
    static std::uint32_t  ComputeCrc32      (std::span<const Byte> bytes);
    static bool           IsSafeEntryPath   (std::string_view path);

private:
    struct CentralRecord;

    static HRESULT        FindEndRecord     (std::span<const Byte> archive, size_t & outOffset);
    static HRESULT        ReadCentralRecord (std::span<const Byte>   archive,
                                             size_t                  offset,
                                             CentralRecord         & outRecord,
                                             size_t                & outNext);
    static HRESULT        ExtractEntry      (std::span<const Byte>    archive,
                                             const CentralRecord    & record,
                                             ZipEntry               & outEntry);
    static std::uint16_t  ReadU16           (std::span<const Byte> bytes, size_t offset);
    static std::uint32_t  ReadU32           (std::span<const Byte> bytes, size_t offset);
};
