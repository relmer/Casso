#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"
#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection
//
//  Casso Explorer's side of RawDeviceProtocol: the one stream to the storage
//  card broker, shared by every card open through it. Each request and its
//  reply are sent under one lock, so cards can be read from several
//  threads.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceConnection
{
public:
    explicit RawDeviceConnection (IByteStream & stream) : m_stream (stream) {}

    //  Fails with the broker's reason when the drive did not open there, and
    //  with E_ACCESSDENIED for a drive that is not a storage card.
    HRESULT  Open   (int number, PhysicalDrive & outDrive);
    void     Close  (int number);

    HRESULT  Read   (int number, uint64_t first, uint32_t count, std::span<Byte> outBytes);
    HRESULT  Write  (int number, uint64_t first, uint32_t count, std::span<const Byte> blocks);

    //  The card erased and given CFFA's layout by the broker. The card is
    //  opened for it and closed after.
    HRESULT  Initialize (int number);

private:
    HRESULT  SendHeader (Byte operation, int number, uint64_t first, uint32_t count, std::span<const Byte> blocks);
    HRESULT  ReadResult (DWORD timeoutMs = RawDeviceProtocol::kReplyTimeoutMs);

    IByteStream  & m_stream;
    std::mutex     m_mutex;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient
//
//  One card open through a RawDeviceConnection, as a block store.
//
//  A written block is in memory until `Commit`, which sends every one to the
//  card, and a read returns it from there. `Discard` drops them, for a
//  write the user declined. Destroying the client closes the card.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceClient : public IBlockStore
{
public:
    explicit RawDeviceClient (RawDeviceConnection & connection) : m_connection (connection) {}
    ~RawDeviceClient () override;

    RawDeviceClient (const RawDeviceClient &) = delete;
    RawDeviceClient & operator= (const RawDeviceClient &) = delete;

    //  A drive of more blocks than a block number counts is cut off there.
    HRESULT   Connect       (int number, PhysicalDrive & outDrive);

    uint32_t  GetBlockCount () const override { return m_blockCount; }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override;

    size_t    GetPendingBlockCount () const { return m_pending.size(); }
    void      Discard              ()       { m_pending.clear(); }

private:
    RawDeviceConnection                    & m_connection;
    int                                      m_number     = -1;
    uint32_t                                 m_blockCount = 0;
    std::map<uint32_t, std::vector<Byte>>    m_pending;
};
