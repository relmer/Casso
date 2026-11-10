#include "Pch.h"

#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceReadStore
//
//  An open drive as a block store that is only read, for finding the Apple
//  layout on it.
//
////////////////////////////////////////////////////////////////////////////////

class DeviceReadStore : public IBlockStore
{
public:
    DeviceReadStore (IRawDevice & device, uint64_t blocks) : m_device (device), m_blocks ((uint32_t) std::min<uint64_t> (blocks, UINT32_MAX)) {}

    uint32_t  GetBlockCount() const override                                                    { return m_blocks; }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override { return m_device.ReadBlocks (first, count, outBytes); }
    HRESULT   WriteBlocks   (uint32_t, std::span<const Byte>) override                          { return E_ACCESSDENIED; }
    HRESULT   Commit        () override                                                         { return S_OK; }

private:
    IRawDevice  & m_device;
    uint32_t      m_blocks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::Run()
{
    HRESULT  hr                                      = S_OK;
    HRESULT  hrRead                                  = S_OK;
    Byte     header[RawDeviceProtocol::kHeaderBytes] = {};
    bool     valid                                   = true;



    while (valid)
    {
        int       number = 0;
        uint64_t  first  = 0;
        uint32_t  count  = 0;

        hrRead = RawDeviceProtocol::ReadExact (m_stream, header, sizeof (header), INFINITE);
        BAIL_OUT_IF (FAILED (hrRead), S_OK);

        number = (int) RawDeviceProtocol::GetLong (header + 1);
        first  = RawDeviceProtocol::GetQuad (header + 5);
        count  = RawDeviceProtocol::GetLong (header + 13);

        switch (header[0])
        {
            case RawDeviceProtocol::kOpen:
                hr = ServeOpen (number);
                break;

            case RawDeviceProtocol::kClose:
                hr = ServeClose (number);
                break;

            case RawDeviceProtocol::kRead:
                hr = ServeRead (number, first, count);
                break;

            case RawDeviceProtocol::kWrite:
                hr = ServeWrite (number, first, count);
                break;

            default:
                hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
                break;
        }

        valid = SUCCEEDED (hr);
    }

Error:
    for (auto & [number, session] : m_sessions)
    {
        session.device->Close();
    }

    m_sessions.clear();
    m_stream.Close();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::FindApplePartitions
//
//  Nothing is writable on a drive with no Apple layout, or one that cannot
//  be read.
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceServer::FindApplePartitions (IRawDevice & device, uint64_t blocks, std::vector<BlockExtent> & outExtents)
{
    DeviceReadStore              store (device, blocks);
    PartitionLayoutKind          kind   = PartitionLayoutKind::None;
    HRESULT                      hrList = S_OK;
    std::vector<PartitionEntry>  entries;



    outExtents.clear();

    hrList = PartitionAccess::ListStore (store, kind, entries);

    if (FAILED (hrList) || kind == PartitionLayoutKind::None)
    {
        return;
    }

    for (const PartitionEntry & entry : entries)
    {
        if (entry.partition.blockCount > 0)
        {
            outExtents.push_back (BlockExtent { entry.partition.firstBlock, entry.partition.blockCount });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeOpen
//
//  A drive open already is opened again, so its Apple layout is found anew.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeOpen (int number)
{
    HRESULT            hr      = S_OK;
    HRESULT            result  = S_OK;
    bool               hasRoom = false;
    Session            session;
    std::vector<Byte>  payload;



    ServeClose (number);

    hasRoom = m_sessions.size() < RawDeviceProtocol::kMaxDrives;
    result  = hasRoom ? S_OK : HRESULT_FROM_WIN32 (ERROR_TOO_MANY_OPEN_FILES);

    if (SUCCEEDED (result))
    {
        session.device = m_makeDevice();
        result         = session.device != nullptr ? session.device->Open (number, session.drive) : E_OUTOFMEMORY;
    }

    if (SUCCEEDED (result) && !session.drive.removable)
    {
        session.device->Close();
        result = E_ACCESSDENIED;
    }

    if (SUCCEEDED (result))
    {
        FindApplePartitions (*session.device, session.drive.bytes / IRawDevice::kBlockBytes, session.writable);

        RawDeviceProtocol::AppendQuad (payload, session.drive.bytes);
        payload.push_back (session.drive.removable ? 1 : 0);
        RawDeviceProtocol::AppendLong (payload, (uint32_t) session.drive.number);
        RawDeviceProtocol::AppendText (payload, session.drive.model);
        RawDeviceProtocol::AppendText (payload, session.drive.bus);

        m_sessions.emplace (number, std::move (session));
    }

    hr = Reply (result, payload);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeClose
//
//  Closing a drive that is not open succeeds. No reply is sent: the client
//  does not wait for one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeClose (int number)
{
    auto  found = m_sessions.find (number);



    if (found != m_sessions.end())
    {
        found->second.device->Close();
        m_sessions.erase (found);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeRead
//
//  A count of zero, or over the protocol's limit, ends the session.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeRead (int number, uint64_t first, uint32_t count)
{
    HRESULT            hr      = S_OK;
    HRESULT            result  = S_OK;
    bool               sized   = count > 0 && count <= RawDeviceProtocol::kMaxBlocks;
    Session          * session = nullptr;
    std::vector<Byte>  blocks;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    result = CheckRange (number, first, count, session);

    if (SUCCEEDED (result))
    {
        blocks.resize ((size_t) count * IRawDevice::kBlockBytes);
        result = session->device->ReadBlocks (first, count, blocks);
    }

    if (FAILED (result))
    {
        blocks.clear();
    }

    hr = Reply (result, blocks);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeWrite
//
//  The blocks are read from the stream before anything is checked, so the
//  next request starts at its header.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeWrite (int number, uint64_t first, uint32_t count)
{
    HRESULT            hr      = S_OK;
    HRESULT            result  = S_OK;
    bool               sized   = count > 0 && count <= RawDeviceProtocol::kMaxBlocks;
    Session          * session = nullptr;
    std::vector<Byte>  blocks;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    blocks.resize ((size_t) count * IRawDevice::kBlockBytes);

    hr = RawDeviceProtocol::ReadExact (m_stream, blocks.data(), blocks.size(), RawDeviceProtocol::kReplyTimeoutMs);
    CHR (hr);

    result = CheckRange (number, first, count, session);

    if (SUCCEEDED (result))
    {
        result = CheckWrite (*session, first, count);
    }

    if (SUCCEEDED (result))
    {
        result = session->device->WriteBlocks (first, blocks);
    }

    hr = Reply (result, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::CheckRange
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::CheckRange (int number, uint64_t first, uint32_t count, Session * & outSession)
{
    HRESULT   hr     = S_OK;
    auto      found  = m_sessions.find (number);
    bool      isOpen = found != m_sessions.end();
    uint64_t  blocks = 0;
    bool      inside = false;



    outSession = nullptr;

    CBREx (isOpen, E_ACCESSDENIED);

    blocks = found->second.drive.bytes / IRawDevice::kBlockBytes;
    inside = first <= blocks && count <= blocks - first;
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

    outSession = &found->second;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::CheckWrite
//
//  The mounted volumes are read at each write, as Windows can mount one at
//  any time. A drive whose volumes cannot be read is not written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::CheckWrite (Session & session, uint64_t first, uint32_t count)
{
    HRESULT                   hr        = S_OK;
    bool                      inApple   = false;
    bool                      inWindows = false;
    std::vector<BlockExtent>  mounted;



    for (const BlockExtent & extent : session.writable)
    {
        inApple = inApple || extent.Contains (first, count);
    }

    CBREx (inApple, E_ACCESSDENIED);

    hr = session.device->GetMountedExtents (mounted);
    CHR (hr);

    for (const BlockExtent & extent : mounted)
    {
        inWindows = inWindows || extent.Overlaps (first, count);
    }

    CBREx (!inWindows, E_ACCESSDENIED);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::Reply
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::Reply (HRESULT result, std::span<const Byte> payload)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  bytes;



    RawDeviceProtocol::AppendLong (bytes, (uint32_t) result);
    bytes.insert (bytes.end(), payload.begin(), payload.end());

    hr = m_stream.Write (bytes.data(), bytes.size());
    CHR (hr);

Error:
    return hr;
}
