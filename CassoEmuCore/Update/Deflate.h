#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate
//
//  A DEFLATE encoder written from RFC 1951. Matches are found over a 32 KB
//  window through hash chains and written in one fixed-Huffman block; when
//  that would be no smaller than the data, the data is written in stored
//  blocks instead. Raw DEFLATE only, as in a zip entry or a gzip file.
//
////////////////////////////////////////////////////////////////////////////////

class Deflate
{
public:
    static void  Compress (std::span<const Byte> data, std::vector<Byte> & outBytes);

    //  The data in stored blocks of up to 65,535 bytes, each a header byte,
    //  its length and the length's complement.
    static void  Store    (std::span<const Byte> data, std::vector<Byte> & outBytes);

private:
    static constexpr size_t    kMinMatch   = 3;
    static constexpr size_t    kMaxMatch   = 258;
    static constexpr size_t    kWindow     = 32768;
    static constexpr uint32_t  kHashBits   = 15;
    static constexpr int       kMaxChain   = 128;
    static constexpr size_t    kMaxStored  = 0xFFFF;
    static constexpr int       kEndOfBlock = 256;

    explicit Deflate (std::vector<Byte> & output) : m_output (output) {}

    void      CompressFixed (std::span<const Byte> data);
    void      PutBits       (uint32_t value, int count);
    void      PutCode       (uint32_t code, int length);
    void      PutLiteral    (int symbol);
    void      PutMatch      (size_t length, size_t distance);
    void      Flush         ();

    static uint32_t  Hash        (std::span<const Byte> data, size_t at);
    static size_t    MatchLength (std::span<const Byte> data, size_t candidate, size_t at);

    std::vector<Byte>  & m_output;
    uint32_t             m_bitBuffer = 0;
    int                  m_bitCount  = 0;
};
