#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxThread
//
//  One thread of a record: its class, format and kind, its CRC, its length
//  expanded and stored, and its bytes as stored.
//
////////////////////////////////////////////////////////////////////////////////

struct NuFxThread
{
    static constexpr Word  kClassMessage  = 0x0000;
    static constexpr Word  kClassData     = 0x0002;
    static constexpr Word  kClassFilename = 0x0003;
    static constexpr Word  kKindComment   = 0x0001;
    static constexpr Word  kKindDataFork  = 0x0000;
    static constexpr Word  kKindDiskImage = 0x0001;
    static constexpr Word  kKindResource  = 0x0002;
    static constexpr Word  kKindFilename  = 0x0000;

    static constexpr Word  kFormatStored  = 0;
    static constexpr Word  kFormatSqueeze = 1;
    static constexpr Word  kFormatLzw1    = 2;
    static constexpr Word  kFormatLzw2    = 3;
    static constexpr Word  kFormatLzc12   = 4;
    static constexpr Word  kFormatLzc16   = 5;
    static constexpr Word  kFormatDeflate = 6;
    static constexpr Word  kFormatBzip2   = 7;

    Word               threadClass = 0;
    Word               format      = kFormatStored;
    Word               kind        = 0;
    Word               crc         = 0;
    uint32_t           eof         = 0;
    std::vector<Byte>  stored;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxRecord
//
//  One record's header and threads. The dates are NuFX's 8-byte form:
//  second, minute, hour, year less 1900, day and month from 0, a filler and
//  the weekday.
//
////////////////////////////////////////////////////////////////////////////////

struct NuFxRecord
{
    static constexpr Word  kFsProDos = 0x0001;
    static constexpr Word  kFsDos33  = 0x0002;

    Word                     version     = 3;
    Word                     fileSystem  = kFsProDos;
    Word                     fsInfo      = '/';
    uint32_t                 access      = 0xE3;
    uint32_t                 fileType    = 0;
    uint32_t                 extraType   = 0;
    Word                     storageType = 1;
    std::array<Byte, 8>      created     = {};
    std::array<Byte, 8>      modified    = {};
    std::array<Byte, 8>      archived    = {};
    std::vector<Byte>        optionList;
    std::string              headerName;
    std::vector<NuFxThread>  threads;

    //  Whether the header's CRC matched when it was read, and whether each
    //  data thread's did once expanded.
    bool                     headerCrcOk = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchive
//
//  Reads and writes NuFX, ShrinkIt's archive format: a 48-byte master header,
//  then records, each a header, an array of 16-byte thread headers, and the
//  threads' bytes. Every CRC is CRC-16/XMODEM; the headers' are seeded with
//  zero, and a version 3 record's data threads are seeded with $FFFF over
//  the expanded bytes.
//
////////////////////////////////////////////////////////////////////////////////

class NuFxArchive
{
public:
    static constexpr size_t  kMasterHeaderSize = 48;

    Word                     masterVersion = 2;
    std::array<Byte, 8>      created       = {};
    std::array<Byte, 8>      modified      = {};
    std::vector<NuFxRecord>  records;

    static bool     IsNuFx (std::span<const Byte> bytes);
    static HRESULT  Parse  (std::span<const Byte> bytes, NuFxArchive & outArchive, std::string & outError);
    void            Build  (std::vector<Byte> & outBytes) const;

    //  A thread's bytes expanded, its CRC checked where the record's version
    //  has one.
    static HRESULT  ExpandThread (const NuFxRecord & record, const NuFxThread & thread, std::vector<Byte> & outBytes, std::string & outError);

    //  A data, resource or disk thread, LZW/2-compressed with its CRC, or a
    //  stored filename thread with room to rename.
    static NuFxThread  MakeDataThread     (Word kind, std::span<const Byte> bytes);
    static NuFxThread  MakeFilenameThread (const std::string & name);

    //  A ShrinkIt disk's record: the disk's blocks in ProDOS order, its name,
    //  and the file system on it.
    static NuFxRecord  MakeDiskRecord     (std::span<const Byte> blocks, const std::string & name, Word fileSystem);

    //  The record's name, from its filename thread or, in an old record, its
    //  header; and the thread of a class and kind, if it has one.
    static std::string         GetName    (const NuFxRecord & record);
    static const NuFxThread  * FindThread (const NuFxRecord & record, Word threadClass, Word kind);

private:
    static constexpr size_t  kRecordFixedBytes = 56;
    static constexpr size_t  kThreadBytes      = 16;
    static constexpr size_t  kFilenameSlack    = 8;
    static constexpr Word    kThreadCrcSeed    = 0xFFFF;

    static HRESULT                ParseRecord      (std::span<const Byte> bytes, size_t & ioOffset, NuFxRecord & outRecord, std::string & outError);
    static std::span<const Byte>  GetDeflateStream (std::span<const Byte> stored);
    static void                   BuildRecord      (const NuFxRecord & record, std::vector<Byte> & outBytes);

    static Word      Read16  (std::span<const Byte> bytes, size_t at);
    static uint32_t  Read32  (std::span<const Byte> bytes, size_t at);
    static void      Write16 (std::vector<Byte> & bytes, Word value);
    static void      Write32 (std::vector<Byte> & bytes, uint32_t value);
};
