#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkSpan / WozChunkWalker
//
//  A WOZ file's chunks, from the header to the first id that isn't a chunk's,
//  which in a version 2 file is the track data after TRKS. A span is a chunk's
//  header offset, and its payload's offset and size.
//
////////////////////////////////////////////////////////////////////////////////

struct WozChunkSpan
{
    Byte    id[4]   = {};
    size_t  start   = 0;
    size_t  payload = 0;
    size_t  size    = 0;
};


class WozChunkWalker
{
public:
    static constexpr size_t  kHeaderSize      = 12;
    static constexpr size_t  kChunkHeaderSize = 8;
    static constexpr size_t  kCrcOffset       = 8;

    static void      Walk        (std::span<const Byte> raw, vector<WozChunkSpan> & outChunks, size_t & outEnd);
    static bool      IsChunk     (const WozChunkSpan & chunk, const Byte (&id)[4]);
    static uint32_t  ReadSize    (const Byte * bytes);
    static void      WriteSize   (Byte * bytes, uint32_t value);
    static void      UpdateCrc   (vector<Byte> & ioFile);
    static uint32_t  ComputeCrc  (std::span<const Byte> file);
};
