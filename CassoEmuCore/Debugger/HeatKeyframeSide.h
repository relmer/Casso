#pragma once

#include "Pch.h"

class SnapshotCompressor;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatKeyframeSide
//
//  What the heat map keeps beside a keyframe: the counts made over the
//  stretch before it, packed (HeatCountDelta), and which RAM had been written
//  as of it (AccessHeatMap's written bits). The side starts with the size of
//  the counts, as a number seven bits a byte, then the counts, then the
//  bits, packed with the snapshots' compressor when that saves anything, or
//  nothing at all where they are the same as at the keyframe before, which
//  after a program has settled is nearly every keyframe. A side with neither
//  is empty: a keyframe taken while the map was off.
//
////////////////////////////////////////////////////////////////////////////////

class HeatKeyframeSide
{
public:
    //  A part of a side, as a pointer into it and a size.
    struct Part
    {
        const Byte  * bytes = nullptr;
        size_t        size  = 0;
    };

    //  The side for packed counts, which may be empty, and the bits, or null
    //  for bits unchanged since the keyframe before.
    static HRESULT  Make          (const std::vector<Byte>      & counts,
                                   const std::vector<uint64_t>  * written,
                                   SnapshotCompressor           & compressor,
                                   std::vector<Byte>            & outSide);

    //  The counts and the bits of a side; false for bytes that are not one.
    //  An empty side splits into two empty parts.
    static bool     TrySplit      (const Byte * side, size_t size, Part & outCounts, Part & outWritten);

    //  The bits a non-empty part holds.
    static HRESULT  UnpackWritten (const Part & written, SnapshotCompressor & compressor, std::vector<uint64_t> & outBits);
};
