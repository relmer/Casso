#pragma once

#include "Pch.h"

class MemoryBus;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  RamPages
//
//  Saves one RAM buffer so that reverse execution's keyframe saves copy only
//  the pages written since the last one. The buffer is split into chunks of
//  a few pages; a sharing save hands each chunk over as an immutable segment
//  (see StateWriter::WriteShared), the same one every save shares until a
//  page of the chunk is written, so a save copies only the chunks written
//  since the save before it. Any other save writes the bytes as they are.
//
//  Writes are noticed through one written flag per 256-byte page. The bus
//  sets them for every store it makes into the buffer once the buffer is
//  registered with it (Attach); the owner calls MarkWritten for a write it
//  makes itself and MarkAllWritten when it fills or loads the whole buffer.
//  A sharing save clears the flags of the chunks it copied. A missed write
//  leaves a stale chunk in every later save, so the debug build checks
//  every chunk a sharing save keeps against the buffer it was copied from
//  (MachineHost::CheckSharedSave).
//
//  Chunk copies come from a pool of the buffers no save holds any
//  more, so a recording that has reached its working size allocates none.
//
////////////////////////////////////////////////////////////////////////////////

class RamPages
{
public:
    static constexpr size_t  kPageBytes  = 256;
    static constexpr size_t  kChunkPages = 4;
    static constexpr size_t  kChunkBytes = kPageBytes * kChunkPages;

               RamPages  () = default;
               ~RamPages ();

               RamPages  (const RamPages &) = delete;
    RamPages & operator= (const RamPages &) = delete;

    void       Attach         (MemoryBus * bus, const Byte * data, size_t size);
    void       Detach         ();
    void       OnBusDestroyed ();

    void       MarkWritten    (size_t offset)       { m_written[offset / kPageBytes] = 1; }
    void       MarkAllWritten ();
    bool       IsWritten      (size_t offset) const { return m_written[offset / kPageBytes] != 0; }

    void       Save           (StateWriter & writer, const Byte * data, size_t size) const;

private:
    using Chunk = std::shared_ptr<const std::vector<Byte>>;

    static constexpr size_t  kPoolScan = 64;      // pool buffers tried before a new one is made

    std::shared_ptr<std::vector<Byte>>  TakeChunkBuffer () const;
    bool                                IsChunkWritten  (size_t chunk) const;

    MemoryBus                                                * m_bus      = nullptr;
    const Byte                                               * m_data     = nullptr;
    size_t                                                     m_size     = 0;
    mutable std::vector<Byte>                                  m_written;              // one flag per page
    mutable std::vector<Chunk>                                 m_chunks;               // what the last sharing save shared
    mutable std::vector<std::shared_ptr<std::vector<Byte>>>    m_pool;
    mutable size_t                                             m_poolNext = 0;
};