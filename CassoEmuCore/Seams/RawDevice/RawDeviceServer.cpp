#include "Pch.h"

#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Seams/RawDevice/Fat32Format.h"





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

            case RawDeviceProtocol::kInitialize:
                hr = ServeInitialize (number, first);
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
        LogTraffic (number, session, true);
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

    hrList = PartitionAccess::ListCard (store, kind, entries);

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

    Log (std::format (L"open drive {} ({}, {} bytes, bus {}, card {})", number, TextEncoding::NarrowToWide (session.drive.model), session.drive.bytes, TextEncoding::NarrowToWide (session.drive.bus), session.drive.removable ? 1 : 0), result);

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
        LogTraffic (number, found->second, true);
        found->second.device->Close();
        m_sessions.erase (found);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::LogTraffic
//
//  The bytes moved, the requests, the time in the device and the time since
//  the last line. Most of the time outside the device is the round trips.
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceServer::LogTraffic (int number, Session & session, bool closing)
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    Clock::time_point  now     = Clock::now();
    bool               isMoved = session.readBytes + session.writtenBytes > 0;



    if (m_log && isMoved)
    {
        m_log (std::format (L"drive {} traffic{}: read {} bytes, wrote {} bytes, {} requests, {} ms in the device of {} ms",
                            number, closing ? L" at close" : L"", session.readBytes, session.writtenBytes, session.requests,
                            duration_cast<milliseconds> (session.deviceTime).count(), duration_cast<milliseconds> (now - session.since).count()));
    }

    session.since        = now;
    session.readBytes    = 0;
    session.writtenBytes = 0;
    session.requests     = 0;
    session.deviceTime   = {};
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
        Clock::time_point  start = Clock::now();

        blocks.resize ((size_t) count * IRawDevice::kBlockBytes);
        result = session->device->ReadBlocks (first, count, blocks);

        session->deviceTime += Clock::now() - start;
        session->readBytes  += blocks.size();
        session->requests++;

        if (session->readBytes + session->writtenBytes >= kTrafficLogBytes)
        {
            LogTraffic (number, *session, false);
        }
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
        Clock::time_point  start = Clock::now();

        result = session->device->WriteBlocks (first, blocks);

        session->deviceTime   += Clock::now() - start;
        session->writtenBytes += blocks.size();
        session->requests++;

        if (session->readBytes + session->writtenBytes >= kTrafficLogBytes)
        {
            LogTraffic (number, *session, false);
        }
    }

    hr = Reply (result, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::CountInitialVolumes
//
//  The same walk WriteCffaLayout makes, without the writes. A card of one
//  slice or less is one volume of its whole size.
//
////////////////////////////////////////////////////////////////////////////////

int RawDeviceServer::CountInitialVolumes (uint64_t blocks)
{
    uint64_t  next  = 0;
    int       count = 0;



    if (blocks < kMinVolumeBlocks)
    {
        return 0;
    }

    if (blocks <= kSliceBlocks)
    {
        return 1;
    }

    while (count < kInitialVolumes && blocks - next >= kGptBlocks + kMinVolumeBlocks)
    {
        next += std::min<uint64_t> (kVolumeBlocks, blocks - next - kGptBlocks) + 1;
        count++;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeInitialize
//
//  The volumes on the card are dismounted first, and the card's Apple layout
//  is found again after the write. `layout` is a count of CFFA volumes, or
//  kLayoutFat32.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeInitialize (int number, uint64_t request)
{
    HRESULT          hr       = S_OK;
    HRESULT          result   = S_OK;
    Session        * session  = nullptr;
    uint64_t         blocks   = 0;
    bool             valid    = false;
    bool             zeroFill = (request & RawDeviceProtocol::kLayoutZeroFill) != 0;
    uint64_t         layout   = request & ~RawDeviceProtocol::kLayoutZeroFill;
    WriteProgress    progress;



    //  Each step of the write is a reply of S_FALSE with the bytes written and
    //  the bytes to write, before the reply that ends the request.
    progress = [this] (uint64_t done, uint64_t total)
    {
        std::vector<Byte>  payload;
        HRESULT            hrSent = S_OK;

        RawDeviceProtocol::AppendQuad (payload, done);
        RawDeviceProtocol::AppendQuad (payload, total);

        hrSent = Reply (S_FALSE, payload);
        IGNORE_RETURN_VALUE (hrSent, S_OK);
    };

    result = CheckRange (number, 0, 0, session);
    Log (std::format (L"initialize drive {} layout {}: open", number, layout), result);

    if (SUCCEEDED (result))
    {
        blocks = session->drive.bytes / IRawDevice::kBlockBytes;
        valid  = layout == RawDeviceProtocol::kLayoutFat32 ? Fat32Format::CanFormat (blocks)
                                                            : layout >= 1 && layout <= (uint64_t) CountInitialVolumes (blocks);
        result = valid ? S_OK : HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
    }

    if (SUCCEEDED (result))
    {
        result = session->device->LockVolumes();
        Log (L"initialize: lock volumes", result);
    }

    if (SUCCEEDED (result))
    {
        result = layout == RawDeviceProtocol::kLayoutFat32 ? Fat32Format::Write (*session->device, blocks, progress)
                                                            : WriteCffaLayout (*session, (int) layout, zeroFill, progress);
        Log (L"initialize: write", result);
        session->device->UnlockVolumes();

        FindApplePartitions (*session->device, blocks, session->writable);
    }

    hr = Reply (result, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::WriteCffaLayout
//
//  Each slice is an empty ProDOS volume, CARD1 on, and the block after it is
//  cleared. The first blocks of every later place a CFFA volume could be
//  are cleared, so no volume of an earlier layout is found there, and so are
//  the last blocks of the card, as a GPT has its second copy there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::WriteCffaLayout (Session & session, int volumes, bool zeroFill, const WriteProgress & progress)
{
    HRESULT            hr      = S_OK;
    uint64_t           total   = session.drive.bytes / IRawDevice::kBlockBytes;
    uint64_t           next    = 0;
    uint64_t           written = 0;
    uint64_t           toWrite = 0;
    std::vector<Byte>  zeros ((size_t) kGptBlocks * IRawDevice::kBlockBytes, 0);
    std::span<Byte>    header (zeros.data(), (size_t) kHeaderBlocks * IRawDevice::kBlockBytes);



    if (total <= kSliceBlocks)
    {
        std::vector<Byte>  volume ((size_t) std::min<uint64_t> (kVolumeBlocks, total) * IRawDevice::kBlockBytes, 0);

        hr = ProDosSkeleton::Write (volume, "CARD1");
        CHR (hr);

        hr = WriteVolume (session, 0, volume, zeroFill, written, volume.size(), progress);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    toWrite = (uint64_t) volumes * kVolumeBlocks * IRawDevice::kBlockBytes;

    for (int slice = 0; slice < volumes; slice++)
    {
        uint64_t           blocks = std::min<uint64_t> (kVolumeBlocks, total - next - kGptBlocks);
        std::vector<Byte>  volume ((size_t) blocks * IRawDevice::kBlockBytes, 0);

        hr = ProDosSkeleton::Write (volume, std::format ("CARD{}", slice + 1));
        CHR (hr);

        hr = WriteVolume (session, next, volume, zeroFill, written, toWrite, progress);
        CHR (hr);

        next += blocks;

        if (next < total - kGptBlocks && blocks == kVolumeBlocks)
        {
            hr = session.device->WriteBlocks (next, std::span<const Byte> (zeros.data(), IRawDevice::kBlockBytes));
            CHR (hr);

            next++;
        }
    }

    for (int slice = volumes; slice < kInitialVolumes; slice++)
    {
        hr = ClearHeader (session, (uint64_t) slice * kSliceBlocks, next, total, header);
        CHR (hr);
    }

    for (uint64_t large = 0; large < kLargeVolumes; large++)
    {
        hr = ClearHeader (session, 4ull * kSliceBlocks + large * kLargeVolumeBlocks, next, total, header);
        CHR (hr);
    }

    hr = session.device->WriteBlocks (total - kGptBlocks, zeros);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::WriteVolume
//
//  A volume's blocks, written kProgressBlocks at a time, with a report to
//  `progress` after each. Without `zeroFill`, only the blocks its bitmap
//  marks used are written, which for a new volume are the boot blocks, the
//  directory and the bitmap. The total reported is the volume's either way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::WriteVolume (Session & session, uint64_t first, std::span<const Byte> volume, bool zeroFill, uint64_t & ioWritten, uint64_t total, const WriteProgress & progress)
{
    HRESULT                                   hr      = S_OK;
    bool                                      found   = false;
    uint64_t                                  start   = ioWritten;
    std::vector<PartitionAccess::BlockRun>    runs;



    if (!zeroFill)
    {
        hr = PartitionAccess::FindUsedRuns ([volume] (uint32_t block, std::span<Byte> outBlock)
                                            {
                                                std::copy_n (volume.begin() + (ptrdiff_t) block * IRawDevice::kBlockBytes, IRawDevice::kBlockBytes, outBlock.begin());
                                                return S_OK;
                                            },
                                            (uint32_t) (volume.size() / IRawDevice::kBlockBytes), runs, found);
        CHR (hr);
    }

    if (!found)
    {
        runs = { PartitionAccess::BlockRun (0, (uint32_t) (volume.size() / IRawDevice::kBlockBytes)) };
    }

    for (const PartitionAccess::BlockRun & run : runs)
    {
        size_t  offset = (size_t) run.first * IRawDevice::kBlockBytes;
        size_t  end    = offset + (size_t) run.second * IRawDevice::kBlockBytes;

        while (offset < end)
        {
            size_t  piece = std::min (end - offset, (size_t) kProgressBlocks * IRawDevice::kBlockBytes);

            hr = session.device->WriteBlocks (first + offset / IRawDevice::kBlockBytes, volume.subspan (offset, piece));
            CHR (hr);

            offset += piece;

            if (progress)
            {
                progress (start + offset, total);
            }
        }
    }

    ioWritten = start + volume.size();

    if (progress)
    {
        progress (ioWritten, total);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ClearHeader
//
//  The first blocks at `at`, unless they are inside the volumes written,
//  which end at `written`, or past the last blocks of the card.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ClearHeader (Session & session, uint64_t at, uint64_t written, uint64_t total, std::span<const Byte> header)
{
    HRESULT  hr     = S_OK;
    bool     beyond = at >= written && at + kHeaderBlocks + kGptBlocks <= total;



    BAIL_OUT_IF (!beyond, S_OK);

    hr = session.device->WriteBlocks (at, header);
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
