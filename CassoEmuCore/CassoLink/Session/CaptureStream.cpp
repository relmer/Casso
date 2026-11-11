#include "Pch.h"

#include "CassoLink/Session/CaptureStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureStream::FindRevolution
//
////////////////////////////////////////////////////////////////////////////////

bool CaptureStream::FindRevolution (std::span<const Byte> bytes, size_t & outLength)
{
    outLength = 0;

    for (size_t length = kMinRevolutionBytes; length + kMatchBytes <= bytes.size(); length++)
    {
        size_t  at         = 0;
        size_t  mismatches = 0;
        size_t  overlap    = bytes.size() - length;

        while (at < kMatchBytes && (bytes[at] | s_kTopBit) == (bytes[at + length] | s_kTopBit))
        {
            at++;
        }

        if (at < kMatchBytes)
        {
            continue;
        }

        for (; at < overlap; at++)
        {
            mismatches += ((bytes[at] | s_kTopBit) != (bytes[at + length] | s_kTopBit)) ? 1 : 0;
        }

        if (mismatches * s_kMismatchShare <= overlap)
        {
            outLength = length;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureStream::BuildRevolution
//
////////////////////////////////////////////////////////////////////////////////

void CaptureStream::BuildRevolution (std::span<const Byte> bytes, size_t length, std::vector<uint8_t> & outBits)
{
    outBits.clear();

    for (size_t at = 0; at < length && at < bytes.size(); at++)
    {
        Byte  value = bytes[at] | s_kTopBit;
        bool  sync  = IsSync (bytes[at]) && (at + length >= bytes.size() || IsSync (bytes[at + length]));

        for (int bit = 7; bit >= 0; bit--)
        {
            outBits.push_back ((uint8_t) ((value >> bit) & 1));
        }

        if (sync)
        {
            outBits.push_back (0);
            outBits.push_back (0);
        }
    }
}
