#include "Pch.h"

#include "AppleLink/AppleLinkSearch.h"
#include "AppleLink/AppleLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSearch::Find
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSearch::Find (IByteStreamFactory & factory, const ComSettings & settings, std::span<const DWORD> speeds,
                               DWORD replyTimeoutMs, std::wstring & outPort)
{
    HRESULT                    hr       = S_OK;
    std::vector<std::wstring>  ports;
    ComSettings                opening  = settings;
    bool                       replied  = false;



    outPort.clear();

    hr = factory.ListComPorts (ports);
    CHR (hr);

    for (const std::wstring & port : ports)
    {
        for (DWORD speed : speeds)
        {
            std::unique_ptr<IByteStream>  stream;
            HRESULT                       opened = S_OK;

            opening.baud = speed;
            opened       = factory.OpenComPort (port, opening, stream);

            //  A port another program has open is skipped at every speed.
            if (FAILED (opened))
            {
                break;
            }

            hr = Probe (*stream, replyTimeoutMs, replied);
            stream->Close();
            IGNORE_RETURN_VALUE (hr, S_OK);

            if (replied)
            {
                break;
            }
        }

        if (replied)
        {
            outPort = port;
            break;
        }
    }

    hr = replied ? S_OK : HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSearch::Probe
//
//  Idle, then whatever arrives until a Ready packet decodes or the time is
//  up. Bytes that do not decode are dropped one at a time, so a device's own
//  output ahead of the reply does not hide it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSearch::Probe (IByteStream & stream, DWORD replyTimeoutMs, bool & outReplied)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  idle;
    std::vector<Byte>  received;
    Byte               chunk[256];
    size_t             got      = 0;
    auto               until    = std::chrono::steady_clock::now() + std::chrono::milliseconds (replyTimeoutMs);
    AppleLinkPacket    packet;



    outReplied = false;

    hr = AppleLinkCodec::Encode (AppleLinkCodec::kIdle, {}, idle);
    CHR (hr);

    hr = stream.Write (idle.data(), idle.size());
    CHR (hr);

    while (!outReplied && std::chrono::steady_clock::now() < until)
    {
        DWORD  left = (DWORD) std::chrono::duration_cast<std::chrono::milliseconds> (until - std::chrono::steady_clock::now()).count();

        hr = stream.Read (chunk, sizeof (chunk), left, got);
        CHR (hr);

        received.insert (received.end(), chunk, chunk + got);

        while (!received.empty() && !outReplied)
        {
            size_t   consumed = 0;
            bool     complete = false;
            HRESULT  decoded  = AppleLinkCodec::Decode (received, consumed, packet, complete);

            if (FAILED (decoded))
            {
                received.erase (received.begin());
                continue;
            }

            if (!complete)
            {
                break;
            }

            received.erase (received.begin(), received.begin() + (ptrdiff_t) consumed);
            outReplied = packet.command == AppleLinkCodec::kReady;
        }
    }

Error:
    return hr;
}
