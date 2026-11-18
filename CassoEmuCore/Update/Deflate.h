#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate
//
//  A DEFLATE encoder written from RFC 1951, at zlib's compression levels 1
//  to 9. Matches are found over a 32 KB window through hash chains, with
//  zlib's search limits for the level: a greedy search at levels 1 to 3, and
//  a lazy one from 4 up, in which a match is compared with the match at the
//  next byte before it is written. Each block of up to 16,383 symbols is
//  written stored, with the fixed codes or with Huffman codes built from its
//  symbols' counts, whichever is smallest. Raw DEFLATE only, as in a zip
//  entry or a gzip file.
//
////////////////////////////////////////////////////////////////////////////////

class Deflate
{
public:
    static constexpr int  kFastest = 1;
    static constexpr int  kDefault = 6;
    static constexpr int  kBest    = 9;

    //  The data at a level from kFastest to kBest; zlib's default is kDefault.
    static void  Compress (std::span<const Byte> data, std::vector<Byte> & outBytes, int level = kDefault);

    //  The data in stored blocks of up to 65,535 bytes, each a header byte,
    //  its length and the length's complement.
    static void  Store    (std::span<const Byte> data, std::vector<Byte> & outBytes);

private:
    static constexpr size_t    kMinMatch       = 3;
    static constexpr size_t    kMaxMatch       = 258;
    static constexpr size_t    kWindow         = 32768;
    static constexpr size_t    kTooFar         = 4096;
    static constexpr uint32_t  kHashBits       = 15;
    static constexpr size_t    kMaxStored      = 0xFFFF;
    static constexpr size_t    kMaxSymbols     = 16383;
    static constexpr int       kEndOfBlock     = 256;
    static constexpr int       kLitLenCodes    = 286;
    static constexpr int       kDistanceCodes  = 30;
    static constexpr int       kLengthCodes    = 19;
    static constexpr int       kMaxBits        = 15;
    static constexpr int       kMaxLengthBits  = 7;

    //  A literal when distance is 0, with the byte in length; otherwise a
    //  match of length bytes, distance back.
    struct Token
    {
        uint16_t  length   = 0;
        uint16_t  distance = 0;
    };

    //  zlib's configuration_table: a match this long cuts the chain search to
    //  a quarter; one this long is taken without a lazy look (or, greedy, is
    //  the longest whose strings are all hashed); one this long ends the
    //  search; and the chain is searched this deep.
    struct Tuning
    {
        size_t  good  = 0;
        size_t  lazy  = 0;
        size_t  nice  = 0;
        int     chain = 0;
    };

    //  A Huffman code: each symbol's length in bits and its code, its bits
    //  reversed, as they go out.
    struct Code
    {
        std::vector<Byte>      lengths;
        std::vector<uint16_t>  codes;
    };

    //  A dynamic block's header: the code lengths as runs of the code length
    //  alphabet (symbol in length, extra bits in distance), that alphabet's
    //  code, the counts of codes sent, and the header's size in bits.
    struct Header
    {
        Code                lengthsCode;
        std::vector<Token>  runs;
        int                 litLenCount   = 0;
        int                 distanceCount = 0;
        int                 lengthCount   = 0;
        uint64_t            bits          = 0;
    };

    Deflate (std::span<const Byte> data, std::vector<Byte> & output, int level);

    void      RunGreedy     ();
    void      RunLazy       ();
    int32_t   Insert        (size_t position);
    size_t    FindMatch     (size_t position, int32_t candidate, size_t prevLength, size_t & ioMatch) const;
    void      AddLiteral    ();
    void      AddMatch      (size_t length, size_t distance);
    void      FlushBlock    (bool isLast);
    void      PutStored     (std::span<const Byte> bytes, bool isLast);
    void      PutDynamic    (const Code & litLen, const Code & distance, const Header & header, bool isLast);
    void      PutTokens     (const Code & litLen, const Code & distance);
    void      PutBits       (uint32_t value, int count);
    void      PutCode       (const Code & code, int symbol);
    void      AlignToByte   ();

    static void      BuildCode         (std::span<const uint32_t> counts, int maxBits, Code & outCode);
    static void      LimitLengths      (std::span<const int> order, int maxBits, std::vector<Byte> & ioLengths);
    static void      AssignCodes       (Code & ioCode);
    static void      BuildFixed        (Code & outLitLen, Code & outDistance);
    static void      BuildHeader       (const Code & litLen, const Code & distance, Header & outHeader);
    static void      ScanLengths       (std::span<const Byte> lengths, std::vector<Token> & ioRuns);
    static int       GetLengthCode     (size_t length);
    static int       GetDistanceCode   (size_t distance);
    static uint64_t  CountTokenBits    (std::span<const Token> tokens, const Code & litLen, const Code & distance);
    static uint32_t  Hash              (std::span<const Byte> data, size_t at);
    static size_t    MatchLength       (std::span<const Byte> data, size_t candidate, size_t at);

    std::span<const Byte>   m_data;
    std::vector<Byte>     & m_output;
    Tuning                  m_tuning;
    bool                    m_isLazy     = false;
    std::vector<int32_t>    m_head;
    std::vector<int32_t>    m_prev;
    std::vector<Token>      m_tokens;
    size_t                  m_blockStart = 0;
    size_t                  m_covered    = 0;
    uint32_t                m_bitBuffer  = 0;
    int                     m_bitCount   = 0;
};
