#include "Pch.h"

#include "Update/Inflate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RFC 1951 tables
//
//  Length codes 257-285 and distance codes 0-29: the base value of each code
//  and how many extra bits follow it (RFC 1951 section 3.2.5). The code
//  length alphabet's lengths arrive in the permuted order of section 3.2.7.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr std::uint16_t  s_kLengthBase[] =
{
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static constexpr Byte  s_kLengthExtraBits[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static constexpr std::uint16_t  s_kDistanceBase[] =
{
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};

static constexpr Byte  s_kDistanceExtraBits[] =
{
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static constexpr Byte  s_kCodeLengthOrder[] =
{
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::Inflate
//
////////////////////////////////////////////////////////////////////////////////

Inflate::Inflate (std::span<const Byte> input, size_t maxOutputBytes, std::vector<Byte> & output) :
    m_input     (input),
    m_maxOutput (maxOutputBytes),
    m_output    (output)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::Decompress
//
//  Decodes a whole raw DEFLATE stream. On failure `outBytes` is empty, never
//  a partial result.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::Decompress (
    std::span<const Byte>   input,
    size_t                  maxOutputBytes,
    std::vector<Byte>     & outBytes)
{
    HRESULT  hr      = S_OK;
    Inflate  decoder (input, maxOutputBytes, outBytes);



    outBytes.clear();

    hr = decoder.Run();
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::Run
//
//  Blocks until the one marked final. Each starts with BFINAL (1 bit) and
//  BTYPE (2 bits); BTYPE 3 is reserved and is corrupt input.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::Run()
{
    static constexpr int  kBlockTypeBits = 2;
    HRESULT               hr             = S_OK;
    std::uint32_t         isFinal        = 0;
    std::uint32_t         type           = 0;



    do
    {
        hr = ReadBits (1, isFinal);
        CHR (hr);

        hr = ReadBits (kBlockTypeBits, type);
        CHR (hr);

        switch ((BlockType) type)
        {
            case BlockType::Stored:
                hr = InflateStored();
                break;

            case BlockType::Fixed:
                hr = InflateFixed();
                break;

            case BlockType::Dynamic:
                hr = InflateDynamic();
                break;

            default:
                hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
                break;
        }

        CHR (hr);
    } while (isFinal == 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::ReadBits
//
//  The next `count` bits (0-16), least significant first, as RFC 1951
//  packs everything but Huffman codes. Running out of input is corrupt
//  input.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::ReadBits (int count, std::uint32_t & outValue)
{
    static constexpr int  kBitsPerByte = 8;
    HRESULT               hr           = S_OK;
    bool                  hasInput     = false;



    outValue = 0;

    while (m_bitCount < count)
    {
        hasInput = m_pos < m_input.size();
        CBREx (hasInput, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        m_bitBuffer |= (std::uint32_t) m_input[m_pos] << m_bitCount;
        m_pos++;
        m_bitCount += kBitsPerByte;
    }

    outValue      = m_bitBuffer & ((1u << count) - 1);
    m_bitBuffer >>= count;
    m_bitCount   -= count;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::BuildHuffman
//
//  A canonical Huffman code from its code lengths (RFC 1951 section 3.2.2):
//  the number of codes of each length, and the symbols sorted by code. An
//  over-subscribed set of lengths cannot be a prefix code and is corrupt; an
//  incomplete one is accepted, and decoding fails only if an unused code
//  actually appears.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::BuildHuffman (std::span<const Byte> lengths, Huffman & outTable)
{
    HRESULT                                       hr      = S_OK;
    std::array<std::uint16_t, kOffsetSlots>       offsets = {};
    int                                           left    = 1;



    outTable = {};

    for (Byte length : lengths)
    {
        CBREx (length <= kMaxCodeBits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        outTable.counts[length]++;
    }

    outTable.counts[0] = 0;

    for (int len = 1; len <= kMaxCodeBits; len++)
    {
        left <<= 1;
        left  -= outTable.counts[len];
        CBREx (left >= 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        offsets[len + 1] = (std::uint16_t) (offsets[len] + outTable.counts[len]);
    }

    outTable.symbols.resize (offsets[kMaxCodeBits + 1]);

    for (size_t symbol = 0; symbol < lengths.size(); symbol++)
    {
        if (lengths[symbol] != 0)
        {
            outTable.symbols[offsets[lengths[symbol]]] = (std::uint16_t) symbol;
            offsets[lengths[symbol]]++;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::DecodeSymbol
//
//  One symbol, reading the code a bit at a time from its most significant
//  bit. Codes of each length are consecutive integers starting where the
//  previous length's codes left off, doubled; that is all the canonical
//  form needs to map a code to its symbol.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::DecodeSymbol (const Huffman & table, int & outSymbol)
{
    HRESULT        hr      = S_OK;
    std::uint32_t  bit     = 0;
    int            code    = 0;
    int            first   = 0;
    int            index   = 0;
    int            count   = 0;
    bool           isFound = false;



    outSymbol = -1;

    for (int len = 1; len <= kMaxCodeBits && !isFound; len++)
    {
        hr = ReadBits (1, bit);
        CHR (hr);

        code |= (int) bit;
        count = table.counts[len];

        if (code < first + count)
        {
            outSymbol = table.symbols[index + code - first];
            isFound   = true;
        }

        index  += count;
        first  += count;
        first <<= 1;
        code  <<= 1;
    }

    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::InflateStored
//
//  A stored block: the rest of the current byte is skipped, then LEN and its
//  one's complement NLEN (16 bits each, little-endian), then LEN raw bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::InflateStored()
{
    static constexpr size_t         kHeaderBytes = 4;
    static constexpr int            kByteShift   = 8;
    static constexpr std::uint32_t  kLengthMask  = 0xFFFF;
    HRESULT                         hr           = S_OK;
    std::uint32_t                   length       = 0;
    std::uint32_t                   complement   = 0;
    bool                            hasHeader    = false;
    bool                            hasPayload   = false;



    m_bitBuffer = 0;
    m_bitCount  = 0;

    hasHeader = m_input.size() - m_pos >= kHeaderBytes;
    CBREx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    length     = m_input[m_pos]     | ((std::uint32_t) m_input[m_pos + 1] << kByteShift);
    complement = m_input[m_pos + 2] | ((std::uint32_t) m_input[m_pos + 3] << kByteShift);
    m_pos     += kHeaderBytes;

    CBREx (length == (~complement & kLengthMask), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hasPayload = m_input.size() - m_pos >= length;
    CBREx (hasPayload, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (std::uint32_t i = 0; i < length; i++)
    {
        hr = PutByte (m_input[m_pos + i]);
        CHR (hr);
    }

    m_pos += length;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::InflateFixed
//
//  A block coded with the fixed Huffman codes of RFC 1951 section 3.2.6.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::InflateFixed()
{
    static constexpr int              kLastEightBitLow = 143;
    static constexpr int              kLastNineBit     = 255;
    static constexpr int              kLastSevenBit    = 279;
    static constexpr Byte             kSevenBits       = 7;
    static constexpr Byte             kEightBits       = 8;
    static constexpr Byte             kNineBits        = 9;
    static constexpr Byte             kDistanceBits    = 5;
    HRESULT                           hr               = S_OK;
    std::array<Byte, kLiteralCodes>   litLens          = {};
    std::array<Byte, kDistanceCodes>  distLens         = {};
    Huffman                           literals;
    Huffman                           distances;



    for (int symbol = 0; symbol < kLiteralCodes; symbol++)
    {
        if (symbol <= kLastEightBitLow)
        {
            litLens[symbol] = kEightBits;
        }
        else if (symbol <= kLastNineBit)
        {
            litLens[symbol] = kNineBits;
        }
        else if (symbol <= kLastSevenBit)
        {
            litLens[symbol] = kSevenBits;
        }
        else
        {
            litLens[symbol] = kEightBits;
        }
    }

    distLens.fill (kDistanceBits);

    hr = BuildHuffman (litLens, literals);
    CHRA (hr);

    hr = BuildHuffman (distLens, distances);
    CHRA (hr);

    hr = InflateCodes (literals, distances);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::InflateDynamic
//
//  A block that sends its own codes (RFC 1951 section 3.2.7): HLIT, HDIST
//  and HCLEN, the code length code's lengths, then the literal/length and
//  distance code lengths coded with it. A code without end-of-block can
//  never finish its block and is corrupt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::InflateDynamic()
{
    static constexpr int                kCountBits           = 5;
    static constexpr int                kCodeLengthCountBits = 4;
    static constexpr int                kCodeLengthBits      = 3;
    static constexpr int                kMinCodeLengthCount  = 4;
    static constexpr int                kMaxLiteralCount     = 286;
    HRESULT                             hr                   = S_OK;
    std::uint32_t                       litCount             = 0;
    std::uint32_t                       distCount            = 0;
    std::uint32_t                       clenCount            = 0;
    std::uint32_t                       value                = 0;
    std::array<Byte, kCodeLengthCodes>  clenLens             = {};
    std::vector<Byte>                   lengths;
    std::span<const Byte>               allLengths;
    bool                                hasEndOfBlock        = false;
    Huffman                             lengthTable;
    Huffman                             literals;
    Huffman                             distances;



    hr = ReadBits (kCountBits, litCount);
    CHR (hr);

    hr = ReadBits (kCountBits, distCount);
    CHR (hr);

    hr = ReadBits (kCodeLengthCountBits, clenCount);
    CHR (hr);

    litCount  += kFirstLengthCode;
    distCount += 1;
    clenCount += kMinCodeLengthCount;

    CBREx (litCount <= kMaxLiteralCount && distCount <= kDistanceCodes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (std::uint32_t i = 0; i < clenCount; i++)
    {
        hr = ReadBits (kCodeLengthBits, value);
        CHR (hr);

        clenLens[s_kCodeLengthOrder[i]] = (Byte) value;
    }

    hr = BuildHuffman (clenLens, lengthTable);
    CHR (hr);

    hr = ReadCodeLengths ((int) (litCount + distCount), lengthTable, lengths);
    CHR (hr);

    hasEndOfBlock = lengths[kEndOfBlock] != 0;
    CBREx (hasEndOfBlock, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    allLengths = lengths;

    hr = BuildHuffman (allLengths.first (litCount), literals);
    CHR (hr);

    hr = BuildHuffman (allLengths.subspan (litCount), distances);
    CHR (hr);

    hr = InflateCodes (literals, distances);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::ReadCodeLengths
//
//  `count` code lengths coded with the code length code: 0-15 literally,
//  16 repeats the previous length 3-6 times, 17 and 18 write 3-10 and
//  11-138 zeros. A repeat with nothing before it, or one that runs past
//  `count`, is corrupt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::ReadCodeLengths (int count, const Huffman & lengthTable, std::vector<Byte> & outLengths)
{
    static constexpr int  kRepeatPrevious  = 16;
    static constexpr int  kRepeatZeroShort = 17;
    static constexpr int  kRepeatPrevBits  = 2;
    static constexpr int  kRepeatPrevMin   = 3;
    static constexpr int  kZeroShortBits   = 3;
    static constexpr int  kZeroShortMin    = 3;
    static constexpr int  kZeroLongBits    = 7;
    static constexpr int  kZeroLongMin     = 11;
    HRESULT               hr               = S_OK;
    int                   symbol           = 0;
    std::uint32_t         extra            = 0;
    int                   repeat           = 0;
    Byte                  value            = 0;
    bool                  hasPrev          = false;
    bool                  fits             = false;



    outLengths.clear();

    while ((int) outLengths.size() < count)
    {
        hr = DecodeSymbol (lengthTable, symbol);
        CHR (hr);

        if (symbol < kRepeatPrevious)
        {
            outLengths.push_back ((Byte) symbol);
            continue;
        }

        if (symbol == kRepeatPrevious)
        {
            hasPrev = !outLengths.empty();
            CBREx (hasPrev, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            value = outLengths.back();
            hr    = ReadBits (kRepeatPrevBits, extra);
            CHR (hr);

            repeat = kRepeatPrevMin + (int) extra;
        }
        else if (symbol == kRepeatZeroShort)
        {
            value = 0;
            hr    = ReadBits (kZeroShortBits, extra);
            CHR (hr);

            repeat = kZeroShortMin + (int) extra;
        }
        else
        {
            value = 0;
            hr    = ReadBits (kZeroLongBits, extra);
            CHR (hr);

            repeat = kZeroLongMin + (int) extra;
        }

        fits = (int) outLengths.size() + repeat <= count;
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outLengths.insert (outLengths.end(), (size_t) repeat, value);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::InflateCodes
//
//  Literal bytes and length/distance matches until end-of-block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::InflateCodes (const Huffman & literals, const Huffman & distances)
{
    HRESULT  hr     = S_OK;
    int      symbol = 0;



    while (true)
    {
        hr = DecodeSymbol (literals, symbol);
        CHR (hr);

        if (symbol < kEndOfBlock)
        {
            hr = PutByte ((Byte) symbol);
            CHR (hr);
            continue;
        }

        if (symbol == kEndOfBlock)
        {
            break;
        }

        hr = CopyMatch (symbol, distances);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::CopyMatch
//
//  A match: the length from `lengthCode` and its extra bits, then a distance
//  code and its extra bits. The copy runs a byte at a time because a match
//  may overlap the bytes it is producing. A distance reaching before the
//  start of the output is corrupt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::CopyMatch (int lengthCode, const Huffman & distances)
{
    HRESULT        hr           = S_OK;
    std::uint32_t  extra        = 0;
    size_t         length       = 0;
    size_t         distance     = 0;
    size_t         from         = 0;
    int            distanceCode = 0;
    int            index        = lengthCode - kFirstLengthCode;
    bool           isInRange    = false;



    CBREx (lengthCode <= kMaxLengthCode, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadBits (s_kLengthExtraBits[index], extra);
    CHR (hr);

    length = s_kLengthBase[index] + extra;

    hr = DecodeSymbol (distances, distanceCode);
    CHR (hr);

    CBREx (distanceCode < kDistanceCodes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadBits (s_kDistanceExtraBits[distanceCode], extra);
    CHR (hr);

    distance  = s_kDistanceBase[distanceCode] + extra;
    isInRange = distance <= m_output.size();
    CBREx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    from = m_output.size() - distance;

    for (size_t i = 0; i < length; i++)
    {
        hr = PutByte (m_output[from + i]);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inflate::PutByte
//
//  Appends one output byte. Output past the caller's limit is treated as
//  corrupt input.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Inflate::PutByte (Byte value)
{
    HRESULT  hr     = S_OK;
    bool     isRoom = m_output.size() < m_maxOutput;



    CBREx (isRoom, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_output.push_back (value);

Error:
    return hr;
}
