#include "Pch.h"

#include "CassoLink/Session/CaptureMarks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks::EncodeBits
//
////////////////////////////////////////////////////////////////////////////////

void CaptureMarks::EncodeBits (std::span<const Byte> stored, std::vector<Byte> & ioOut)
{
    for (size_t at = 0; at < stored.size(); at += s_kMarksPerByte)
    {
        Byte  bits = 0;

        for (size_t bit = 0; bit < s_kMarksPerByte; bit++)
        {
            bool  isSet = at + bit < stored.size() && (stored[at + bit] & s_kTopBit) != 0;

            bits = (Byte) ((bits << 1) | (isSet ? 1 : 0));
        }

        ioOut.push_back (bits);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks::EncodeRuns
//
////////////////////////////////////////////////////////////////////////////////

void CaptureMarks::EncodeRuns (std::span<const Byte> stored, std::vector<Byte> & ioOut)
{
    bool    marked = false;
    size_t  run    = 0;



    for (Byte b : stored)
    {
        bool  isMarked = (b & s_kTopBit) == 0;

        if (isMarked != marked)
        {
            ioOut.push_back ((Byte) run);
            marked = isMarked;
            run    = 0;
        }

        run++;

        if (run == s_kLongRun)
        {
            ioOut.push_back (s_kLongRun);
            run = 0;
        }
    }

    ioOut.push_back ((Byte) run);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks::Apply
//
////////////////////////////////////////////////////////////////////////////////

bool CaptureMarks::Apply (std::span<const Byte> encoded, Byte moduleVersion, size_t & ioAt, std::vector<Byte> & ioBytes)
{
    Byte  encoding = kBits;
    bool  valid    = true;



    if (moduleVersion >= kRunsVersion)
    {
        valid    = ioAt < encoded.size();
        encoding = valid ? encoded[ioAt++] : kBits;
    }

    if (valid && encoding == kBits)
    {
        valid = ApplyBits (encoded, ioAt, ioBytes);
    }
    else if (valid && encoding == kRuns)
    {
        valid = ApplyRuns (encoded, ioAt, ioBytes);
    }
    else
    {
        valid = false;
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks::ApplyBits
//
////////////////////////////////////////////////////////////////////////////////

bool CaptureMarks::ApplyBits (std::span<const Byte> encoded, size_t & ioAt, std::vector<Byte> & ioBytes)
{
    size_t  marks = (ioBytes.size() + s_kMarksPerByte - 1) / s_kMarksPerByte;
    bool    valid = ioAt + marks <= encoded.size();



    for (size_t each = 0; valid && each < ioBytes.size(); each++)
    {
        bool  isMarked = ((encoded[ioAt + each / s_kMarksPerByte] << (each % s_kMarksPerByte)) & s_kTopBit) == 0;

        ioBytes[each] = isMarked ? (Byte) (ioBytes[each] & ~s_kTopBit) : ioBytes[each];
    }

    ioAt += valid ? marks : 0;

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks::ApplyRuns
//
////////////////////////////////////////////////////////////////////////////////

bool CaptureMarks::ApplyRuns (std::span<const Byte> encoded, size_t & ioAt, std::vector<Byte> & ioBytes)
{
    size_t  done   = 0;
    bool    marked = false;
    bool    valid  = true;



    while (valid && done < ioBytes.size())
    {
        Byte    run  = 0;
        size_t  last = 0;

        valid = ioAt < encoded.size();

        if (!valid)
        {
            break;
        }

        run  = encoded[ioAt++];
        last = std::min (ioBytes.size(), done + run);

        for (; marked && done < last; done++)
        {
            ioBytes[done] = (Byte) (ioBytes[done] & ~s_kTopBit);
        }

        done   = last;
        marked = run == s_kLongRun ? marked : !marked;
    }

    return valid;
}
