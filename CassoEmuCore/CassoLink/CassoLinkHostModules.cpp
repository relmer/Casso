#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/RawWritePass.h"
#include "CassoLink/Session/RawWriteStream.h"
#include "Seams/IByteStream.h"
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

        hr = LoadChunk (chunk);
        CHR (hr);
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
//  CassoLinkHost::LoadChunk
//
//  One chunk of a module, sent again by itself after the line is
//  recovered when the client doesn't take it or its reply is damaged, up to
//  kMaxResends times. The chunk has its offset, so a chunk sent twice is
//  stored once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadChunk (const std::vector<Byte> & chunk)
{
    HRESULT          hr    = S_OK;
    CassoLinkPacket  reply;
    bool             valid = false;



    for (int sent = 0; sent <= kMaxResends; sent++)
    {
        if (sent > 0)
        {
            hr = RecoverLine();
            CHR (hr);
        }

        hr = AwaitRequest();
        CHR (hr);

        hr = SendPacket (CassoLinkCodec::kLoadModule, chunk);
        CHR (hr);

        hr = ReadReply (reply);
        valid = SUCCEEDED (hr) && reply.command == CassoLinkCodec::kAcknowledge && reply.payload.size() == 3
             && reply.payload[0] == CassoLinkCodec::kAck && reply.payload[1] == chunk[0] && reply.payload[2] == chunk[1];

        hr = valid ? S_OK : FAILED (hr) ? hr : HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
        BAIL_OUT_IF (valid || (hr != kLostRequest && hr != HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)), hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::RecoverLine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::RecoverLine()
{
    HRESULT      hr     = S_OK;
    std::string  errors;



    hr = m_stream->ResetLine (errors);
    CHR (hr);

    hr = DiscardReceived();
    CHR (hr);

    if (m_adtProServer.m_activity)
    {
        m_adtProServer.m_activity (errors.empty() ? std::string ("The link was reset after a missed reply. The serial port didn't report an error.")
                                                  : "The link was reset after a missed reply. The serial port reported: " + errors + ".");
    }

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

HRESULT CassoLinkHost::CaptureQuarterTrack (Byte drive, Byte position, Byte delay, Byte startPercent, Byte endPercent, Byte syncPosition, std::span<const Byte> marker,
                                            std::vector<Byte> & outBytes, CassoLinkStatus & outStatus, CaptureTiming & outTiming)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  request   = { startPercent, endPercent, drive, position, delay, syncPosition, (Byte) marker.size() };
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

    request.insert (request.end(), marker.begin(), marker.end());

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
//  CassoLinkHost::GetTrackPercents
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 2> CassoLinkHost::GetTrackPercents (Byte track)
{
    return { (Byte) (track * 100 / kNibbleTracks), (Byte) ((track + 1) * 100 / kNibbleTracks) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::GetStepPercents
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 2> CassoLinkHost::GetStepPercents (size_t index, size_t count)
{
    if (count == 0)
    {
        return { 100, 100 };
    }

    return { (Byte) (index * 100 / count), (Byte) ((index + 1) * 100 / count) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteNibbleTrack
//
//  The Apple requests the stream with an Acknowledge, and requests it again
//  with a NAK when it arrived damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteNibbleTrack (Byte drive, Byte track, std::span<const Byte> stream, CassoLinkStatus & outStatus)
{
    HRESULT              hr       = S_OK;
    std::array<Byte, 2>  percents = GetTrackPercents (track);
    std::vector<Byte>    request  = { drive, track, (Byte) (stream.size() & 0xFF), (Byte) (stream.size() >> 8), percents[0], percents[1] };
    bool                 valid    = !stream.empty() && stream.size() <= CassoLinkCodec::kMaxNibbleStream;



    outStatus = {};
    CBRAEx (valid, E_INVALIDARG);

    hr = SendStreamRequest (CassoLinkCodec::kWriteNibbleTrack, request, stream, std::format ("track {}'s nibbles", track), outStatus);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteRawTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteRawTrack (Byte drive, Byte position, const RawWriteStream & stream, std::array<Byte, 2> percents, CassoLinkStatus & outStatus)
{
    HRESULT              hr       = S_OK;
    std::vector<Byte>    moduleStream;
    size_t               count    = 0;
    std::vector<Byte>    request;
    bool                 valid    = position < CassoLinkCodec::kQuarterTracks && stream.BuildModuleStream (moduleStream, count);



    outStatus = {};
    CBRAEx (valid, E_INVALIDARG);

    request = { drive, position, (Byte) (count & 0xFF), (Byte) (count >> 8), percents[0], percents[1] };

    hr = SendStreamRequest (CassoLinkCodec::kWriteRawTrack, request, moduleStream, std::format ("track {}'s bits", position / 4.0), outStatus, kRawPieceBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteRawPass
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteRawPass (Byte drive, const RawWritePass & pass, std::array<Byte, 2> percents, CassoLinkStatus & outStatus)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  request;
    bool               valid   = pass.position >= 0 && pass.position < CassoLinkCodec::kQuarterTracks && pass.count > 0 && pass.stream.size() == 2 * RawWriteStream::kMaxBytes;



    outStatus = {};
    CBRAEx (valid, E_INVALIDARG);

    request = { drive, (Byte) pass.position, (Byte) (pass.count & 0xFF), (Byte) (pass.count >> 8), percents[0], percents[1] };

    hr = SendStreamRequest (CassoLinkCodec::kWriteRawTrack, request, pass.stream, std::format ("the arcs from track {}", pass.position / 4.0), outStatus, kRawPieceBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendStreamRequest
//
//  The Apple requests the stream with an Acknowledge, and requests it again
//  with a NAK when it arrived damaged. With a piece size, the stream is sent
//  in pieces of that many bytes: the second byte of each Acknowledge or NAK
//  is the piece the Apple requests.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendStreamRequest (Byte command, std::span<const Byte> request, std::span<const Byte> stream, const std::string & item, CassoLinkStatus & outStatus, size_t pieceBytes)
{
    HRESULT            hr      = S_OK;
    CassoLinkPacket    packet;
    std::vector<Byte>  payload;
    size_t             piece   = pieceBytes == 0 ? stream.size() : pieceBytes;
    size_t             start   = 0;
    Word               crc     = 0;
    bool               valid   = false;



    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (command, request);
    CHR (hr);

    for (;;)
    {
        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        valid = packet.command == CassoLinkCodec::kAcknowledge && packet.payload.size() == 3;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        ReportNak (packet, item);

        start = pieceBytes == 0 ? 0 : packet.payload[1] * piece;
        valid = start + piece <= stream.size();
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        crc = CassoLinkCodec::ComputeCrc (stream.subspan (start, piece));
        payload.assign (stream.begin() + start, stream.begin() + start + piece);
        payload.push_back ((Byte) (crc & 0xFF));
        payload.push_back ((Byte) (crc >> 8));

        hr = SendPacket (CassoLinkCodec::kNibbleData, payload);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadThirteenTrack
//
//  A sector whose packet fails its CRC is left out of outFields, so the
//  caller requests it again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadThirteenTrack (Byte drive, Byte track, Word requested, std::vector<std::vector<Byte>> & outFields, Word & outUnwritten, CassoLinkStatus & outStatus)
{
    HRESULT              hr       = S_OK;
    CassoLinkPacket      packet;
    std::array<Byte, 2>  percents = GetTrackPercents (track);
    std::vector<Byte>    request  = { drive, track, (Byte) (requested & 0xFF), (Byte) (requested >> 8), percents[0], percents[1] };



    outStatus    = {};
    outUnwritten = 0;
    outFields.assign (CassoLinkCodec::kThirteenSectors, std::vector<Byte>());

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kReadThirteenTrack, request);
    CHR (hr);

    for (;;)
    {
        Byte  sector = 0;
        bool  valid  = false;

        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        valid = packet.command == CassoLinkCodec::kThirteenData && !packet.payload.empty();
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        sector = (Byte) (packet.payload[0] & ~CassoLinkCodec::kNoDataField);
        valid  = sector < CassoLinkCodec::kThirteenSectors;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if ((packet.payload[0] & CassoLinkCodec::kNoDataField) != 0)
        {
            valid = packet.payload.size() == 1;
            CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            outUnwritten = (Word) (outUnwritten | (1u << sector));
        }
        else
        {
            std::span<const Byte>  field;
            Word                   crc = 0;

            valid = packet.payload.size() == 1 + CassoLinkCodec::kThirteenField + 2;
            CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            field = std::span<const Byte> (packet.payload).subspan (1, CassoLinkCodec::kThirteenField);
            crc   = (Word) (packet.payload[1 + CassoLinkCodec::kThirteenField] | (packet.payload[2 + CassoLinkCodec::kThirteenField] << 8));

            if (CassoLinkCodec::ComputeCrc (field) == crc)
            {
                outFields[sector].assign (field.begin(), field.end());
            }
        }
    }

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
//  CassoLinkHost::BuildRawWriteModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::BuildRawWriteModule (std::vector<Byte> & outModule) const
{
    return LinkProgram::BuildRawWriteModule (outModule, m_programChip, m_faultProgram);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::BuildThirteenSectorModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::BuildThirteenSectorModule (std::vector<Byte> & outModule) const
{
    return LinkProgram::BuildThirteenSectorModule (outModule, m_programChip, m_faultProgram);
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
