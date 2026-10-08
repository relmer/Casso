#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate
//
//  A DEFLATE decoder written from RFC 1951: stored, fixed-Huffman and
//  dynamic-Huffman blocks. Raw DEFLATE only, as a zip entry holds it; no
//  zlib or gzip wrapper. Corrupt input of any kind is ERROR_INVALID_DATA,
//  and so is output beyond `maxOutputBytes`, so a hostile stream cannot
//  expand without bound.
//
////////////////////////////////////////////////////////////////////////////////

class Inflate
{
public:
    static HRESULT Decompress (std::span<const Byte>   input,
                               size_t                  maxOutputBytes,
                               std::vector<Byte>     & outBytes);

private:
    static constexpr int     kMaxCodeBits      = 15;
    static constexpr int     kLiteralCodes     = 288;
    static constexpr int     kDistanceCodes    = 30;
    static constexpr int     kCodeLengthCodes  = 19;
    static constexpr int     kEndOfBlock       = 256;
    static constexpr int     kFirstLengthCode  = 257;
    static constexpr int     kMaxLengthCode    = 285;
    static constexpr int     kOffsetSlots      = kMaxCodeBits + 2;

    enum class BlockType
    {
        Stored  = 0,
        Fixed   = 1,
        Dynamic = 2,
    };

    struct Huffman
    {
        std::array<std::uint16_t, kMaxCodeBits + 1>  counts  = {};
        std::vector<std::uint16_t>                    symbols;
    };

    Inflate (std::span<const Byte> input, size_t maxOutputBytes, std::vector<Byte> & output);

    HRESULT         Run              ();
    HRESULT         ReadBits         (int count, std::uint32_t & outValue);
    HRESULT         DecodeSymbol     (const Huffman & table, int & outSymbol);
    HRESULT         InflateStored    ();
    HRESULT         InflateFixed     ();
    HRESULT         InflateDynamic   ();
    HRESULT         ReadCodeLengths  (int count, const Huffman & lengthTable, std::vector<Byte> & outLengths);
    HRESULT         InflateCodes     (const Huffman & literals, const Huffman & distances);
    HRESULT         CopyMatch        (int lengthCode, const Huffman & distances);
    HRESULT         PutByte          (Byte value);

    static HRESULT  BuildHuffman     (std::span<const Byte> lengths, Huffman & outTable);

    std::span<const Byte>   m_input;
    size_t                  m_pos       = 0;
    std::uint32_t           m_bitBuffer = 0;
    int                     m_bitCount  = 0;
    size_t                  m_maxOutput = 0;
    std::vector<Byte>     & m_output;
};
