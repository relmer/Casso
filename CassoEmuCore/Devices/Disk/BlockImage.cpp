#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/BlockBuffer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImage::size
//
////////////////////////////////////////////////////////////////////////////////

size_t BlockImage::size() const
{
    if (m_view != nullptr)
    {
        return m_view->size();
    }

    return m_buffer != nullptr ? m_buffer->size() : m_size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImage::ToVector
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> BlockImage::ToVector() const
{
    std::vector<Byte>  bytes;



    if (m_view != nullptr)
    {
        return *m_view;
    }

    if (m_buffer != nullptr)
    {
        return m_buffer->ToVector();
    }

    bytes.resize (m_size);

    for (size_t chunk = 0; chunk * kChunkBytes < m_size; chunk++)
    {
        const std::vector<Byte> & loaded = LoadChunk (chunk);
        size_t                    first  = chunk * kChunkBytes;

        std::copy_n (loaded.begin(), std::min (kChunkBytes, m_size - first), bytes.begin() + (ptrdiff_t) first);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImage::ReadStoreByte
//
////////////////////////////////////////////////////////////////////////////////

Byte BlockImage::ReadStoreByte (size_t at) const
{
    if (m_buffer != nullptr)
    {
        return at < m_buffer->size() ? (*m_buffer)[at] : 0;
    }

    if (at >= m_size)
    {
        return 0;
    }

    return LoadChunk (at / kChunkBytes)[at % kChunkBytes];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImage::LoadChunk
//
//  A chunk past the last whole one is read as far as the store goes, and the
//  rest of it is zeros.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<Byte> & BlockImage::LoadChunk (size_t chunk) const
{
    auto                found = m_chunks.find (chunk);
    HRESULT             hr    = S_OK;
    uint32_t            first = (uint32_t) (chunk * kChunkBlocks);
    uint32_t            count = 0;
    std::vector<Byte>   bytes (kChunkBytes, 0);



    if (found != m_chunks.end())
    {
        return found->second;
    }

    count = (uint32_t) std::min<size_t> (kChunkBlocks, m_store->GetBlockCount() - first);

    hr = m_store->ReadBlocks (first, count, std::span<Byte> (bytes.data(), (size_t) count * IBlockStore::kBlockBytes));

    if (FAILED (hr))
    {
        std::fill (bytes.begin(), bytes.end(), (Byte) 0);
        m_readError = SUCCEEDED (m_readError) ? hr : m_readError;
    }

    return m_chunks.emplace (chunk, std::move (bytes)).first->second;
}
