#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DosOrderBlockStore
//
//  A 140K block device's 280 blocks in ProDOS order, as a 143,360-byte
//  buffer in DOS logical sector order, the order the volume layer reads a
//  140K volume in. Block N of this store is bytes N * 512 to N * 512 + 511 of
//  that buffer: two DOS sectors, each half of a block of the device. A write
//  reads each block it changes half of and writes the whole block back.
//
////////////////////////////////////////////////////////////////////////////////

class DosOrderBlockStore : public IBlockStore
{
public:
    explicit DosOrderBlockStore (IBlockStore & blocks);

    uint32_t  GetBlockCount () const override { return m_blocks.GetBlockCount(); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override { return m_blocks.Commit(); }

    //  Whether a device of this many blocks is read through this store.
    static bool  IsDosOrderSize (uint32_t blocks);

private:
    static constexpr size_t  kSectorBytes = 256;
    static constexpr size_t  kSectors     = 560;

    IBlockStore                     & m_blocks;
    std::array<uint16_t, kSectors>    m_blockOf  = {};   // each DOS sector's block of the device, and in
    std::array<Byte, kSectors>        m_halfOf   = {};   // which half
};
