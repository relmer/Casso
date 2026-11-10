#include "Pch.h"

#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::Connect
//
//  A drive of more blocks than a block number counts is cut off there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::Connect (PhysicalDrive & outDrive)
{
    HRESULT   hr        = S_OK;
    Byte      fixed[13] = {};
    uint64_t  blocks    = 0;



    outDrive     = PhysicalDrive();
    m_blockCount = 0;

    hr = SendHeader (RawDeviceProtocol::kInfo, 0, 0, {});
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

    blocks       = outDrive.bytes / kBlockBytes;
    m_blockCount = (uint32_t) std::min<uint64_t> (blocks, UINT32_MAX);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::ReadBlocks
//
//  The drive's blocks, with each block written since the last Commit in
//  place of the drive's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT   hr     = S_OK;
    size_t    bytes  = (size_t) count * kBlockBytes;
    bool      inside = (uint64_t) first + count <= m_blockCount;
    bool      sized  = outBytes.size() >= bytes;
    uint32_t  done   = 0;



    CBRAEx (inside, E_INVALIDARG);
    CBRAEx (sized,  E_INVALIDARG);

    while (done < count)
    {
        uint32_t  run = std::min (count - done, RawDeviceProtocol::kMaxBlocks);

        hr = SendHeader (RawDeviceProtocol::kRead, (uint64_t) first + done, run, {});
        CHR (hr);

        hr = ReadResult();
        CHR (hr);

        hr = RawDeviceProtocol::ReadExact (m_stream, outBytes.data() + (size_t) done * kBlockBytes, (size_t) run * kBlockBytes, RawDeviceProtocol::kReplyTimeoutMs);
        CHR (hr);

        done += run;
    }

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
//  block the drive has written is no longer pending, so a Commit after a
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

        hr = SendHeader (RawDeviceProtocol::kWrite, first, count, blocks);
        CHR (hr);

        hr = ReadResult();
        CHR (hr);

        m_pending.erase (begin, end);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceClient::SendHeader
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::SendHeader (Byte operation, uint64_t first, uint32_t count, std::span<const Byte> blocks)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  bytes;



    bytes.push_back (operation);
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
//  RawDeviceClient::ReadResult
//
//  Returns the HRESULT a reply begins with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceClient::ReadResult()
{
    HRESULT  hr       = S_OK;
    Byte     bytes[4] = {};



    hr = RawDeviceProtocol::ReadExact (m_stream, bytes, sizeof (bytes), RawDeviceProtocol::kReplyTimeoutMs);
    CHR (hr);

    hr = (HRESULT) RawDeviceProtocol::GetLong (bytes);
    CHR (hr);

Error:
    return hr;
}
