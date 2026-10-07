#include "Pch.h"

#include "Debugger/HeatCountDelta.h"
#include "Debugger/Reverse/SnapshotCompressor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::Encode
//
//  Each run of changed entries, taking in gaps of up to kMaxBridge unchanged
//  ones, which cost less inside a segment than a segment of their own.
//
////////////////////////////////////////////////////////////////////////////////

void HeatCountDelta::Encode (
    const int64_t      * later,
    const int64_t      * earlier,
    size_t               count,
    std::vector<Byte>  & outBytes)
{
    size_t  end    = 0;
    size_t  i      = 0;
    size_t  runEnd = 0;
    size_t  gapEnd = 0;



    outBytes.clear();

    while (i < count)
    {
        if (GetIncrease (later, earlier, i) == 0)
        {
            i++;
            continue;
        }

        runEnd = i + 1;

        while (runEnd < count)
        {
            gapEnd = runEnd;

            while (gapEnd < count && gapEnd - runEnd < kMaxBridge && GetIncrease (later, earlier, gapEnd) == 0)
            {
                gapEnd++;
            }

            if (gapEnd >= count || GetIncrease (later, earlier, gapEnd) == 0)
            {
                break;
            }

            runEnd = gapEnd + 1;
        }

        AppendRun (later, earlier, i, runEnd, end, outBytes);

        i = runEnd;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::AppendRun
//
//  One run of changed entries as segments: every stretch of at least
//  kMinUniform equal counts as a uniform segment, and what lies between them
//  as varied ones.
//
////////////////////////////////////////////////////////////////////////////////

void HeatCountDelta::AppendRun (
    const int64_t      * later,
    const int64_t      * earlier,
    size_t               start,
    size_t               end,
    size_t             & ioEnd,
    std::vector<Byte>  & outBytes)
{
    size_t    pieceStart = start;
    size_t    j          = start;
    size_t    k          = 0;
    uint64_t  value      = 0;



    while (j < end)
    {
        value = GetIncrease (later, earlier, j);
        k     = j + 1;

        while (k < end && GetIncrease (later, earlier, k) == value)
        {
            k++;
        }

        if (k - j >= kMinUniform)
        {
            if (j > pieceStart)
            {
                AppendSegment (later, earlier, pieceStart, j, false, ioEnd, outBytes);
            }

            AppendSegment (later, earlier, j, k, true, ioEnd, outBytes);

            pieceStart = k;
        }

        j = k;
    }

    if (pieceStart < end)
    {
        AppendSegment (later, earlier, pieceStart, end, false, ioEnd, outBytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::AppendSegment
//
//  The gap since the last segment ended, the length less one shifted left
//  with the uniform flag in the low bit, then the counts.
//
////////////////////////////////////////////////////////////////////////////////

void HeatCountDelta::AppendSegment (
    const int64_t      * later,
    const int64_t      * earlier,
    size_t               start,
    size_t               end,
    bool                 isUniform,
    size_t             & ioEnd,
    std::vector<Byte>  & outBytes)
{
    size_t  length = end - start;
    size_t  i      = 0;



    AppendNumber (start - ioEnd, outBytes);
    AppendNumber (((uint64_t) (length - 1) << 1) | (isUniform ? 1u : 0u), outBytes);

    if (isUniform)
    {
        AppendNumber (GetIncrease (later, earlier, start), outBytes);
    }
    else
    {
        for (i = start; i < end; i++)
        {
            AppendNumber (GetIncrease (later, earlier, i), outBytes);
        }
    }

    ioEnd = end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::TryApply
//
////////////////////////////////////////////////////////////////////////////////

bool HeatCountDelta::TryApply (
    const Byte  * bytes,
    size_t        size,
    bool          isSubtracting,
    int64_t     * totals,
    size_t        count)
{
    size_t    at        = 0;
    size_t    end       = 0;
    size_t    start     = 0;
    size_t    length    = 0;
    size_t    i         = 0;
    uint64_t  gap       = 0;
    uint64_t  header    = 0;
    uint64_t  value     = 0;
    bool      isUniform = false;
    bool      isRead    = true;



    while (isRead && at < size)
    {
        isRead = TryReadNumber (bytes, size, at, gap) && TryReadNumber (bytes, size, at, header);

        if (!isRead)
        {
            break;
        }

        isUniform = (header & 1) != 0;
        length    = (size_t) (header >> 1) + 1;
        isRead    = gap <= count - end && length <= count - end - (size_t) gap;

        if (!isRead)
        {
            break;
        }

        start = end + (size_t) gap;

        for (i = start; i < start + length; i++)
        {
            if (!isUniform || i == start)
            {
                isRead = TryReadNumber (bytes, size, at, value);
            }

            if (!isRead)
            {
                break;
            }

            totals[i] += isSubtracting ? -(int64_t) value : (int64_t) value;
        }

        end = start + length;
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::TryGetCount
//
//  The segments are walked until one reaches the entry, or passes it.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatCountDelta::TryGetCount (
    const Byte  * bytes,
    size_t        size,
    size_t        index,
    uint64_t    & outCount)
{
    size_t    at        = 0;
    size_t    end       = 0;
    size_t    start     = 0;
    size_t    length    = 0;
    uint64_t  gap       = 0;
    uint64_t  header    = 0;
    uint64_t  value     = 0;
    bool      isUniform = false;
    bool      isRead    = true;



    outCount = 0;

    while (isRead && at < size)
    {
        isRead = TryReadNumber (bytes, size, at, gap) && TryReadNumber (bytes, size, at, header);

        if (!isRead)
        {
            break;
        }

        isUniform = (header & 1) != 0;
        length    = (size_t) (header >> 1) + 1;
        start     = end + (size_t) gap;

        if (start > index)
        {
            return true;
        }

        for (size_t i = start; isRead && i < start + length; i++)
        {
            if (!isUniform || i == start)
            {
                isRead = TryReadNumber (bytes, size, at, value);
            }

            if (i == index)
            {
                outCount = isRead ? value : 0;
                return isRead;
            }
        }

        end = start + length;
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::Pack
//
//  The header, then the delta packed when packing makes it smaller, else
//  the delta itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatCountDelta::Pack (
    const std::vector<Byte>  & delta,
    SnapshotCompressor       & compressor,
    std::vector<Byte>        & outBytes)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  packed;
    bool               isPacked = false;



    outBytes.clear();

    BAIL_OUT_IF (delta.empty(), S_OK);

    hr = compressor.Compress (delta.data(), delta.size(), packed);
    CHR (hr);

    isPacked = packed.size() < delta.size();

    AppendNumber (((uint64_t) delta.size() << 1) | (isPacked ? 1u : 0u), outBytes);

    if (isPacked)
    {
        outBytes.insert (outBytes.end(), packed.begin(), packed.end());
    }
    else
    {
        outBytes.insert (outBytes.end(), delta.begin(), delta.end());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::Unpack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatCountDelta::Unpack (
    const Byte          * bytes,
    size_t                size,
    SnapshotCompressor  & compressor,
    std::vector<Byte>   & outDelta)
{
    constexpr uint64_t  kLargestDelta = 1ull << 32;
    HRESULT             hr            = S_OK;
    size_t              at            = 0;
    uint64_t            header        = 0;
    uint64_t            length        = 0;
    bool                isRead        = false;
    bool                isPacked      = false;
    bool                isComplete    = false;



    outDelta.clear();

    BAIL_OUT_IF (size == 0, S_OK);

    isRead = TryReadNumber (bytes, size, at, header);
    CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    length   = header >> 1;
    isPacked = (header & 1) != 0;

    CBREx (length < kLargestDelta, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (isPacked)
    {
        hr = compressor.Decompress (bytes + at, size - at, (size_t) length, outDelta);
        CHR (hr);
    }
    else
    {
        isComplete = size - at == length;
        CBREx (isComplete, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outDelta.assign (bytes + at, bytes + size);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::AppendNumber
//
//  Seven bits a byte, low first, the top bit set on every byte but the last.
//
////////////////////////////////////////////////////////////////////////////////

void HeatCountDelta::AppendNumber (
    uint64_t             value,
    std::vector<Byte>  & outBytes)
{
    constexpr uint64_t  kLowBits = 0x7F;
    constexpr Byte      kMore    = 0x80;
    constexpr int       kShift   = 7;



    while (value > kLowBits)
    {
        outBytes.push_back ((Byte) ((value & kLowBits) | kMore));
        value >>= kShift;
    }

    outBytes.push_back ((Byte) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::TryReadNumber
//
////////////////////////////////////////////////////////////////////////////////

bool HeatCountDelta::TryReadNumber (
    const Byte  * bytes,
    size_t        size,
    size_t      & ioAt,
    uint64_t    & outValue)
{
    constexpr Byte  kLowBits  = 0x7F;
    constexpr Byte  kMore     = 0x80;
    constexpr int   kShift    = 7;
    constexpr int   kMaxShift = 63;
    int             shift     = 0;
    bool            isMore    = true;



    outValue = 0;

    while (isMore && ioAt < size && shift <= kMaxShift)
    {
        outValue |= (uint64_t) (bytes[ioAt] & kLowBits) << shift;
        isMore    = (bytes[ioAt] & kMore) != 0;
        shift    += kShift;
        ioAt++;
    }

    return !isMore;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta::GetIncrease
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HeatCountDelta::GetIncrease (
    const int64_t  * later,
    const int64_t  * earlier,
    size_t           index)
{
    int64_t  increase = later[index] - earlier[index];



    return (increase > 0) ? (uint64_t) increase : 0;
}





