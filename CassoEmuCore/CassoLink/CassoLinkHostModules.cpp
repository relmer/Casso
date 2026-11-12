#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::QueryModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::QueryModule (Byte & outId, Byte & outVersion)
{
    HRESULT          hr    = S_OK;
    CassoLinkPacket  reply;
    bool             valid = false;



    outId      = 0;
    outVersion = 0;

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kLoadModule, {});
    CHR (hr);

    hr = ReadReply (reply);
    CHR (hr);

    valid = reply.command == CassoLinkCodec::kLoadModule && reply.payload.size() == 2;
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outId      = reply.payload[0];
    outVersion = reply.payload[1];

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadModule
//
//  Each chunk is acknowledged with its offset; a NAK in place of the ACK is the
//  module's not fitting. The check's reply has the module's id, 0 when it
//  failed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadModule (std::span<const Byte> module)
{
    HRESULT          hr      = S_OK;
    CassoLinkPacket  reply;
    bool             valid   = false;
    const Byte       check[] = { 0xFF, 0xFF };
    bool             sized   = module.size() > LinkProgram::kModuleCrcStart;



    CBRAEx (sized, E_INVALIDARG);

    for (size_t at = 0; at < module.size(); at += CassoLinkCodec::kModuleChunk)
    {
        size_t             count = std::min (CassoLinkCodec::kModuleChunk, module.size() - at);
        std::vector<Byte>  chunk = { (Byte) (at & 0xFF), (Byte) (at >> 8) };

        chunk.insert (chunk.end(), module.begin() + (ptrdiff_t) at, module.begin() + (ptrdiff_t) (at + count));

        hr = AwaitReady();
        CHR (hr);

        hr = SendPacket (CassoLinkCodec::kLoadModule, chunk);
        CHR (hr);

        hr = ReadReply (reply);
        CHR (hr);

        valid = reply.command == CassoLinkCodec::kAcknowledge && reply.payload.size() == 3
             && reply.payload[0] == CassoLinkCodec::kAck && reply.payload[1] == chunk[0] && reply.payload[2] == chunk[1];
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kLoadModule, check);
    CHR (hr);

    hr = ReadReply (reply);
    CHR (hr);

    valid = reply.command == CassoLinkCodec::kLoadModule && reply.payload.size() == 2
         && reply.payload[0] == module[2] && reply.payload[1] == module[3];
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_CRC));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::EnsureModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::EnsureModule (std::span<const Byte> module)
{
    HRESULT  hr      = S_OK;
    Byte     id      = 0;
    Byte     version = 0;
    bool     sized   = module.size() > LinkProgram::kModuleCrcStart;



    CBRAEx (sized, E_INVALIDARG);

    hr = QueryModule (id, version);
    CHR (hr);

    if (id != module[2] || version != module[3])
    {
        hr = LoadModule (module);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::CaptureQuarterTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::CaptureQuarterTrack (Byte drive, Byte position, Byte delay, Byte startPercent, Byte endPercent, std::vector<Byte> & outBytes,
                                            CassoLinkStatus & outStatus, CaptureTiming & outTiming)
{
    HRESULT            hr        = S_OK;
    const Byte         request[] = { startPercent, endPercent, drive, position, delay };
    CassoLinkPacket    packet;
    std::vector<Byte>  encoded;
    LARGE_INTEGER      frequency = {};
    LARGE_INTEGER      sent      = {};
    LARGE_INTEGER      first     = {};
    LARGE_INTEGER      last      = {};
    size_t             sequence  = 0;
    bool               decoded   = false;



    outStatus = {};
    outTiming = {};
    outBytes.clear();

    QueryPerformanceFrequency (&frequency);

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kCapture, request);
    CHR (hr);

    QueryPerformanceCounter (&sent);

    for (;;)
    {
        std::span<const Byte>  data;
        Word                   crc   = 0;
        bool                   valid = false;

        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        valid = packet.command == CassoLinkCodec::kCaptureData && packet.payload.size() == 1 + CassoLinkCodec::kPageSize + 2
             && packet.payload[0] == (Byte) sequence;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        data = std::span<const Byte> (packet.payload).subspan (1, CassoLinkCodec::kPageSize);
        crc  = (Word) (packet.payload[1 + CassoLinkCodec::kPageSize] | (packet.payload[2 + CassoLinkCodec::kPageSize] << 8));

        valid = CassoLinkCodec::ComputeCrc (data) == crc;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_CRC));

        if (sequence == 0)
        {
            QueryPerformanceCounter (&first);
        }

        encoded.insert (encoded.end(), data.begin(), data.end());
        sequence++;
    }

    QueryPerformanceCounter (&last);

    BAIL_OUT_IF (outStatus.error != 0, S_OK);

    decoded = CaptureStream::Decode (encoded, CassoLinkCodec::kCapturePages * CassoLinkCodec::kPageSize, outBytes);
    CBREx (decoded, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outTiming.diskMs    = (double) (first.QuadPart - sent.QuadPart)  * 1000.0 / (double) frequency.QuadPart;
    outTiming.lineMs    = (double) (last.QuadPart  - first.QuadPart) * 1000.0 / (double) frequency.QuadPart;
    outTiming.sentBytes = encoded.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::BuildCaptureModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::BuildCaptureModule (std::vector<Byte> & outModule) const
{
    return LinkProgram::BuildCaptureModule (outModule, m_programChip, m_faultProgram);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::RequestModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::RequestModule (Byte command, std::span<const Byte> payload, CassoLinkPacket & outReply)
{
    HRESULT  hr = S_OK;



    outReply = CassoLinkPacket();

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (command, payload);
    CHR (hr);

    hr = ReadReply (outReply);
    CHR (hr);

Error:
    return hr;
}
