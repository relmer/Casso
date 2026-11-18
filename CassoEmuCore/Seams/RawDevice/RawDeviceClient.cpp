#include "Pch.h"

#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::Open (int number, PhysicalDrive & outDrive)
{
    HRESULT                      hr        = S_OK;
    Byte                         fixed[13] = {};
    std::lock_guard<std::mutex>  lock (m_mutex);



    outDrive = PhysicalDrive();

    hr = SendHeader (RawDeviceProtocol::kOpen, number, 0, 0, {});
    CHR (hr);

    hr = ReadResult();
    CHR (hr);

    hr = RawDeviceProtocol::ReadExact (m_stream, fixed, sizeof (fixed), RawDeviceProtocol::kReplyTimeoutMs);
    CHR (hr);

    outDrive.bytes     = RawDeviceProtocol::GetQuad (fixed);
    outDrive.removable = fixed[8] != 0;
    outDrive.number    = (int) RawDeviceProtocol::GetLong (fixed + 9);

    hr = RawDeviceProtocol::ReadText (m_stream, outDrive.model);
    CHR (hr);

    hr = RawDeviceProtocol::ReadText (m_stream, outDrive.bus);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::Close
//
//  The broker doesn't reply to a close.
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceConnection::Close (int number)
{
    HRESULT                      hrSend = S_OK;
    std::lock_guard<std::mutex>  lock (m_mutex);



    hrSend = SendHeader (RawDeviceProtocol::kClose, number, 0, 0, {});
    IGNORE_RETURN_VALUE (hrSend, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::Read
//
//  Runs longer than the protocol's limit are sent as several requests.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::Read (int number, uint64_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT                      hr    = S_OK;
    uint32_t                     done  = 0;
    bool                         sized = outBytes.size() >= (size_t) count * IRawDevice::kBlockBytes;
    std::lock_guard<std::mutex>  lock (m_mutex);



    CBRAEx (sized, E_INVALIDARG);

    while (done < count)
    {
        uint32_t  run = std::min (count - done, RawDeviceProtocol::kMaxBlocks);

        hr = SendHeader (RawDeviceProtocol::kRead, number, first + done, run, {});
        CHR (hr);

        hr = ReadResult();
        CHR (hr);

        hr = RawDeviceProtocol::ReadExact (m_stream, outBytes.data() + (size_t) done * IRawDevice::kBlockBytes, (size_t) run * IRawDevice::kBlockBytes, RawDeviceProtocol::kReplyTimeoutMs);
        CHR (hr);

        done += run;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::Write (int number, uint64_t first, uint32_t count, std::span<const Byte> blocks)
{
    HRESULT                      hr     = S_OK;
    bool                         sized  = blocks.size() == (size_t) count * IRawDevice::kBlockBytes && count <= RawDeviceProtocol::kMaxBlocks;
    std::lock_guard<std::mutex>  lock (m_mutex);



    CBRAEx (sized, E_INVALIDARG);

    hr = SendHeader (RawDeviceProtocol::kWrite, number, first, count, blocks);
    CHR (hr);

    hr = ReadResult();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::Initialize (int number, uint64_t layout, const WriteProgress & progress)
{
    HRESULT        hr     = S_OK;
    PhysicalDrive  drive;
    bool           opened = false;



    hr = Open (number, drive);
    CHR (hr);

    opened = true;

    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        hr = SendHeader (RawDeviceProtocol::kInitialize, number, layout, 0, {});
        CHR (hr);

        //  Replies of S_FALSE are progress, each with the bytes written and
        //  the bytes to write; the last reply ends the request.
        hr = ReadResult (RawDeviceProtocol::kInitializeTimeoutMs);

        while (hr == S_FALSE)
        {
            Byte  counts[16] = {};

            hr = RawDeviceProtocol::ReadExact (m_stream, counts, sizeof (counts), RawDeviceProtocol::kInitializeTimeoutMs);
            CHR (hr);

            if (progress)
            {
                progress (RawDeviceProtocol::GetQuad (counts), RawDeviceProtocol::GetQuad (counts + 8));
            }

            hr = ReadResult (RawDeviceProtocol::kInitializeTimeoutMs);
        }

        CHR (hr);
    }

Error:
    if (opened)
    {
        Close (number);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::SendHeader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::SendHeader (Byte operation, int number, uint64_t first, uint32_t count, std::span<const Byte> blocks)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  bytes;



    bytes.push_back (operation);
    RawDeviceProtocol::AppendLong (bytes, (uint32_t) number);
    RawDeviceProtocol::AppendQuad (bytes, first);
    RawDeviceProtocol::AppendLong (bytes, count);
    bytes.insert (bytes.end(), blocks.begin(), blocks.end());

    hr = m_stream.Write (bytes.data(), bytes.size());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceConnection::ReadResult
//
//  Returns the HRESULT a reply begins with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceConnection::ReadResult (DWORD timeoutMs)
{
    HRESULT  hr       = S_OK;
    Byte     bytes[4] = {};



    hr = RawDeviceProtocol::ReadExact (m_stream, bytes, sizeof (bytes), timeoutMs);
    CHR (hr);

    hr = (HRESULT) RawDeviceProtocol::GetLong (bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::~RawDeviceClient
//
////////////////////////////////////////////////////////////////////////////////

RawDeviceClient::~RawDeviceClient()
{
    if (m_number >= 0)
    {
        m_connection.Close (m_number);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::Connect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::Connect (int number, PhysicalDrive & outDrive)
{
    HRESULT   hr     = S_OK;
    uint64_t  blocks = 0;



    m_blockCount = 0;

    hr = m_connection.Open (number, outDrive);
    CHR (hr);

    m_number     = number;
    blocks       = outDrive.bytes / kBlockBytes;
    m_blockCount = (uint32_t) std::min<uint64_t> (blocks, UINT32_MAX);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::ReadBlocks
//
//  The card's blocks, with each block written since the last Commit in
//  place of the card's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    bool     inside = (uint64_t) first + count <= m_blockCount;
    bool     sized  = outBytes.size() >= (size_t) count * kBlockBytes;



    CBRAEx (inside, E_INVALIDARG);
    CBRAEx (sized,  E_INVALIDARG);

    hr = m_connection.Read (m_number, first, count, outBytes);
    CHR (hr);

    for (auto it = m_pending.lower_bound (first); it != m_pending.end() && it->first - first < count; ++it)
    {
        std::copy (it->second.begin(), it->second.end(), outBytes.begin() + (ptrdiff_t) ((size_t) (it->first - first) * kBlockBytes));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::WriteBlocks
//
//  Whole blocks only. Nothing is sent until Commit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT   hr     = S_OK;
    uint64_t  count  = bytes.size() / kBlockBytes;
    bool      whole  = bytes.size() % kBlockBytes == 0;
    bool      inside = (uint64_t) first + count <= m_blockCount;



    CBRAEx (whole,  E_INVALIDARG);
    CBRAEx (inside, E_INVALIDARG);

    for (uint32_t i = 0; i < count; i++)
    {
        auto  start = bytes.begin() + (ptrdiff_t) ((size_t) i * kBlockBytes);

        m_pending[first + i].assign (start, start + (ptrdiff_t) kBlockBytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::Commit
//
//  Consecutive blocks are sent as one write, up to the protocol's limit. A
//  block the card has written is no longer pending, so a Commit after a
//  failure sends the rest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::Commit()
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  blocks;



    while (!m_pending.empty())
    {
        auto      begin = m_pending.begin();
        auto      end   = begin;
        uint32_t  first = begin->first;
        uint32_t  count = 0;

        blocks.clear();

        while (end != m_pending.end() && end->first == first + count && count < RawDeviceProtocol::kMaxBlocks)
        {
            blocks.insert (blocks.end(), end->second.begin(), end->second.end());
            ++end;
            count++;
        }

        hr = m_connection.Write (m_number, first, count, blocks);
        CHR (hr);

        m_pending.erase (begin, end);
    }

Error:
    return hr;
}
