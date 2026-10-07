#include "Pch.h"

#include "Debugger/HeatKeyframeSide.h"

#include "Debugger/HeatCountDelta.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKeyframeSide::Make
//
//  The bits are packed as a delta is, size and flag first, since a delta's
//  packing is any bytes'.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatKeyframeSide::Make (
    const std::vector<Byte>      & counts,
    const std::vector<uint64_t>  * written,
    SnapshotCompressor           & compressor,
    std::vector<Byte>            & outSide)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  raw;
    std::vector<Byte>  packed;



    outSide.clear();

    HeatCountDelta::AppendNumber (counts.size(), outSide);
    outSide.insert (outSide.end(), counts.begin(), counts.end());

    BAIL_OUT_IF (written == nullptr, S_OK);

    raw.resize (written->size() * sizeof (uint64_t));
    memcpy (raw.data(), written->data(), raw.size());

    hr = HeatCountDelta::Pack (raw, compressor, packed);
    CHR (hr);

    outSide.insert (outSide.end(), packed.begin(), packed.end());

Error:
    if (FAILED (hr))
    {
        outSide.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKeyframeSide::TrySplit
//
////////////////////////////////////////////////////////////////////////////////

bool HeatKeyframeSide::TrySplit (
    const Byte  * side,
    size_t        size,
    Part        & outCounts,
    Part        & outWritten)
{
    size_t    at     = 0;
    uint64_t  counts = 0;



    outCounts  = Part();
    outWritten = Part();

    if (side == nullptr || size == 0)
    {
        return true;
    }

    if (!HeatCountDelta::TryReadNumber (side, size, at, counts) || counts > size - at)
    {
        return false;
    }

    outCounts  = Part { side + at, (size_t) counts };
    at        += (size_t) counts;
    outWritten = Part { side + at, size - at };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKeyframeSide::UnpackWritten
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatKeyframeSide::UnpackWritten (
    const Part             & written,
    SnapshotCompressor     & compressor,
    std::vector<uint64_t>  & outBits)
{
    HRESULT            hr          = S_OK;
    std::vector<Byte>  raw;
    bool               isWholeWord = false;



    outBits.clear();

    hr = HeatCountDelta::Unpack (written.bytes, written.size, compressor, raw);
    CHR (hr);

    isWholeWord = !raw.empty() && raw.size() % sizeof (uint64_t) == 0;
    CBREx (isWholeWord, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outBits.resize (raw.size() / sizeof (uint64_t));
    memcpy (outBits.data(), raw.data(), raw.size());

Error:
    return hr;
}





