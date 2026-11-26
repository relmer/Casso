#include "Pch.h"

#include "CassoLink/Session/RawWriteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FromBits
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::FromBits (std::span<const uint8_t> bits, RawWriteStream & outStream)
{
    size_t  at = 0;



    outStream.values.clear();
    outStream.zeros.clear();

    while (at < bits.size())
    {
        Byte    value = 0;
        size_t  zeros = 0;

        for (size_t i = 0; i < kBitsPerByte; i++)
        {
            value = (Byte) ((value << 1) | ((at < bits.size() && bits[at] != 0) ? 1 : 0));
            at++;
        }

        while (at < bits.size() && bits[at] == 0 && zeros < kMaxZeros)
        {
            zeros++;
            at++;
        }

        outStream.values.push_back (value);
        outStream.zeros.push_back ((Byte) zeros);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FromNibbles
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::FromNibbles (std::span<const Byte> nibbles, RawWriteStream & outStream)
{
    size_t  length = FindRevolution (nibbles);
    size_t  at     = 0;



    outStream.values.assign (nibbles.begin(), nibbles.begin() + length);
    outStream.zeros.assign  (length, 0);

    while (at < length)
    {
        size_t  run = 0;

        while (at + run < length && nibbles[at + run] == s_kSyncValue)
        {
            run++;
        }

        if (run >= kSyncRun)
        {
            std::fill_n (outStream.zeros.begin() + at, run, (Byte) kSyncZeros);
        }

        at += std::max<size_t> (run, 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FindRevolution
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::FindRevolution (std::span<const Byte> nibbles)
{
    for (size_t length = s_kMinRevolution; length + s_kMatchBytes <= nibbles.size(); length++)
    {
        if (std::equal (nibbles.begin(), nibbles.begin() + s_kMatchBytes, nibbles.begin() + length))
        {
            return length;
        }
    }

    return nibbles.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::GetCellCount
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::GetCellCount() const
{
    size_t  cells = values.size() * kBitsPerByte;



    for (Byte count : zeros)
    {
        cells += count;
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::ToBits
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::ToBits (std::vector<uint8_t> & outBits) const
{
    outBits.clear();

    for (size_t i = 0; i < values.size(); i++)
    {
        for (size_t bit = 0; bit < kBitsPerByte; bit++)
        {
            outBits.push_back ((uint8_t) ((values[i] >> (kBitsPerByte - 1 - bit)) & 1));
        }

        outBits.insert (outBits.end(), zeros[i], 0);
    }
}
