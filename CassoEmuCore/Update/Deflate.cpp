#include "Pch.h"

#include "Update/Deflate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RFC 1951 tables
//
//  Length codes 257-285 and distance codes 0-29: the base value of each code
//  and how many extra bits follow it (RFC 1951 section 3.2.5).
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





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Compress
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::Compress (std::span<const Byte> data, std::vector<Byte> & outBytes)
{
    constexpr size_t  kStoredHeader = 5;



    std::vector<Byte>  fixed;
    Deflate            encoder (fixed);
    size_t             blocks = std::max<size_t> (1, (data.size() + kMaxStored - 1) / kMaxStored);



    encoder.CompressFixed (data);

    if (fixed.size() < data.size() + blocks * kStoredHeader)
    {
        outBytes = std::move (fixed);
    }
    else
    {
        Store (data, outBytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Store
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::Store (std::span<const Byte> data, std::vector<Byte> & outBytes)
{
    size_t  at = 0;



    outBytes.clear();

    do
    {
        size_t  take = std::min (kMaxStored, data.size() - at);
        bool    last = at + take == data.size();

        outBytes.push_back (last ? 1 : 0);
        outBytes.push_back ((Byte) (take & 0xFF));
        outBytes.push_back ((Byte) (take >> 8));
        outBytes.push_back ((Byte) (~take & 0xFF));
        outBytes.push_back ((Byte) ((~take >> 8) & 0xFF));
        outBytes.insert (outBytes.end(), data.begin() + (ptrdiff_t) at, data.begin() + (ptrdiff_t) (at + take));

        at += take;
    }
    while (at < data.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::CompressFixed
//
//  One final block of fixed-Huffman codes. At each position the hash chain
//  of its first three bytes is searched for the longest earlier match in the
//  window; a match of three bytes or more is written as a length and a
//  distance, and anything shorter as a literal. Every position is added to
//  its chain, matched or not.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::CompressFixed (std::span<const Byte> data)
{
    constexpr uint32_t  kFinalFixed = 0x3;      // BFINAL, then BTYPE 01



    std::vector<int32_t>  head (size_t (1) << kHashBits, -1);
    std::vector<int32_t>  prev (kWindow, -1);
    size_t                at        = 0;
    size_t                best      = 0;
    size_t                distance  = 0;
    size_t                length    = 0;
    size_t                k         = 0;
    int32_t               candidate = -1;
    int                   chain     = 0;



    auto  insert = [&head, &prev, data] (size_t position)
    {
        if (position + kMinMatch <= data.size())
        {
            uint32_t  hash = Hash (data, position);

            prev[position % kWindow] = head[hash];
            head[hash]               = (int32_t) position;
        }
    };

    PutBits (kFinalFixed, 3);

    while (at < data.size())
    {
        best     = 0;
        distance = 0;

        if (at + kMinMatch <= data.size())
        {
            candidate = head[Hash (data, at)];
            chain     = 0;

            while (candidate >= 0 && (size_t) candidate < at && at - (size_t) candidate <= kWindow && chain++ < kMaxChain && best < kMaxMatch)
            {
                length = MatchLength (data, (size_t) candidate, at);

                if (length > best)
                {
                    best     = length;
                    distance = at - (size_t) candidate;
                }

                candidate = prev[(size_t) candidate % kWindow];
            }
        }

        if (best >= kMinMatch)
        {
            PutMatch (best, distance);

            for (k = 0; k < best; k++)
            {
                insert (at + k);
            }

            at += best;
        }
        else
        {
            PutLiteral (data[at]);
            insert (at);
            at++;
        }
    }

    PutLiteral (kEndOfBlock);
    Flush();
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
//  A Huffman code goes out from its highest bit, so its bits are reversed
//  before they are packed.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutCode (uint32_t code, int length)
{
    uint32_t  reversed = 0;
    int       i        = 0;



    for (i = 0; i < length; i++)
    {
        reversed = (reversed << 1) | ((code >> i) & 1);
    }

    PutBits (reversed, length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutLiteral
//
//  The fixed literal and length codes of RFC 1951 section 3.2.6.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutLiteral (int symbol)
{
    if (symbol < 144)
    {
        PutCode (0x30 + (uint32_t) symbol, 8);
    }
    else if (symbol < 256)
    {
        PutCode (0x190 + (uint32_t) (symbol - 144), 9);
    }
    else if (symbol < 280)
    {
        PutCode ((uint32_t) (symbol - 256), 7);
    }
    else
    {
        PutCode (0xC0 + (uint32_t) (symbol - 280), 8);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::PutMatch
//
//  The length's code and extra bits, then the distance's five-bit code and
//  extra bits.
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::PutMatch (size_t length, size_t distance)
{
    size_t  lengthCode   = std::size (s_kLengthBase) - 1;
    size_t  distanceCode = std::size (s_kDistanceBase) - 1;



    while (s_kLengthBase[lengthCode] > length)
    {
        lengthCode--;
    }

    while (s_kDistanceBase[distanceCode] > distance)
    {
        distanceCode--;
    }

    PutLiteral (kEndOfBlock + 1 + (int) lengthCode);
    PutBits ((uint32_t) (length - s_kLengthBase[lengthCode]), s_kLengthExtraBits[lengthCode]);
    PutCode ((uint32_t) distanceCode, 5);
    PutBits ((uint32_t) (distance - s_kDistanceBase[distanceCode]), s_kDistanceExtraBits[distanceCode]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deflate::Flush
//
////////////////////////////////////////////////////////////////////////////////

void Deflate::Flush()
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
