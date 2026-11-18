#include "Pch.h"

#include "Devices/Disk/OffsetBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OffsetBlockStore::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OffsetBlockStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    bool     inside = (uint64_t) first + count <= m_count;



    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    hr = m_store.ReadBlocks (m_first + first, count, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OffsetBlockStore::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OffsetBlockStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT  hr     = S_OK;
    bool     inside = (uint64_t) first + bytes.size() / kBlockBytes <= m_count;



    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    hr = m_store.WriteBlocks (m_first + first, bytes);
    CHR (hr);

Error:
    return hr;
}
