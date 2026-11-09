#include "Pch.h"

#include "Devices/Disk/Archives/LzcDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LzcDecoder::RoundUpToGroup
//
//  The bit after the group of eight codes the bit is in, at the width those
//  codes have; a bit at a group's start stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t LzcDecoder::RoundUpToGroup (uint64_t bit, int width)
{
    uint64_t  group = (uint64_t) width * 8;



    return (bit + group - 1) / group * group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LzcDecoder::Expand
//
//  A code equal to the next free entry is the previous string followed by
//  that string's first byte; a code past it is damage. After a clear, the next
//  free entry is one below the first, so the next code's entry lands on the
//  clear code and is never read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LzcDecoder::Expand (std::span<const Byte> in, int maxBits, size_t outLength, std::vector<Byte> & outBytes, std::string & outError)
{
    HRESULT             hr        = S_OK;
    size_t              start     = 0;
    bool                block     = true;
    bool                bitsOk    = false;
    int                 width     = kInitialBits;
    int                 maxCode   = (1 << kInitialBits) - 1;
    int                 maxMax    = 0;
    int                 freeEntry = kFirstCode;
    int                 oldCode   = -1;
    Byte                finChar   = 0;
    uint64_t            bit       = 0;
    uint64_t            bitLimit  = 0;
    std::vector<Word>   prefix;
    std::vector<Byte>   suffix;
    std::vector<Byte>   stack;



    outBytes.clear();

    if (in.size() >= 3 && in[0] == kMagic1 && in[1] == kMagic2)
    {
        maxBits = in[2] & kBitsMask;
        block   = (in[2] & kBlockFlag) != 0;
        start   = 3;
    }

    bitsOk = maxBits >= kMinMaxBits && maxBits <= kMaxMaxBits;
    CBRFEx (bitsOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format ("LZC data of {} bits isn't one this reader expands.", maxBits));

    maxMax    = 1 << maxBits;
    freeEntry = block ? kFirstCode : kClearCode;
    bit       = (uint64_t) start * 8;
    bitLimit  = (uint64_t) in.size() * 8;

    prefix.assign ((size_t) maxMax, 0);
    suffix.assign ((size_t) maxMax, 0);

    for (int i = 0; i < kClearCode; i++)
    {
        suffix[(size_t) i] = (Byte) i;
    }

    while (outBytes.size() < outLength)
    {
        int  code   = 0;
        int  inCode = 0;

        if (freeEntry > maxCode)
        {
            bit     = start * 8 + RoundUpToGroup (bit - start * 8, width);
            width++;
            maxCode = (width == maxBits) ? maxMax : (1 << width) - 1;
        }

        if (bit + (uint64_t) width > bitLimit)
        {
            break;
        }

        for (int b = 0; b < width; b++, bit++)
        {
            code |= ((in[(size_t) (bit / 8)] >> (bit % 8)) & 1) << b;
        }

        if (oldCode == -1)
        {
            CBRFEx (code < kClearCode, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "LZC data starts with a code that isn't a byte.");

            finChar = (Byte) code;
            oldCode = code;
            outBytes.push_back (finChar);
            continue;
        }

        if (block && code == kClearCode)
        {
            bit       = start * 8 + RoundUpToGroup (bit - start * 8, width);
            width     = kInitialBits;
            maxCode   = (1 << kInitialBits) - 1;
            freeEntry = kFirstCode - 1;
            continue;
        }

        inCode = code;
        stack.clear();

        CBRFEx (code <= freeEntry, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "LZC data has a code its table doesn't define.");

        if (code == freeEntry)
        {
            stack.push_back (finChar);
            code = oldCode;
        }

        while (code >= kClearCode)
        {
            stack.push_back (suffix[(size_t) code]);
            code = prefix[(size_t) code];
        }

        finChar = suffix[(size_t) code];
        stack.push_back (finChar);
        outBytes.insert (outBytes.end(), stack.rbegin(), stack.rend());

        if (freeEntry < maxMax)
        {
            prefix[(size_t) freeEntry] = (Word) oldCode;
            suffix[(size_t) freeEntry] = finChar;
            freeEntry++;
        }

        oldCode = inCode;
    }

    if (outBytes.size() > outLength)
    {
        outBytes.resize (outLength);
    }

Error:
    return hr;
}
