#include "Pch.h"

#include "Sha1.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::RotateLeft
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Sha1::RotateLeft (uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::ProcessBlock
//
//  One 512-bit block: the message schedule of eighty words, then eighty rounds
//  in four groups of twenty, each group with its own function and constant.
//
////////////////////////////////////////////////////////////////////////////////

void Sha1::ProcessBlock (const uint8_t * block, State & state)
{
    static constexpr uint32_t  kRoundConstant[4] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    std::array<uint32_t, 80>   w                 = {};
    uint32_t                   a                 = state[0];
    uint32_t                   b                 = state[1];
    uint32_t                   c                 = state[2];
    uint32_t                   d                 = state[3];
    uint32_t                   e                 = state[4];



    for (size_t t = 0; t < 16; t++)
    {
        w[t] = ((uint32_t) block[t * 4]     << 24) | ((uint32_t) block[t * 4 + 1] << 16) |
               ((uint32_t) block[t * 4 + 2] <<  8) |  (uint32_t) block[t * 4 + 3];
    }

    for (size_t t = 16; t < 80; t++)
    {
        w[t] = RotateLeft (w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }

    for (size_t t = 0; t < 80; t++)
    {
        uint32_t  f    = 0;
        uint32_t  temp = 0;



        if      (t < 20) { f = (b & c) | (~b & d);          }
        else if (t < 40) { f = b ^ c ^ d;                   }
        else if (t < 60) { f = (b & c) | (b & d) | (c & d); }
        else             { f = b ^ c ^ d;                   }

        temp = RotateLeft (a, 5) + f + e + w[t] + kRoundConstant[t / 20];
        e    = d;
        d    = c;
        c    = RotateLeft (b, 30);
        b    = a;
        a    = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::Compute
//
//  Whole blocks straight from the input, then the tail padded: a 1 bit, zeros,
//  and the message length in bits as a 64-bit big-endian number, which takes a
//  second block when fewer than eight bytes are left in the first.
//
////////////////////////////////////////////////////////////////////////////////

Sha1::Digest Sha1::Compute (std::span<const uint8_t> bytes)
{
    static constexpr uint8_t              kPadMarker   = 0x80;
    static constexpr size_t               kLengthBytes = 8;
    State                                 state        = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    size_t                                whole        = (bytes.size() / kBlockBytes) * kBlockBytes;
    uint64_t                              bitLength    = (uint64_t) bytes.size() * 8;
    std::array<uint8_t, kBlockBytes * 2>  tail         = {};
    size_t                                tailLength   = bytes.size() - whole;
    size_t                                tailBlocks   = (tailLength + 1 + kLengthBytes > kBlockBytes) ? 2 : 1;
    Digest                                digest       = {};



    for (size_t at = 0; at < whole; at += kBlockBytes)
    {
        ProcessBlock (bytes.data() + at, state);
    }

    std::copy (bytes.begin() + (ptrdiff_t) whole, bytes.end(), tail.begin());
    tail[tailLength] = kPadMarker;

    for (size_t i = 0; i < kLengthBytes; i++)
    {
        tail[tailBlocks * kBlockBytes - 1 - i] = (uint8_t) (bitLength >> (i * 8));
    }

    for (size_t block = 0; block < tailBlocks; block++)
    {
        ProcessBlock (tail.data() + block * kBlockBytes, state);
    }

    for (size_t i = 0; i < kDigestBytes; i++)
    {
        digest[i] = (uint8_t) (state[i / 4] >> (24 - (i % 4) * 8));
    }

    return digest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::ToHex
//
////////////////////////////////////////////////////////////////////////////////

std::string Sha1::ToHex (const Digest & digest)
{
    static constexpr char  kDigits[] = "0123456789abcdef";
    std::string            text;



    for (uint8_t value : digest)
    {
        text += kDigits[value >> 4];
        text += kDigits[value & 0x0F];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::NormalizeLineEndings
//
////////////////////////////////////////////////////////////////////////////////

std::string Sha1::NormalizeLineEndings (std::string_view text)
{
    std::string  result;



    result.reserve (text.size());

    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] != '\r')
        {
            result += text[i];
            continue;
        }

        result += '\n';

        if (i + 1 < text.size() && text[i + 1] == '\n')
        {
            i++;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1::ComputeTextHex
//
////////////////////////////////////////////////////////////////////////////////

std::string Sha1::ComputeTextHex (std::string_view text)
{
    std::string  normalized = NormalizeLineEndings (text);



    return ToHex (Compute (std::span<const uint8_t> ((const uint8_t *) normalized.data(), normalized.size())));
}
