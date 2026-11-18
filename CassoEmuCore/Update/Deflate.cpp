#include "Pch.h"

#include "Update/Deflate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RFC 1951 tables
//
//  Length codes 257-285 and distance codes 0-29: the base value of each code
//  and how many extra bits follow it (RFC 1951 section 3.2.5). The code
//  length alphabet's lengths go out in the permuted order of section 3.2.7.
//  Then zlib's search limits for levels 1 to 9.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint16_t  s_kLengthBase[] =
{
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static constexpr Byte  s_kLengthExtraBits[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static constexpr uint16_t  s_kDistanceBase[] =
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

//  Good, lazy, nice and chain for each level, from zlib's deflate.c.
static constexpr size_t  s_kTunings[][4] =
{
    { 0,   0,   0,    0    },
    { 4,   4,   8,    4    },
    { 4,   5,   16,   8    },
    { 4,   6,   32,   32   },
    { 4,   4,   16,   16   },
    { 8,   16,  32,   32   },
    { 8,   16,  128,  128  },
    { 8,   32,  128,  256  },
    { 32,  128, 258,  1024 },
    { 32,  258, 258,  4096 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Deflate
//
//  Levels 1 to 3 search greedily, as zlib's deflate_fast does, and 4 to 9
//  lazily, as its deflate_slow does.
//
////////////////////////////////////////////////////////////////////////////////

Deflate::Deflate (std::span<const Byte> data, std::vector<Byte> & output, int level) :
    m_data   (data),
    m_output (output),
    m_isLazy (level >= 4),
    m_head   (size_t (1) << kHashBits, -1),
    m_prev   (kWindow, -1)
{
    const size_t *  tuning = s_kTunings[std::clamp (level, kFastest, kBest)];



    m_tuning.good  = tuning[0];
    m_tuning.lazy  = tuning[1];
    m_tuning.nice  = tuning[2];
    m_tuning.chain = (int) tuning[3];

    m_tokens.reserve (kMaxSymbols);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Compress
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::Compress (std::span<const Byte> data, std::vector<Byte> & outBytes, int level)
{
    Deflate  encoder (data, outBytes, level);



    outBytes.clear();

    if (encoder.m_isLazy)
    {
        encoder.RunLazy();
    }
    else
    {
        encoder.RunGreedy();
    }

    encoder.FlushBlock (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Store
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::Store (std::span<const Byte> data, std::vector<Byte> & outBytes)
{
    Deflate  encoder (data, outBytes, kFastest);



    outBytes.clear();
    encoder.PutStored (data, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::RunGreedy
//
//  The longest match at each position is taken. The strings inside a match
//  are hashed only when the match is no longer than the level's lazy limit.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::RunGreedy()
{
    size_t   at     = 0;
    size_t   match  = 0;
    size_t   length = 0;
    size_t   k      = 0;
    int32_t  head   = -1;



    while (at < m_data.size())
    {
        head   = Insert (at);
        length = 0;

        if (head >= 0 && at - (size_t) head <= kWindow)
        {
            length = FindMatch (at, head, kMinMatch - 1, match);
        }

        if (length >= kMinMatch)
        {
            AddMatch (length, at - match);

            if (length <= m_tuning.lazy)
            {
                for (k = 1; k < length; k++)
                {
                    Insert (at + k);
                }
            }

            at += length;
        }
        else
        {
            AddLiteral();
            at++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::RunLazy
//
//  The match at a position is written only after the next position is
//  searched. When that position has a longer match, the first position goes
//  out as a literal, and the longer match is compared with the one after it.
//  A match of three bytes more than 4 KB back costs more than its literals,
//  so it isn't taken.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::RunLazy()
{
    size_t   at          = 0;
    size_t   matchLength = kMinMatch - 1;
    size_t   matchStart  = 0;
    size_t   prevLength  = 0;
    size_t   prevMatch   = 0;
    size_t   k           = 0;
    int32_t  head        = -1;
    bool     isHeld      = false;



    while (at < m_data.size())
    {
        head        = Insert (at);
        prevLength  = matchLength;
        prevMatch   = matchStart;
        matchLength = kMinMatch - 1;

        if (head >= 0 && prevLength < m_tuning.lazy && at - (size_t) head <= kWindow)
        {
            matchLength = FindMatch (at, head, prevLength, matchStart);

            if (matchLength == kMinMatch && at - matchStart > kTooFar)
            {
                matchLength = kMinMatch - 1;
            }
        }

        if (prevLength >= kMinMatch && matchLength <= prevLength)
        {
            AddMatch (prevLength, at - 1 - prevMatch);

            for (k = 2; k < prevLength; k++)
            {
                Insert (at - 1 + k);
            }

            at          = at - 1 + prevLength;
            isHeld      = false;
            matchLength = kMinMatch - 1;
        }
        else if (isHeld)
        {
            AddLiteral();
            at++;
        }
        else
        {
            isHeld = true;
            at++;
        }
    }

    if (isHeld)
    {
        AddLiteral();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Insert
//
//  Adds the string at a position to its hash chain, and returns the chain's
//  previous head, or -1. The last two positions start no string.
//
////////////////////////////////////////////////////////////////////////////////

int32_t Deflate::Insert (size_t position)
{
    uint32_t  hash  = 0;
    int32_t   prior = -1;



    if (position + kMinMatch <= m_data.size())
    {
        hash                       = Hash (m_data, position);
        prior                      = m_head[hash];
        m_prev[position % kWindow] = prior;
        m_head[hash]               = (int32_t) position;
    }

    return prior;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::FindMatch
//
//  Searches the chain from `candidate` for a match longer than prevLength,
//  as zlib's longest_match does: a quarter of the chain once prevLength is
//  good, and no further once a match is nice. A longer match sets ioMatch.
//  Returns the longest length found, or prevLength.
//
////////////////////////////////////////////////////////////////////////////////

size_t Deflate::FindMatch (size_t position, int32_t candidate, size_t prevLength, size_t & ioMatch) const
{
    size_t   lookahead = m_data.size() - position;
    size_t   nice      = std::min (m_tuning.nice, lookahead);
    size_t   best      = prevLength;
    size_t   length    = 0;
    int      chain     = prevLength >= m_tuning.good ? m_tuning.chain >> 2 : m_tuning.chain;
    int32_t  at        = candidate;
    int32_t  next      = -1;
    bool     isDone    = false;



    while (!isDone && at >= 0 && (size_t) at < position && position - (size_t) at <= kWindow && chain-- > 0)
    {
        if (best >= lookahead || m_data[(size_t) at + best] == m_data[position + best])
        {
            length = MatchLength (m_data, (size_t) at, position);

            if (length > best)
            {
                best    = length;
                ioMatch = (size_t) at;
                isDone  = best >= nice;
            }
        }

        next = m_prev[(size_t) at % kWindow];
        at   = next < at ? next : -1;
    }

    return std::min (best, lookahead);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::AddLiteral
//
//  The next byte not yet in a token, as a literal.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::AddLiteral()
{
    m_tokens.push_back ({ m_data[m_covered], 0 });
    m_covered++;

    if (m_tokens.size() >= kMaxSymbols)
    {
        FlushBlock (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::AddMatch
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::AddMatch (size_t length, size_t distance)
{
    m_tokens.push_back ({ (uint16_t) length, (uint16_t) distance });
    m_covered += length;

    if (m_tokens.size() >= kMaxSymbols)
    {
        FlushBlock (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::FlushBlock
//
//  The tokens since the last block, as whichever block is smallest: its
//  bytes stored, the fixed codes, or codes built from its symbols' counts.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::FlushBlock (bool isLast)
{
    constexpr uint64_t  kBlockHeaderBits = 3;
    constexpr size_t    kStoredHeader    = 5;



    std::span<const Byte>  raw          = m_data.subspan (m_blockStart, m_covered - m_blockStart);
    size_t                 storedBlocks = std::max<size_t> (1, (raw.size() + kMaxStored - 1) / kMaxStored);
    uint64_t               storedBytes  = raw.size() + storedBlocks * kStoredHeader;
    uint64_t               fixedBits    = 0;
    uint64_t               dynamicBits  = 0;
    std::vector<uint32_t>  litLenCounts   (kLitLenCodes, 0);
    std::vector<uint32_t>  distanceCounts (kDistanceCodes, 0);
    Code                   litLen;
    Code                   distance;
    Code                   fixedLitLen;
    Code                   fixedDistance;
    Header                 header;



    for (const Token & token : m_tokens)
    {
        if (token.distance == 0)
        {
            litLenCounts[token.length]++;
        }
        else
        {
            litLenCounts[kEndOfBlock + 1 + GetLengthCode (token.length)]++;
            distanceCounts[GetDistanceCode (token.distance)]++;
        }
    }

    litLenCounts[kEndOfBlock]++;

    BuildCode   (litLenCounts,   kMaxBits, litLen);
    BuildCode   (distanceCounts, kMaxBits, distance);
    BuildFixed  (fixedLitLen, fixedDistance);
    BuildHeader (litLen, distance, header);

    fixedBits   = kBlockHeaderBits + CountTokenBits (m_tokens, fixedLitLen, fixedDistance) + fixedLitLen.lengths[kEndOfBlock];
    dynamicBits = kBlockHeaderBits + header.bits + CountTokenBits (m_tokens, litLen, distance) + litLen.lengths[kEndOfBlock];

    if (storedBytes <= (std::min (fixedBits, dynamicBits) + 7) / 8)
    {
        PutStored (raw, isLast);
    }
    else if (fixedBits <= dynamicBits)
    {
        PutBits   ((isLast ? 1u : 0u) | (1u << 1), (int) kBlockHeaderBits);
        PutTokens (fixedLitLen, fixedDistance);
        PutCode   (fixedLitLen, kEndOfBlock);
    }
    else
    {
        PutDynamic (litLen, distance, header, isLast);
    }

    m_tokens.clear();
    m_blockStart = m_covered;

    if (isLast)
    {
        AlignToByte();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutStored
//
//  Stored blocks of up to 65,535 bytes: the block header's three bits, then
//  from the next byte the length, its complement and the bytes. The last
//  block is final when isLast is set.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutStored (std::span<const Byte> bytes, bool isLast)
{
    size_t  at   = 0;
    size_t  take = 0;
    bool    last = false;



    do
    {
        take = std::min (kMaxStored, bytes.size() - at);
        last = isLast && at + take == bytes.size();

        PutBits (last ? 1u : 0u, 3);
        AlignToByte();

        m_output.push_back ((Byte) (take & 0xFF));
        m_output.push_back ((Byte) (take >> 8));
        m_output.push_back ((Byte) (~take & 0xFF));
        m_output.push_back ((Byte) ((~take >> 8) & 0xFF));
        m_output.insert (m_output.end(), bytes.begin() + (ptrdiff_t) at, bytes.begin() + (ptrdiff_t) (at + take));

        at += take;
    }
    while (at < bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutDynamic
//
//  A dynamic block (RFC 1951 section 3.2.7): the counts of codes, the code
//  length alphabet's lengths, the runs of code lengths, then the tokens and
//  the end of the block.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutDynamic (const Code & litLen, const Code & distance, const Header & header, bool isLast)
{
    int  i = 0;



    PutBits ((isLast ? 1u : 0u) | (2u << 1), 3);
    PutBits ((uint32_t) (header.litLenCount - (kEndOfBlock + 1)), 5);
    PutBits ((uint32_t) (header.distanceCount - 1), 5);
    PutBits ((uint32_t) (header.lengthCount - 4), 4);

    for (i = 0; i < header.lengthCount; i++)
    {
        PutBits (header.lengthsCode.lengths[s_kCodeLengthOrder[i]], 3);
    }

    for (const Token & run : header.runs)
    {
        PutCode (header.lengthsCode, run.length);

        if (run.length == 16)
        {
            PutBits (run.distance, 2);
        }
        else if (run.length == 17)
        {
            PutBits (run.distance, 3);
        }
        else if (run.length == 18)
        {
            PutBits (run.distance, 7);
        }
    }

    PutTokens (litLen, distance);
    PutCode   (litLen, kEndOfBlock);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutTokens
//
//  Each literal's code, and each match's length code and extra bits, then
//  its distance code and extra bits.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutTokens (const Code & litLen, const Code & distance)
{
    int  lengthCode   = 0;
    int  distanceCode = 0;



    for (const Token & token : m_tokens)
    {
        if (token.distance == 0)
        {
            PutCode (litLen, token.length);
        }
        else
        {
            lengthCode   = GetLengthCode (token.length);
            distanceCode = GetDistanceCode (token.distance);

            PutCode (litLen, kEndOfBlock + 1 + lengthCode);
            PutBits ((uint32_t) (token.length - s_kLengthBase[lengthCode]), s_kLengthExtraBits[lengthCode]);
            PutCode (distance, distanceCode);
            PutBits ((uint32_t) (token.distance - s_kDistanceBase[distanceCode]), s_kDistanceExtraBits[distanceCode]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutBits
//
//  Values are packed from each byte's lowest bit up.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutBits (uint32_t value, int count)
{
    m_bitBuffer |= value << m_bitCount;
    m_bitCount  += count;

    while (m_bitCount >= 8)
    {
        m_output.push_back ((Byte) m_bitBuffer);
        m_bitBuffer >>= 8;
        m_bitCount   -= 8;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutCode
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutCode (const Code & code, int symbol)
{
    PutBits (code.codes[(size_t) symbol], code.lengths[(size_t) symbol]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::AlignToByte
//
//  A partly filled byte goes out, its high bits zero.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::AlignToByte()
{
    if (m_bitCount > 0)
    {
        m_output.push_back ((Byte) m_bitBuffer);
    }

    m_bitBuffer = 0;
    m_bitCount  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::BuildCode
//
//  A Huffman code for the counts, no code longer than maxBits. At least two
//  symbols get codes, as zlib does, so every code is complete. The tree is
//  built with two queues: the symbols sorted by count, and the joined nodes,
//  which come out in order of weight.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::BuildCode (std::span<const uint32_t> counts, int maxBits, Code & outCode)
{
    std::vector<uint32_t>  weights (counts.begin(), counts.end());
    std::vector<int>       order;
    std::vector<uint64_t>  nodeWeights;
    std::vector<int>       parents;
    std::vector<int>       depths;
    size_t                 used      = 0;
    size_t                 nodes     = 0;
    size_t                 leaf      = 0;
    size_t                 joined    = 0;
    size_t                 next      = 0;
    size_t                 i         = 0;
    size_t                 pick[2]   = {};
    bool                   isTooLong = false;



    for (i = 0; i < weights.size(); i++)
    {
        used += weights[i] != 0 ? 1 : 0;
    }

    for (i = 0; used < 2 && i < weights.size(); i++)
    {
        if (weights[i] == 0)
        {
            weights[i] = 1;
            used++;
        }
    }

    for (i = 0; i < weights.size(); i++)
    {
        if (weights[i] != 0)
        {
            order.push_back ((int) i);
        }
    }

    std::stable_sort (order.begin(), order.end(), [&weights] (int a, int b) { return weights[(size_t) a] < weights[(size_t) b]; });

    nodes = 2 * order.size() - 1;
    nodeWeights.assign (nodes, 0);
    parents.assign (nodes, -1);
    depths.assign (nodes, 0);

    for (i = 0; i < order.size(); i++)
    {
        nodeWeights[i] = weights[(size_t) order[i]];
    }

    joined = order.size();

    for (next = order.size(); next < nodes; next++)
    {
        for (size_t & chosen : pick)
        {
            if (leaf < order.size() && (joined >= next || nodeWeights[leaf] <= nodeWeights[joined]))
            {
                chosen = leaf++;
            }
            else
            {
                chosen = joined++;
            }
        }

        nodeWeights[next] = nodeWeights[pick[0]] + nodeWeights[pick[1]];
        parents[pick[0]]  = (int) next;
        parents[pick[1]]  = (int) next;
    }

    for (i = nodes - 1; i-- > 0;)
    {
        depths[i] = depths[(size_t) parents[i]] + 1;
    }

    outCode.lengths.assign (weights.size(), 0);

    for (i = 0; i < order.size(); i++)
    {
        outCode.lengths[(size_t) order[i]] = (Byte) std::min (depths[i], 255);
        isTooLong                          = isTooLong || depths[i] > maxBits;
    }

    if (isTooLong)
    {
        LimitLengths (order, maxBits, outCode.lengths);
    }

    AssignCodes (outCode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::LimitLengths
//
//  Codes past maxBits are cut to it, which oversubscribes the code. The
//  least frequent code shorter than the longest allowed is then lengthened,
//  the longest of those first, until the code fits; then the most frequent
//  codes are shortened while the code has room, so it is complete. `order`
//  is the coded symbols, least frequent first.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::LimitLengths (std::span<const int> order, int maxBits, std::vector<Byte> & ioLengths)
{
    uint32_t  limit   = 1u << maxBits;
    uint32_t  kraft   = 0;
    int       longest = -1;
    size_t    i       = 0;



    for (int symbol : order)
    {
        ioLengths[(size_t) symbol] = (Byte) std::min<int> (ioLengths[(size_t) symbol], maxBits);
        kraft                     += 1u << (maxBits - ioLengths[(size_t) symbol]);
    }

    while (kraft > limit)
    {
        longest = -1;

        for (int symbol : order)
        {
            if (ioLengths[(size_t) symbol] < maxBits && (longest < 0 || ioLengths[(size_t) symbol] > ioLengths[(size_t) longest]))
            {
                longest = symbol;
            }
        }

        kraft -= 1u << (maxBits - ioLengths[(size_t) longest] - 1);
        ioLengths[(size_t) longest]++;
    }

    while (kraft < limit)
    {
        for (i = order.size(); i-- > 0;)
        {
            Byte &  length = ioLengths[(size_t) order[i]];

            if (length > 1 && kraft + (1u << (maxBits - length)) <= limit)
            {
                kraft += 1u << (maxBits - length);
                length--;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::AssignCodes
//
//  The canonical codes of RFC 1951 section 3.2.2 for the lengths, each
//  stored with its bits reversed.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::AssignCodes (Code & ioCode)
{
    uint32_t  lengthCounts[kMaxBits + 1] = {};
    uint32_t  nextCode[kMaxBits + 1]     = {};
    uint32_t  code                       = 0;
    uint32_t  reversed                   = 0;
    int       bits                       = 0;
    int       i                          = 0;



    for (Byte length : ioCode.lengths)
    {
        lengthCounts[length]++;
    }

    lengthCounts[0] = 0;

    for (bits = 1; bits <= kMaxBits; bits++)
    {
        code           = (code + lengthCounts[bits - 1]) << 1;
        nextCode[bits] = code;
    }

    ioCode.codes.assign (ioCode.lengths.size(), 0);

    for (size_t symbol = 0; symbol < ioCode.lengths.size(); symbol++)
    {
        bits = ioCode.lengths[symbol];

        if (bits != 0)
        {
            code     = nextCode[bits]++;
            reversed = 0;

            for (i = 0; i < bits; i++)
            {
                reversed = (reversed << 1) | ((code >> i) & 1);
            }

            ioCode.codes[symbol] = (uint16_t) reversed;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::BuildFixed
//
//  The fixed codes of RFC 1951 section 3.2.6.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::BuildFixed (Code & outLitLen, Code & outDistance)
{
    constexpr size_t  kFixedLitLen   = 288;
    constexpr size_t  kFixedDistance = 30;
    constexpr Byte    kDistanceBits  = 5;



    size_t  symbol = 0;



    outLitLen.lengths.assign (kFixedLitLen, 8);

    for (symbol = 144; symbol < 256; symbol++)
    {
        outLitLen.lengths[symbol] = 9;
    }

    for (symbol = 256; symbol < 280; symbol++)
    {
        outLitLen.lengths[symbol] = 7;
    }

    outDistance.lengths.assign (kFixedDistance, kDistanceBits);

    AssignCodes (outLitLen);
    AssignCodes (outDistance);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::BuildHeader
//
//  The literal and length code's lengths up to its last used symbol, at
//  least 257 of them, then the distance code's, at least one, the runs of
//  each coded separately; then the code length alphabet's code, its lengths
//  sent up to the last one in the permuted order, at least four.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::BuildHeader (const Code & litLen, const Code & distance, Header & outHeader)
{
    std::vector<uint32_t>  lengthCounts (kLengthCodes, 0);



    outHeader.litLenCount   = kLitLenCodes;
    outHeader.distanceCount = kDistanceCodes;
    outHeader.lengthCount   = kLengthCodes;
    outHeader.runs.clear();

    while (outHeader.litLenCount > kEndOfBlock + 1 && litLen.lengths[(size_t) outHeader.litLenCount - 1] == 0)
    {
        outHeader.litLenCount--;
    }

    while (outHeader.distanceCount > 1 && distance.lengths[(size_t) outHeader.distanceCount - 1] == 0)
    {
        outHeader.distanceCount--;
    }

    ScanLengths (std::span<const Byte> (litLen.lengths).first ((size_t) outHeader.litLenCount), outHeader.runs);
    ScanLengths (std::span<const Byte> (distance.lengths).first ((size_t) outHeader.distanceCount), outHeader.runs);

    for (const Token & run : outHeader.runs)
    {
        lengthCounts[run.length]++;
    }

    BuildCode (lengthCounts, kMaxLengthBits, outHeader.lengthsCode);

    while (outHeader.lengthCount > 4 && outHeader.lengthsCode.lengths[s_kCodeLengthOrder[outHeader.lengthCount - 1]] == 0)
    {
        outHeader.lengthCount--;
    }

    outHeader.bits = 5 + 5 + 4 + 3 * (uint64_t) outHeader.lengthCount;

    for (const Token & run : outHeader.runs)
    {
        outHeader.bits += outHeader.lengthsCode.lengths[run.length];
        outHeader.bits += run.length == 16 ? 2 : run.length == 17 ? 3 : run.length == 18 ? 7 : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::ScanLengths
//
//  Code lengths as runs, as zlib's scan_tree codes them: 16 repeats the
//  previous length 3 to 6 times, 17 repeats a zero 3 to 10 times and 18
//  11 to 138 times. A run too short for those goes out length by length.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::ScanLengths (std::span<const Byte> lengths, std::vector<Token> & ioRuns)
{
    int     count    = 0;
    int     previous = -1;
    int     current  = 0;
    int     nextOne  = lengths.empty() ? 0 : lengths[0];
    int     maxCount = nextOne == 0 ? 138 : 7;
    int     minCount = nextOne == 0 ? 3 : 4;
    int     k        = 0;
    size_t  n        = 0;



    for (n = 0; n < lengths.size(); n++)
    {
        current = nextOne;
        nextOne = n + 1 < lengths.size() ? lengths[n + 1] : -1;

        if (++count >= maxCount || current != nextOne)
        {
            if (count < minCount)
            {
                for (k = 0; k < count; k++)
                {
                    ioRuns.push_back ({ (uint16_t) current, 0 });
                }
            }
            else if (current != 0)
            {
                if (current != previous)
                {
                    ioRuns.push_back ({ (uint16_t) current, 0 });
                    count--;
                }

                ioRuns.push_back ({ 16, (uint16_t) (count - 3) });
            }
            else if (count <= 10)
            {
                ioRuns.push_back ({ 17, (uint16_t) (count - 3) });
            }
            else
            {
                ioRuns.push_back ({ 18, (uint16_t) (count - 11) });
            }

            count    = 0;
            previous = current;
            maxCount = nextOne == 0 ? 138 : current == nextOne ? 6 : 7;
            minCount = nextOne == 0 || current == nextOne ? 3 : 4;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::GetLengthCode
//
//  The length code, 0 to 28, whose range has the length.
//
////////////////////////////////////////////////////////////////////////////////

int Deflate::GetLengthCode (size_t length)
{
    int  code = (int) std::size (s_kLengthBase) - 1;



    while (s_kLengthBase[code] > length)
    {
        code--;
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::GetDistanceCode
//
////////////////////////////////////////////////////////////////////////////////

int Deflate::GetDistanceCode (size_t distance)
{
    int  code = (int) std::size (s_kDistanceBase) - 1;



    while (s_kDistanceBase[code] > distance)
    {
        code--;
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::CountTokenBits
//
//  The tokens' size in bits with these codes, extra bits included.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Deflate::CountTokenBits (std::span<const Token> tokens, const Code & litLen, const Code & distance)
{
    uint64_t  bits         = 0;
    int       lengthCode   = 0;
    int       distanceCode = 0;



    for (const Token & token : tokens)
    {
        if (token.distance == 0)
        {
            bits += litLen.lengths[token.length];
        }
        else
        {
            lengthCode   = GetLengthCode (token.length);
            distanceCode = GetDistanceCode (token.distance);
            bits        += litLen.lengths[(size_t) (kEndOfBlock + 1 + lengthCode)] + s_kLengthExtraBits[lengthCode];
            bits        += distance.lengths[(size_t) distanceCode] + s_kDistanceExtraBits[distanceCode];
        }
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Hash
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Deflate::Hash (std::span<const Byte> data, size_t at)
{
    return (((uint32_t) data[at] << 10) ^ ((uint32_t) data[at + 1] << 5) ^ data[at + 2]) & ((1u << kHashBits) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::MatchLength
//
//  The bytes at `at` that match those at `candidate`, up to the longest
//  match a length code has.
//
////////////////////////////////////////////////////////////////////////////////

size_t Deflate::MatchLength (std::span<const Byte> data, size_t candidate, size_t at)
{
    size_t  limit  = std::min (kMaxMatch, data.size() - at);
    size_t  length = 0;



    while (length < limit && data[candidate + length] == data[at + length])
    {
        length++;
    }

    return length;
}
