#include "Pch.h"

#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::Run
//
//  A drive that fails to open, or is not removable, is reported in the reply
//  to each request, so the window has the reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::Run (int driveNumber)
{
    HRESULT  hr                                      = S_OK;
    HRESULT  hrRead                                  = S_OK;
    Byte     header[RawDeviceProtocol::kHeaderBytes] = {};
    bool     valid                                   = true;



    m_opened = m_device.Open (driveNumber, m_drive);

    if (SUCCEEDED (m_opened) && !m_drive.removable)
    {
        m_device.Close();
        m_opened = E_ACCESSDENIED;
    }

    while (valid)
    {
        uint64_t  first = 0;
        uint32_t  count = 0;

        hrRead = RawDeviceProtocol::ReadExact (m_stream, header, sizeof (header), INFINITE);
        BAIL_OUT_IF (FAILED (hrRead), S_OK);

        first = RawDeviceProtocol::GetQuad (header + 1);
        count = RawDeviceProtocol::GetLong (header + 9);

        switch (header[0])
        {
            case RawDeviceProtocol::kInfo:
                hr = ServeInfo();
                break;

            case RawDeviceProtocol::kRead:
                hr = ServeRead (first, count);
                break;

            case RawDeviceProtocol::kWrite:
                hr = ServeWrite (first, count);
                break;

            default:
                hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
                break;
        }

        valid = SUCCEEDED (hr);
    }

Error:
    if (SUCCEEDED (m_opened))
    {
        m_device.Close();
    }

    m_stream.Close();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeInfo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeInfo()
{
    std::vector<Byte>  payload;



    if (SUCCEEDED (m_opened))
    {
        RawDeviceProtocol::AppendQuad (payload, m_drive.bytes);
        payload.push_back (m_drive.removable ? 1 : 0);
        RawDeviceProtocol::AppendLong (payload, (uint32_t) m_drive.number);
        RawDeviceProtocol::AppendText (payload, m_drive.model);
        RawDeviceProtocol::AppendText (payload, m_drive.bus);
    }

    return Reply (m_opened, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer::ServeRead
//
//  A count of zero, or over the protocol's limit, ends the session.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeRead (uint64_t first, uint32_t count)
{
    HRESULT            hr     = S_OK;
    HRESULT            result = S_OK;
    bool               sized  = count > 0 && count <= RawDeviceProtocol::kMaxBlocks;
    std::vector<Byte>  blocks;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    result = CheckRange (first, count);

    if (SUCCEEDED (result))
    {
        blocks.resize ((size_t) count * IRawDevice::kBlockBytes);
        result = m_device.ReadBlocks (first, count, blocks);
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
//  The blocks are read from the stream before the range is checked, so the
//  next request starts at its header.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceServer::ServeWrite (uint64_t first, uint32_t count)
{
    HRESULT            hr     = S_OK;
    HRESULT            result = S_OK;
    bool               sized  = count > 0 && count <= RawDeviceProtocol::kMaxBlocks;
    std::vector<Byte>  blocks;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    blocks.resize ((size_t) count * IRawDevice::kBlockBytes);

    hr = RawDeviceProtocol::ReadExact (m_stream, blocks.data(), blocks.size(), RawDeviceProtocol::kReplyTimeoutMs);
    CHR (hr);

    result = CheckRange (first, count);

    if (SUCCEEDED (result))
    {
        result = m_device.WriteBlocks (first, blocks);
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

HRESULT RawDeviceServer::CheckRange (uint64_t first, uint32_t count) const
{
    HRESULT   hr     = S_OK;
    uint64_t  blocks = m_drive.bytes / IRawDevice::kBlockBytes;
    bool      inside = first <= blocks && count <= blocks - first;



    CHR (m_opened);
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

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
