#include "Pch.h"

#include "Devices/Disk/Archives/SqueezeDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SqueezeDecoder::Expand
//
//  The tree's codes until the end code, then the runs; output past
//  `outLength` is dropped, since a thread's length is its true length.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SqueezeDecoder::Expand (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError)
{
    HRESULT                               hr       = S_OK;
    size_t                                nodes    = 0;
    size_t                                at       = 2;
    uint64_t                              bit      = 0;
    uint64_t                              bitLimit = 0;
    int                                   node     = 0;
    bool                                  done     = false;
    bool                                  sized    = false;
    bool                                  fits     = false;
    std::vector<std::array<int16_t, 2>>   tree;
    std::vector<Byte>                     coded;
    Byte                                  last     = 0;
    bool                                  inRun    = false;



    outBytes.clear();

    if (in.size() >= 4 && in[0] == kMagic1 && in[1] == kMagic2)
    {
        size_t  nameEnd = 4;

        while (nameEnd < in.size() && in[nameEnd] != 0)
        {
            nameEnd++;
        }

        in = in.subspan (std::min (nameEnd + 1, in.size()));
    }

    if (in.size() < 2)
    {
        BAIL_OUT_IF (true, S_OK);
    }

    nodes = (size_t) (in[0] | (in[1] << 8));
    sized = nodes <= kMaxNodes && at + nodes * 4 <= in.size();
    CBRFEx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The Squeeze tree is larger than its data.");

    for (size_t i = 0; i < nodes; i++, at += 4)
    {
        tree.push_back ({ (int16_t) (in[at] | (in[at + 1] << 8)), (int16_t) (in[at + 2] | (in[at + 3] << 8)) });
    }

    bit      = (uint64_t) at * 8;
    bitLimit = (uint64_t) in.size() * 8;

    while (!done && nodes > 0)
    {
        int  child = 0;

        fits = bit < bitLimit;
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The Squeeze data ends before its end code.");

        child = tree[(size_t) node][(in[(size_t) (bit / 8)] >> (bit % 8)) & 1];
        bit++;

        if (child >= 0)
        {
            fits = (size_t) child < nodes;
            CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The Squeeze tree points past its last node.");

            node = child;
            continue;
        }

        if (-(child + 1) == kEndCode)
        {
            done = true;
            continue;
        }

        coded.push_back ((Byte) -(child + 1));
        node = 0;
    }

    for (Byte b : coded)
    {
        if (inRun)
        {
            inRun = false;

            if (b == 0)
            {
                outBytes.push_back (kRunEscape);
                last = kRunEscape;
            }
            else
            {
                outBytes.insert (outBytes.end(), (size_t) b - 1, last);
            }

            continue;
        }

        if (b == kRunEscape)
        {
            inRun = true;
            continue;
        }

        outBytes.push_back (b);
        last = b;
    }

    if (outBytes.size() > outLength)
    {
        outBytes.resize (outLength);
    }

Error:
    return hr;
}
