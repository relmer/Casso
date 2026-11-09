#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryTwoEntry
//
//  One file of a Binary II archive: its ProDOS attributes, its path inside
//  the archive and its data. A directory is an entry of type DIR with no
//  data, ahead of the files in it.
//
////////////////////////////////////////////////////////////////////////////////

struct BinaryTwoEntry
{
    static constexpr Byte  kTypeDirectory = 0x0F;

    Byte               access      = 0xE3;
    Byte               fileType    = 0;
    Word               auxType     = 0;
    Byte               storageType = 1;
    Word               modDate     = 0;
    Word               modTime     = 0;
    Word               createDate  = 0;
    Word               createTime  = 0;
    std::string        name;
    std::vector<Byte>  data;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryTwoArchive
//
//  Reads and writes Binary II (Apple File Type Note $E0/0000): each file a
//  128-byte header and its data, padded to a multiple of 128 bytes. A Binary
//  II file of one NuFX archive is a `.bxy`, opened as the archive inside it.
//
////////////////////////////////////////////////////////////////////////////////

class BinaryTwoArchive
{
public:
    static constexpr size_t  kHeaderSize = 128;

    static bool     IsBinaryTwo (std::span<const Byte> bytes);
    static HRESULT  Parse       (std::span<const Byte> bytes, std::vector<BinaryTwoEntry> & outEntries, std::string & outError);
    static void     Build       (const std::vector<BinaryTwoEntry> & entries, std::vector<Byte> & outBytes);

private:
    static constexpr size_t  kOffAccess     = 3;
    static constexpr size_t  kOffFileType   = 4;
    static constexpr size_t  kOffAuxType    = 5;
    static constexpr size_t  kOffStorage    = 7;
    static constexpr size_t  kOffBlocks     = 8;
    static constexpr size_t  kOffModDate    = 10;
    static constexpr size_t  kOffModTime    = 12;
    static constexpr size_t  kOffCreateDate = 14;
    static constexpr size_t  kOffCreateTime = 16;
    static constexpr size_t  kOffIdByte     = 18;
    static constexpr size_t  kOffEof        = 20;
    static constexpr size_t  kOffName       = 23;
    static constexpr size_t  kOffEofHigh    = 116;
    static constexpr size_t  kOffVersion    = 126;
    static constexpr size_t  kOffFollowing  = 127;
    static constexpr size_t  kMaxName       = 64;
    static constexpr Byte    kIdByte        = 0x02;
};
