#include "Pch.h"

#include "Machines/Apple2/Common/WozChunkWalker.h"

#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::Walk
//
//  Every chunk from the header on, stopping at the first four bytes that
//  are not uppercase letters or at a chunk that runs past the end. outEnd is
//  where the walk stopped.
//
////////////////////////////////////////////////////////////////////////////////

void WozChunkWalker::Walk (std::span<const Byte> raw, vector<WozChunkSpan> & outChunks, size_t & outEnd)
{
    size_t  pos  = kHeaderSize;
    bool    more = true;



    outChunks.clear();

    while (more && pos + kChunkHeaderSize <= raw.size())
    {
        WozChunkSpan  chunk;
        bool          isId = std::all_of (raw.begin() + (ptrdiff_t) pos, raw.begin() + (ptrdiff_t) (pos + sizeof (chunk.id)), [] (Byte b) { return b >= 'A' && b <= 'Z'; });

        chunk.start   = pos;
        chunk.payload = pos + kChunkHeaderSize;
        chunk.size    = ReadSize (raw.data() + pos + sizeof (chunk.id));
        more          = isId && chunk.payload + chunk.size <= raw.size();

        if (more)
        {
            memcpy (chunk.id, raw.data() + pos, sizeof (chunk.id));
            outChunks.push_back (chunk);
            pos = chunk.payload + chunk.size;
        }
    }

    outEnd = pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::IsChunk
//
////////////////////////////////////////////////////////////////////////////////

bool WozChunkWalker::IsChunk (const WozChunkSpan & chunk, const Byte (&id)[4])
{
    return memcmp (chunk.id, id, sizeof (chunk.id)) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::ReadSize
//
//  A chunk's 32-bit little-endian size field.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WozChunkWalker::ReadSize (const Byte * bytes)
{
    return static_cast<uint32_t> (bytes[0]
        | (static_cast<uint32_t> (bytes[1]) << 8)
        | (static_cast<uint32_t> (bytes[2]) << 16)
        | (static_cast<uint32_t> (bytes[3]) << 24));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::WriteSize
//
////////////////////////////////////////////////////////////////////////////////

void WozChunkWalker::WriteSize (Byte * bytes, uint32_t value)
{
    bytes[0] = static_cast<Byte> (value         & 0xFF);
    bytes[1] = static_cast<Byte> ((value >> 8)  & 0xFF);
    bytes[2] = static_cast<Byte> ((value >> 16) & 0xFF);
    bytes[3] = static_cast<Byte> ((value >> 24) & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::ComputeCrc
//
//  The header's CRC-32, over every byte after the header.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t WozChunkWalker::ComputeCrc (std::span<const Byte> file)
{
    return ZipArchive::ComputeCrc32 (file.subspan (kHeaderSize));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunkWalker::UpdateCrc
//
//  Writes the header's CRC-32 for the file as it is now.
//
////////////////////////////////////////////////////////////////////////////////

void WozChunkWalker::UpdateCrc (vector<Byte> & ioFile)
{
    WriteSize (ioFile.data() + kCrcOffset, ComputeCrc (ioFile));
}
