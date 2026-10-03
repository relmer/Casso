#include "Pch.h"

#include "Core/RamPages.h"

#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ~RamPages
//
////////////////////////////////////////////////////////////////////////////////

RamPages::~RamPages()
{
    Detach();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
//  Sizes the flags for a buffer of size bytes, every page written, so the
//  first sharing save copies every chunk, and registers the buffer with the
//  bus, when there is one, so the bus marks the pages it stores into.
//
////////////////////////////////////////////////////////////////////////////////

void RamPages::Attach (
    MemoryBus   * bus,
    const Byte  * data,
    size_t        size)
{
    Detach();

    m_bus  = bus;
    m_data = data;
    m_size = size;

    m_written.assign ((size + kPageBytes - 1) / kPageBytes, 1);
    m_chunks.assign  ((size + kChunkBytes - 1) / kChunkBytes, Chunk());

    if (m_bus != nullptr)
    {
        m_bus->RegisterRamPages (m_data, m_size, m_written.data());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Detach
//
////////////////////////////////////////////////////////////////////////////////

void RamPages::Detach()
{
    if (m_bus != nullptr)
    {
        m_bus->UnregisterRamPages (m_data);
        m_bus = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkAllWritten
//
//  For a fill or a load of the whole buffer.
//
////////////////////////////////////////////////////////////////////////////////

void RamPages::MarkAllWritten()
{
    std::fill (m_written.begin(), m_written.end(), Byte (1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Save
//
//  The buffer's bytes. A sharing writer gets one segment per chunk: the one
//  the last sharing save gave, unless a page of the chunk has been written
//  since, in which case the chunk is copied into a buffer of its own first
//  and its flags cleared. Any other writer, or a buffer never attached, gets
//  the bytes themselves.
//
////////////////////////////////////////////////////////////////////////////////

void RamPages::Save (
    StateWriter  & writer,
    const Byte   * data,
    size_t         size) const
{
    std::shared_ptr<std::vector<Byte>>  buffer;
    size_t                              chunk     = 0;
    size_t                              offset    = 0;
    size_t                              count     = 0;
    size_t                              page      = 0;
    bool                                isTracked = writer.IsSharing() && m_bus != nullptr && size == m_size;



    if (!isTracked)
    {
        writer.WriteBytes (data, size);
        return;
    }

    for (chunk = 0; chunk < m_chunks.size(); chunk++)
    {
        if (m_chunks[chunk] == nullptr || IsChunkWritten (chunk))
        {
            offset = chunk * kChunkBytes;
            count  = std::min (kChunkBytes, size - offset);
            buffer = TakeChunkBuffer();

            buffer->assign (data + offset, data + offset + count);

            m_chunks[chunk] = buffer;

            for (page = offset / kPageBytes; page < (offset + count + kPageBytes - 1) / kPageBytes; page++)
            {
                m_written[page] = 0;
            }
        }

        writer.WriteShared (m_chunks[chunk]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsChunkWritten
//
////////////////////////////////////////////////////////////////////////////////

bool RamPages::IsChunkWritten (size_t chunk) const
{
    size_t  first = chunk * kChunkPages;
    size_t  last  = std::min (first + kChunkPages, m_written.size());
    size_t  page  = 0;
    bool    isHit = false;



    for (page = first; page < last && !isHit; page++)
    {
        isHit = m_written[page] != 0;
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeChunkBuffer
//
//  A buffer only the pool holds, every checkpoint that shared it having been
//  dropped, or a new one when none of the next few is free. The pool is
//  walked round from where the last search stopped, so the oldest buffers,
//  the first to be freed, are the first tried.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<Byte>> RamPages::TakeChunkBuffer() const
{
    std::shared_ptr<std::vector<Byte>>  buffer;
    size_t                              tries  = std::min (kPoolScan, m_pool.size());
    size_t                              i      = 0;



    for (i = 0; i < tries && buffer == nullptr; i++)
    {
        m_poolNext = (m_poolNext + 1 < m_pool.size()) ? m_poolNext + 1 : 0;

        if (m_pool[m_poolNext].use_count() == 1)
        {
            buffer = m_pool[m_poolNext];
        }
    }

    if (buffer == nullptr)
    {
        buffer = std::make_shared<std::vector<Byte>>();
        buffer->reserve (kChunkBytes);

        m_pool.push_back (buffer);
    }

    return buffer;
}





