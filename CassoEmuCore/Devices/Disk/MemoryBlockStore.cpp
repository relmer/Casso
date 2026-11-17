#include "Pch.h"

#include "Devices/Disk/MemoryBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockStore::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    size_t   bytes  = (size_t) count * kBlockBytes;
    bool     inside = (uint64_t) first + count <= GetBlockCount();
    bool     sized  = outBytes.size() >= bytes;



    CBRAEx (inside, E_INVALIDARG);
    CBRAEx (sized,  E_INVALIDARG);

    std::copy_n (m_bytes.begin() + (ptrdiff_t) ((size_t) first * kBlockBytes), bytes, outBytes.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockStore::WriteBlocks
//
//  Whole blocks only: a write that ends partway through a block is refused.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT   hr     = S_OK;
    uint64_t  count  = bytes.size() / kBlockBytes;
    bool      whole  = bytes.size() % kBlockBytes == 0;
    bool      inside = (uint64_t) first + count <= GetBlockCount();



    CBRAEx (whole,  E_INVALIDARG);
    CBRAEx (inside, E_INVALIDARG);

    std::copy (bytes.begin(), bytes.end(), m_bytes.begin() + (ptrdiff_t) ((size_t) first * kBlockBytes));

Error:
    return hr;
}
