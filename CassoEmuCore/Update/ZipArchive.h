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

    //  The archive with the entry at `path` stored with `data`, or a stored
    //  entry added at the end when no entry has that path. Every other
    //  entry is copied with its compressed bytes.
    static HRESULT        Replace           (std::span<const Byte>    archive,
                                             const std::string      & path,
                                             std::span<const Byte>    data,
                                             std::vector<Byte>      & outArchive);
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
    static HRESULT        LocateData        (std::span<const Byte>    archive,
                                             const CentralRecord    & record,
                                             std::span<const Byte>  & outData);
    static void           AppendEntry       (std::span<const Byte>    central,
                                             std::uint16_t            method,
                                             std::uint32_t            crc32,
                                             std::span<const Byte>    stored,
                                             std::uint32_t            uncompressedSize,
                                             std::vector<Byte>      & outArchive,
                                             std::vector<Byte>      & outDirectory);
    static void           PutU16            (std::vector<Byte> & bytes, std::uint16_t value);
    static void           PutU32            (std::vector<Byte> & bytes, std::uint32_t value);
    static void           SetU16            (std::vector<Byte> & bytes, size_t offset, std::uint16_t value);
    static void           SetU32            (std::vector<Byte> & bytes, size_t offset, std::uint32_t value);
    static std::uint16_t  ReadU16           (std::span<const Byte> bytes, size_t offset);
    static std::uint32_t  ReadU32           (std::span<const Byte> bytes, size_t offset);
};
