#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw
//
//  ShrinkIt's two LZW methods for NuFX threads. The input is cut into chunks
//  of 4,096 bytes, the last padded with zeros; each chunk is run-length
//  encoded, then LZW-coded with 9- to 12-bit codes packed least significant
//  bit first. LZW/1 starts every chunk with an empty table and has a CRC of
//  the padded data in its header. LZW/2 keeps the table from chunk to chunk,
//  clears it in the stream with code $100 when it fills, and has each chunk's
//  compressed length in its header. A chunk that doesn't shrink is stored.
//
////////////////////////////////////////////////////////////////////////////////

class NuLzw
{
public:
    static constexpr size_t  kChunkBytes = 4096;
    static constexpr Byte    kRleEscape  = 0xDB;

    static HRESULT  ExpandLzw1   (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError);
    static HRESULT  ExpandLzw2   (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError);
    static void     CompressLzw2 (std::span<const Byte> in, std::vector<Byte> & outBytes);

    //  CRC-16/XMODEM, polynomial $1021, from a seed.
    static Word     ComputeCrc   (std::span<const Byte> data, Word seed);

private:
    static constexpr Word    kClearCode  = 0x100;
    static constexpr Word    kFirstCode  = 0x101;
    static constexpr Word    kTableSize  = 0x1000;
    static constexpr Word    kLastEntry  = 0xFFD;
    static constexpr size_t  kMinRun     = 4;
    static constexpr size_t  kMaxRun     = 256;

    //  What a decoder knows between codes: the table's next free entry, the
    //  last code read, whether the next code is the first since a clear, and
    //  whether that clear came from the stream.
    struct State
    {
        Word  next          = kFirstCode;
        Word  prev          = 0;
        bool  isFirst       = true;
        bool  clearedInLine = false;

        std::array<Word, kTableSize>  prefix = {};
        std::array<Byte, kTableSize>  suffix = {};
        std::array<Byte, kTableSize>  first  = {};
    };

    static int      GetCodeWidth (Word next);
    static void     ResetState   (State & state, bool clearedInLine);
    static void     EndChunk     (State & state);
    static HRESULT  ExpandRle    (std::span<const Byte> in, Byte escape, std::vector<Byte> & outChunk, std::string & outError);
    static void     CompressRle  (std::span<const Byte> chunk, Byte escape, std::vector<Byte> & outBytes);
    static HRESULT  ExpandCodes  (std::span<const Byte> in, size_t & ioPosition, size_t wanted, bool allowClear, State & state, std::vector<Byte> & outBytes, std::string & outError);
    static HRESULT  ExpandChunks (std::span<const Byte> in, size_t position, size_t outLength, bool isLzw2, std::vector<Byte> & outBytes, std::string & outError);
    static void     AppendString (const State & state, Word code, std::vector<Byte> & outBytes);
};
