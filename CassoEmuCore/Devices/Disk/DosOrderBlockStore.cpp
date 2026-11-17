#include "Pch.h"

#include "Devices/Disk/DosOrderBlockStore.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DosOrderBlockStore::DosOrderBlockStore
//
//  Each half of each block is placed in the DOS-order buffer where the
//  volume layer reads it, and its DOS sector noted.
//
////////////////////////////////////////////////////////////////////////////////

DosOrderBlockStore::DosOrderBlockStore (IBlockStore & blocks) : m_blocks (blocks)
{
    size_t  sector = 0;



    static_assert (kSectors == (size_t) ProDosSkeleton::kTotalBlocks * 2);

    for (int block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
    {
        for (Byte half = 0; half < 2; half++)
        {
            sector = ProDosSkeleton::GetBlockByteOffset (block, half * kSectorBytes) / kSectorBytes;

            m_blockOf[sector] = (uint16_t) block;
            m_halfOf[sector]  = half;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosOrderBlockStore::IsDosOrderSize
//
////////////////////////////////////////////////////////////////////////////////

bool DosOrderBlockStore::IsDosOrderSize (uint32_t blocks)
{
    return blocks == (uint32_t) ProDosSkeleton::kTotalBlocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosOrderBlockStore::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosOrderBlockStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT                         hr     = S_OK;
    std::array<Byte, kBlockBytes>   block  = {};
    size_t                          sector = 0;
    bool                            inside = (size_t) (first + count) * 2 <= kSectors && outBytes.size() >= (size_t) count * kBlockBytes;



    CBRAEx (inside, E_INVALIDARG);

    for (size_t at = 0; at < (size_t) count * 2; at++)
    {
        sector = (size_t) first * 2 + at;

        hr = m_blocks.ReadBlocks (m_blockOf[sector], 1, block);
        CHR (hr);

        std::copy_n (block.begin() + (ptrdiff_t) (m_halfOf[sector] * kSectorBytes), kSectorBytes, outBytes.begin() + (ptrdiff_t) (at * kSectorBytes));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DosOrderBlockStore::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DosOrderBlockStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT                         hr      = S_OK;
    std::array<Byte, kBlockBytes>   block   = {};
    size_t                          sector  = 0;
    size_t                          sectors = bytes.size() / kSectorBytes;
    bool                            inside  = bytes.size() % kBlockBytes == 0 && (size_t) first * 2 + sectors <= kSectors;



    CBRAEx (inside, E_INVALIDARG);

    for (size_t at = 0; at < sectors; at++)
    {
        sector = (size_t) first * 2 + at;

        hr = m_blocks.ReadBlocks (m_blockOf[sector], 1, block);
        CHR (hr);

        std::copy_n (bytes.begin() + (ptrdiff_t) (at * kSectorBytes), kSectorBytes, block.begin() + (ptrdiff_t) (m_halfOf[sector] * kSectorBytes));

        hr = m_blocks.WriteBlocks (m_blockOf[sector], block);
        CHR (hr);
    }

Error:
    return hr;
}
