#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgHeader
//
//  The fields of a 2IMG file's 64-byte header that are kept and written back:
//  the creator, the payload's format, the flags, the block count, and the
//  comment and creator data that follow the payload.
//
////////////////////////////////////////////////////////////////////////////////

struct TwoImgHeader
{
    static constexpr uint32_t  kFormatDosOrder    = 0;
    static constexpr uint32_t  kFormatProDosOrder = 1;
    static constexpr uint32_t  kFormatNibbles     = 2;

    //  Bit 31 marks the image locked, bit 8 marks the low byte as a DOS 3.3
    //  volume number.
    static constexpr uint32_t  kFlagLocked        = 0x80000000;
    static constexpr uint32_t  kFlagVolumeValid   = 0x00000100;

    std::array<Byte, 4>  creator     = { 'C', 'S', 'S', 'O' };
    uint32_t             format      = kFormatProDosOrder;
    uint32_t             flags       = 0;
    uint32_t             blockCount  = 0;
    std::string          comment;
    std::vector<Byte>    creatorData;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TwoImgContainer
//
//  Reads and writes the 2IMG container: a 64-byte little-endian header, the
//  payload in DOS sector order, ProDOS block order or as nibbles, then an
//  optional comment and an optional block of creator data.
//
////////////////////////////////////////////////////////////////////////////////

class TwoImgContainer
{
public:
    static constexpr size_t  kHeaderSize = 64;

    static bool     IsTwoImg (std::span<const Byte> bytes);
    static HRESULT  Parse    (std::span<const Byte> bytes, TwoImgHeader & outHeader, std::vector<Byte> & outPayload, std::string & outError);
    static void     Build    (const TwoImgHeader & header, std::span<const Byte> payload, std::vector<Byte> & outBytes);

private:
    static constexpr Word    kVersion       = 1;
    static constexpr size_t  kOffCreator    = 4;
    static constexpr size_t  kOffHeaderSize = 8;
    static constexpr size_t  kOffVersion    = 10;
    static constexpr size_t  kOffFormat     = 12;
    static constexpr size_t  kOffFlags      = 16;
    static constexpr size_t  kOffBlocks     = 20;
    static constexpr size_t  kOffData       = 24;
    static constexpr size_t  kOffDataLength = 28;
    static constexpr size_t  kOffComment    = 32;
    static constexpr size_t  kOffCommentLen = 36;
    static constexpr size_t  kOffCreator2   = 40;
    static constexpr size_t  kOffCreatorLen = 44;

    static uint32_t  ReadLittle32  (std::span<const Byte> bytes, size_t at);
    static void      WriteLittle32 (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static bool      IsInside      (std::span<const Byte> bytes, uint32_t offset, uint32_t length);
};
