#pragma once

#include "Pch.h"

class SnapshotCompressor;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDelta
//
//  The heat map's access counts made over one stretch of history, packed
//  small enough to keep with every keyframe: only the entries that changed,
//  in index order, as segments of consecutive entries. Each segment is the
//  gap since the last one ended and its length, then either one count that
//  every entry in it shares or one count per entry, all as variable-length
//  integers of seven bits a byte. An idle machine touches a few hundred
//  addresses a stretch, and a program that fills the screen writes long runs
//  of addresses the same number of times, so both pack to little.
//
//  As history keeps it, a delta is packed once more with the snapshots'
//  compressor (Pack), which takes about a quarter off a busy program's, and
//  kept as it is where that saves nothing.
//
////////////////////////////////////////////////////////////////////////////////

class HeatCountDelta
{
public:
    //  A run of at least this many equal counts becomes a segment of its own.
    static constexpr size_t  kMinUniform = 4;

    //  Untouched entries a segment of varied counts takes in rather than end.
    static constexpr size_t  kMaxBridge  = 2;

    //  How much later exceeds earlier at each of count entries; an entry
    //  where it does not is taken as unchanged.
    static void  Encode   (const int64_t * later, const int64_t * earlier, size_t count, std::vector<Byte> & outBytes);

    //  Adds the counts the bytes hold to totals, or takes them away when
    //  isSubtracting; false, with totals partly changed, when the bytes are
    //  not a delta of count entries.
    static bool  TryApply (const Byte * bytes, size_t size, bool isSubtracting, int64_t * totals, size_t count);

    //  The count the bytes hold for one entry, zero where they hold none;
    //  false when the bytes are not a delta reaching that far.
    static bool  TryGetCount (const Byte * bytes, size_t size, size_t index, uint64_t & outCount);

    //  A delta as history keeps it: its size, shifted left with a flag in
    //  the low bit for whether what follows is packed, then the bytes; and
    //  back. Unpack fails on bytes that are not a packed delta.
    static HRESULT  Pack   (const std::vector<Byte> & delta, SnapshotCompressor & compressor, std::vector<Byte> & outBytes);
    static HRESULT  Unpack (const Byte * bytes, size_t size, SnapshotCompressor & compressor, std::vector<Byte> & outDelta);

    //  A number seven bits a byte, low first, and back.
    static void  AppendNumber   (uint64_t value, std::vector<Byte> & outBytes);
    static bool  TryReadNumber  (const Byte * bytes, size_t size, size_t & ioAt, uint64_t & outValue);

private:
    static void  AppendSegment  (const int64_t * later, const int64_t * earlier, size_t start, size_t end, bool isUniform, size_t & ioEnd, std::vector<Byte> & outBytes);
    static void  AppendRun      (const int64_t * later, const int64_t * earlier, size_t start, size_t end, size_t & ioEnd, std::vector<Byte> & outBytes);

    static uint64_t  GetIncrease (const int64_t * later, const int64_t * earlier, size_t index);
};
