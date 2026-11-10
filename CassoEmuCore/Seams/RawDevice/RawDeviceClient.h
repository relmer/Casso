#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"
#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient
//
//  A Casso Explorer window's side of RawDeviceProtocol: the drive the
//  elevated process has open, as a block store.
//
//  A written block is in memory until `Commit`, which sends every one to the
//  drive, and a read returns it from there. `Discard` drops them, for a
//  write the user declined.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceClient : public IBlockStore
{
public:
    explicit RawDeviceClient (IByteStream & stream) : m_stream (stream) {}

    //  Fails with the elevated process's reason when the drive did not open
    //  there, and with E_ACCESSDENIED for a drive that is not removable.
    HRESULT   Connect       (PhysicalDrive & outDrive);

    uint32_t  GetBlockCount () const override { return m_blockCount; }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override;

    size_t    GetPendingBlockCount () const { return m_pending.size(); }
    void      Discard              ()       { m_pending.clear(); }

private:
    HRESULT   SendHeader (Byte operation, uint64_t first, uint32_t count, std::span<const Byte> blocks);
    HRESULT   ReadResult ();

    IByteStream                            & m_stream;
    uint32_t                                 m_blockCount = 0;
    std::map<uint32_t, std::vector<Byte>>    m_pending;
};
