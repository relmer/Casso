#include "Pch.h"

#include "Devices/Disk/CachingBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CachingBlockStore::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CachingBlockStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT                    hr     = S_OK;
    bool                       inside = (uint64_t) first + count <= GetBlockCount();
    bool                       sized  = outBytes.size() >= (size_t) count * kBlockBytes;
    uint32_t                   i      = 0;
    const std::vector<Byte>  * chunk  = nullptr;



    CBRAEx (inside, E_INVALIDARG);
    CBRAEx (sized,  E_INVALIDARG);

    for (i = 0; i < count; i++)
    {
        uint32_t  block   = first + i;
        Byte    * target  = outBytes.data() + (size_t) i * kBlockBytes;
        auto      changed = m_changed.find (block);

        if (changed != m_changed.end())
        {
            std::copy_n (changed->second.begin(), kBlockBytes, target);
            continue;
        }

        hr = LoadChunk (block / kChunkBlocks, chunk);
        CHR (hr);

        std::copy_n (chunk->begin() + (ptrdiff_t) ((size_t) (block % kChunkBlocks) * kBlockBytes), kBlockBytes, target);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CachingBlockStore::WriteBlocks
//
//  Whole blocks only. A block equal to the store's block is not a change,
//  and a block written back to what the store has stops being one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CachingBlockStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT                    hr     = S_OK;
    uint64_t                   count  = bytes.size() / kBlockBytes;
    bool                       whole  = bytes.size() % kBlockBytes == 0;
    bool                       inside = (uint64_t) first + count <= GetBlockCount();
    uint32_t                   i      = 0;
    const std::vector<Byte>  * chunk  = nullptr;



    CBRAEx (whole,  E_INVALIDARG);
    CBRAEx (inside, E_INVALIDARG);

    for (i = 0; i < (uint32_t) count; i++)
    {
        uint32_t      block  = first + i;
        const Byte  * source = bytes.data() + (size_t) i * kBlockBytes;
        bool          same   = false;

        hr = LoadChunk (block / kChunkBlocks, chunk);
        CHR (hr);

        same = std::equal (source, source + kBlockBytes, chunk->begin() + (ptrdiff_t) ((size_t) (block % kChunkBlocks) * kBlockBytes));

        if (same)
        {
            m_changed.erase (block);
        }
        else
        {
            m_changed[block].assign (source, source + kBlockBytes);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CachingBlockStore::Commit
//
//  The changed blocks go to the store in runs of consecutive blocks. After a
//  failure the cache is as it was and the changes are dropped, and the store
//  may have some of them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CachingBlockStore::Commit()
{
    HRESULT            hr    = S_OK;
    auto               it    = m_changed.begin();
    uint32_t           first = 0;
    std::vector<Byte>  run;



    while (it != m_changed.end())
    {
        first = it->first;
        run.clear();

        while (it != m_changed.end() && it->first == first + run.size() / kBlockBytes && run.size() < (size_t) kChunkBlocks * kBlockBytes)
        {
            run.insert (run.end(), it->second.begin(), it->second.end());
            ++it;
        }

        hr = m_inner.WriteBlocks (first, run);
        CHR (hr);
    }

    hr = m_inner.Commit();
    CHR (hr);

    for (const auto & [block, bytes] : m_changed)
    {
        std::vector<Byte> & chunk = m_chunks[block / kChunkBlocks];

        std::copy (bytes.begin(), bytes.end(), chunk.begin() + (ptrdiff_t) ((size_t) (block % kChunkBlocks) * kBlockBytes));
    }

Error:
    m_changed.clear();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CachingBlockStore::LoadChunk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CachingBlockStore::LoadChunk (uint32_t chunk, const std::vector<Byte> * & outBytes)
{
    HRESULT            hr     = S_OK;
    auto               found  = m_chunks.find (chunk);
    uint32_t           first  = chunk * kChunkBlocks;
    uint32_t           count  = std::min (kChunkBlocks, GetBlockCount() - first);
    std::vector<Byte>  bytes;



    outBytes = nullptr;

    if (found == m_chunks.end())
    {
        bytes.resize ((size_t) count * kBlockBytes);

        hr = m_inner.ReadBlocks (first, count, bytes);
        CHR (hr);

        found = m_chunks.emplace (chunk, std::move (bytes)).first;
    }

    outBytes = &found->second;

Error:
    return hr;
}