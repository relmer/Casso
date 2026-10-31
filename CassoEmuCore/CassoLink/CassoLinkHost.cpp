#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::CassoLinkHost
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkHost::CassoLinkHost (std::shared_ptr<IByteStream> stream, ICassoLinkFolder & folder, ClockFn clock) :
    m_stream (std::move (stream)),
    m_folder (folder),
    m_clock  (std::move (clock))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::Fill
//
//  outFilled is clear when the timeout passes first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::Fill (size_t count, DWORD timeoutMs, bool & outFilled)
{
    HRESULT  hr       = S_OK;
    Byte     chunk[4096];
    size_t   got      = 0;
    bool     closed   = false;



    outFilled = false;

    while (m_adtProServer.m_buffer.size() < count)
    {
        hr = m_stream->Read (chunk, sizeof (chunk), timeoutMs, got);
        CHR (hr);

        closed = got == 0 && m_stream->IsClosed();
        CBREx (!closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));
        BAIL_OUT_IF (got == 0, S_OK);

        m_lastHeardMs = GetTickCount64();
        m_adtProServer.m_buffer.insert (m_adtProServer.m_buffer.end(), chunk, chunk + got);
    }

    outFilled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::GetSilenceMs
//
////////////////////////////////////////////////////////////////////////////////

DWORD CassoLinkHost::GetSilenceMs() const
{
    return (DWORD) std::min<uint64_t> (GetTickCount64() - m_lastHeardMs, MAXDWORD);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::Consume
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::Consume (size_t count)
{
    m_adtProServer.m_buffer.erase (m_adtProServer.m_buffer.begin(), m_adtProServer.m_buffer.begin() + (ptrdiff_t) std::min (count, m_adtProServer.m_buffer.size()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadPacket
//
//  ERROR_TIMEOUT when the packet is not whole within the timeout, and
//  ERROR_INVALID_DATA for a damaged packet, whose bytes are dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadPacket (DWORD timeoutMs, CassoLinkPacket & outPacket)
{
    HRESULT  hr       = S_OK;
    size_t   consumed = 0;
    bool     complete = false;
    bool     filled   = false;



    for (;;)
    {
        hr = CassoLinkCodec::Decode (m_adtProServer.m_buffer, consumed, outPacket, complete);
        Consume (consumed);
        CHR (hr);
        BAIL_OUT_IF (complete, S_OK);

        hr = Fill (m_adtProServer.m_buffer.size() + 1, timeoutMs, filled);
        CHR (hr);
        CBREx (filled, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::Send
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::Send (std::span<const Byte> bytes)
{
    std::lock_guard<std::mutex>  lock (m_sendLock);



    return m_stream->Write (bytes.data(), bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendByte
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendByte (Byte value)
{
    std::lock_guard<std::mutex>  lock (m_sendLock);



    return m_stream->Write (&value, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadSectors
//
//  Each operation is sent again when the client did not take its request,
//  up to kMaxResends times; every operation can be repeated safely.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus)
{
    return Resend ([&] () { return ReadSectorsOnce (drive, track, sectors, outData, outStatus); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendSectors (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                    std::span<const Byte> data, CassoLinkStatus & outStatus, std::span<const Byte> trailer)
{
    return Resend ([&] () { return SendSectorsOnce (command, request, track, sectors, data, outStatus, trailer); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::FormatTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::FormatTrack (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap)
{
    return Resend ([&] () { return FormatTrackOnce (drive, track, volume, outStatus, gap); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::TimeDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::TimeDrive (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus)
{
    return Resend ([&] () { return TimeDriveOnce (drive, track, revolutions, outRevolutionMs, outStatus); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadSectorsOnce
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadSectorsOnce (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  request   = { drive, track, (Byte) sectors.size() };
    CassoLinkPacket    packet;
    Byte               gotTrack  = 0;
    Byte               gotSector = 0;
    size_t             received  = 0;
    bool               isData    = false;
    bool               matches   = false;



    outData.assign (sectors.size() * kSectorSize, 0);
    outStatus = {};

    request.insert (request.end(), sectors.begin(), sectors.end());
    request.insert (request.end(), { m_progressStart, m_progressEnd });

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kReadSectors, request);
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

        isData = packet.command == CassoLinkCodec::kSectorData && received < sectors.size();
        CBREx (isData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = CassoLinkCodec::ParseSectorData (packet, gotTrack, gotSector, std::span<Byte> (outData).subspan (received * kSectorSize, kSectorSize));
        CHR (hr);

        matches = gotTrack == track && gotSector == sectors[received];
        CBREx (matches, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        received++;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::span<const Byte> data, CassoLinkStatus & outStatus)
{
    std::vector<Byte>  request = { drive, track, (Byte) sectors.size() };



    request.insert (request.end(), sectors.begin(), sectors.end());

    return SendSectors (CassoLinkCodec::kWriteSectors, std::move (request), track, sectors, data, outStatus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteTrack (Byte drive, Byte track, Byte volume, std::span<const Byte> data, CassoLinkStatus & outStatus, Byte gap)
{
    std::array<Byte, 16>  physicals = {};
    const Byte            trailer[] = { gap };



    for (size_t i = 0; i < physicals.size(); i++)
    {
        physicals[i] = (Byte) i;
    }

    return SendSectors (CassoLinkCodec::kWriteTrack, { drive, track, volume }, track, physicals, data, outStatus, trailer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::Exit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::Exit()
{
    HRESULT  hr = S_OK;



    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kExit, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::TimeDriveOnce
//
//  Each marker is timed as it is read. A packet in place of a marker is the
//  Status of a track with no sector 0, which ends the markers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::TimeDriveOnce (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { drive, track, revolutions, m_progressStart, m_progressEnd };
    CassoLinkPacket  packet;
    LARGE_INTEGER    frequency = {};
    LARGE_INTEGER    first     = {};
    LARGE_INTEGER    last      = {};
    int              marks     = 0;
    bool             filled    = false;



    outRevolutionMs = 0.0;
    outStatus       = {};

    QueryPerformanceFrequency (&frequency);

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kTimeDrive, request);
    CHR (hr);

    while (marks < revolutions)
    {
        hr = Fill (1, m_adtProServer.m_operationTimeoutMs, filled);
        CHR (hr);

        CBREx (filled, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

        if (m_adtProServer.m_buffer.front() != CassoLinkCodec::kTimeMark)
        {
            break;
        }

        QueryPerformanceCounter (marks == 0 ? &first : &last);
        Consume (1);
        marks++;
    }

    hr = ReadReply (packet);
    CHR (hr);

    hr = ReadStatus (packet, outStatus);
    CHR (hr);

    if (marks >= 2 && outStatus.error == 0)
    {
        outRevolutionMs = (double) (last.QuadPart - first.QuadPart) * 1000.0 / (double) frequency.QuadPart / (double) (marks - 1);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ComputeFormatGap
//
//  A sector without its gap is 14 address bytes and 349 data bytes, 32
//  cycles each, and 5 self-sync bytes, 40 each; each gap byte is 40. The
//  margin is about 1% of a revolution, for the drive's drift and the eight
//  cycles at each page the stream crosses.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoLinkHost::ComputeFormatGap (double revolutionMs)
{
    constexpr double  kSectorCycles = 14.0 * 32.0 + 5.0 * 40.0 + 349.0 * 32.0;
    constexpr double  kMarginCycles = 2400.0;
    double            room          = revolutionMs * kCpuCyclesPerMs - kMarginCycles - 16.0 * kSectorCycles;
    int               gap           = (int) std::floor (room / (16.0 * 40.0));



    return (Byte) std::clamp (gap, 4, 40);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendSectorsOnce
//
//  A write's request, then each sector the client requests by an
//  Acknowledge, until its Status.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendSectorsOnce (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                    std::span<const Byte> data, CassoLinkStatus & outStatus, std::span<const Byte> trailer)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  encoded;
    CassoLinkPacket    packet;
    size_t             index    = 0;
    size_t             sends    = 0;
    size_t             limit    = sectors.size() * (kMaxResends + 1);
    bool               sized    = data.size() == sectors.size() * kSectorSize;
    bool               isAck    = false;
    bool               listed   = false;



    outStatus = {};

    CBRAEx (sized, E_INVALIDARG);

    request.insert (request.end(), { m_progressStart, m_progressEnd });
    request.insert (request.end(), trailer.begin(), trailer.end());

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (command, request);
    CHR (hr);

    m_writing = true;

    //  Each Acknowledge from the client has the number of the sector to
    //  send, again after a damaged copy; a Status ends the operation.
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

        isAck = packet.command == CassoLinkCodec::kAcknowledge && packet.payload.size() == 3;
        CBREx (isAck, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if (m_cancelRequested)
        {
            hr = SendPacket (CassoLinkCodec::kCancel, {});
            CHR (hr);
            continue;
        }

        index = (size_t) (std::find (sectors.begin(), sectors.end(), packet.payload[1]) - sectors.begin());
        listed = index < sectors.size();
        CBREx (listed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        sends++;
        CBREx (sends <= limit, HRESULT_FROM_WIN32 (ERROR_RETRY));

        encoded.clear();

        //  Write sectors takes each sector as its nibbles, written as they
        //  arrive; Write track takes the bytes, formatted with the track.
        //  A sector of zeros goes to Write track as its numbers alone.
        if (command == CassoLinkCodec::kWriteSectors)
        {
            hr = CassoLinkCodec::EncodeSectorNibbles (track, sectors[index], data.subspan (index * kSectorSize, kSectorSize), encoded);
        }
        else if (IsBlank (data.subspan (index * kSectorSize, kSectorSize)))
        {
            hr = CassoLinkCodec::EncodeSectorBlank (track, sectors[index], encoded);
        }
        else
        {
            hr = CassoLinkCodec::EncodeSectorData (track, sectors[index], data.subspan (index * kSectorSize, kSectorSize), encoded);
        }

        CHR (hr);

        hr = Send (encoded);
        CHR (hr);
    }

Error:
    m_writing = false;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::FormatTrackOnce
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::FormatTrackOnce (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { drive, track, volume, m_progressStart, m_progressEnd, gap };
    CassoLinkPacket  packet;



    outStatus = {};

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kFormatTrack, request);
    CHR (hr);

    hr = ReadReply (packet);
    CHR (hr);

    hr = ReadStatus (packet, outStatus);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SetSpeed
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SetSpeed (Byte control)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { control };
    CassoLinkPacket  packet;
    bool             acked     = false;



    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kSetSpeed, request);
    CHR (hr);

    hr = ReadReply (packet);
    CHR (hr);

    acked = packet.command == CassoLinkCodec::kAcknowledge && !packet.payload.empty() && packet.payload[0] == CassoLinkCodec::kAck;
    CBREx (acked, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendLabel
//
//  Anything outside printable ASCII is drawn as a question mark.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendLabel (const std::string & text)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  label;



    for (char ch : text)
    {
        if (label.size() == CassoLinkCodec::kLabelLength)
        {
            break;
        }

        label.push_back ((ch >= ' ' && ch <= '~') ? (Byte) toupper ((unsigned char) ch) : (Byte) '?');
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kLabel, label);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendLog
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendLog (int hour, int minute, const std::string & text)
{
    HRESULT            hr   = S_OK;
    std::vector<Byte>  line = { (Byte) hour, (Byte) minute };



    for (char ch : text)
    {
        if (line.size() == 2 + CassoLinkCodec::kLogLength)
        {
            break;
        }

        line.push_back ((ch >= ' ' && ch <= '~') ? (Byte) toupper ((unsigned char) ch) : (Byte) '?');
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kLog, line);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendSessionScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendSessionScreen (std::span<const Byte, 2> drives, Word sent, Word duplicates, Word bad, Byte tone, const std::string & name)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  screen = { drives[0], drives[1],
                                  (Byte) (sent & 0xFF),       (Byte) (sent >> 8),
                                  (Byte) (duplicates & 0xFF), (Byte) (duplicates >> 8),
                                  (Byte) (bad & 0xFF),        (Byte) (bad >> 8),
                                  tone };



    for (size_t i = 0; i < name.size() && i < CassoLinkCodec::kSessionNameLength; i++)
    {
        screen.push_back ((name[i] >= ' ' && name[i] <= '~') ? (Byte) toupper ((unsigned char) name[i]) : (Byte) '?');
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kSessionScreen, screen);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WaitForKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WaitForKey (Byte & outKey)
{
    return Resend ([&] () { return WaitForKeyOnce (outKey); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WaitForKeyOnce
//
//  The program sends Key with no code once a second while no key is pressed,
//  so an Apple that was reset or unplugged times out as in any operation.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WaitForKeyOnce (Byte & outKey)
{
    HRESULT          hr    = S_OK;
    CassoLinkPacket  packet;
    bool             isKey = false;



    outKey = CassoLinkCodec::kKeyNone;

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kWaitForKey, {});
    CHR (hr);

    m_awaitingKey = true;

    while (outKey == CassoLinkCodec::kKeyNone)
    {
        hr = ReadReply (packet);
        CHR (hr);

        isKey = packet.command == CassoLinkCodec::kWaitForKey && packet.payload.size() == 1;
        CBREx (isKey, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outKey = packet.payload[0];
    }

Error:
    m_awaitingKey = false;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::Cancel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::Cancel()
{
    HRESULT  hr = S_OK;



    m_cancelRequested = true;

    //  While a client writes, its 6551 has room for only the first byte;
    //  WriteSectors sends Cancel in reply to the client's next Acknowledge.
    BAIL_OUT_IF (m_writing, S_OK);

    hr = SendPacket (CassoLinkCodec::kCancel, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeReady
//
//  A client of another version is sent the current program.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeReady (const CassoLinkPacket & packet)
{
    HRESULT  hr    = S_OK;
    bool     whole = packet.payload.size() == 4 || packet.payload.size() == 9;



    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ReadClient (packet);

    if (m_client.programVersion == LinkProgram::kProgramVersion)
    {
        hr = SendPacket (CassoLinkCodec::kIdle, {});
        CHR (hr);
    }
    else
    {
        hr = LoadProgram();
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadClient
//
//  A Ready's versions and drives, and the program's ROM identification;
//  the payload is 4 or 9 bytes.
//
//  The program sent is the build for the chip the Ready indicates, whatever
//  the link's transport: a link of the wrong kind at the Apple's address,
//  such as a Casso link to an emulated Uthernet II, would otherwise load a
//  program built for another chip, and the line would go silent.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::ReadClient (const CassoLinkPacket & packet)
{
    LinkChip  chip = (packet.payload[2] & LinkProgram::kW5100ReadyFlag) != 0 ? LinkChip::W5100 : LinkChip::Acia;



    if (chip != m_programChip)
    {
        SetProgramChip (chip);
    }

    m_client.programVersion = (Word) (packet.payload[0] | (packet.payload[1] << 8));
    m_client.shimVersion    = (Byte) (packet.payload[2] & ~LinkProgram::kW5100ReadyFlag);
    m_client.driveMap       = packet.payload[3];
    m_client.hasRomId       = packet.payload.size() == 9;
    m_hasClient             = true;

    if (m_client.hasRomId)
    {
        std::copy_n (packet.payload.begin() + 4, m_client.romId.size(), m_client.romId.begin());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::AwaitReady
//
//  Waits for Ready from the current program, loading it first if the Ready
//  is from another version. Other packets, and damage between them, are
//  passed over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::AwaitReady()
{
    HRESULT          hr      = S_OK;
    CassoLinkPacket  packet;
    bool             current = false;



    m_cancelRequested = false;

    //  A canceled operation ended on the client's Ready, which is the reply
    //  this operation waits for.
    current        = m_readyPending;
    m_readyPending = false;

    while (!current)
    {
        hr = ReadPacket (m_adtProServer.m_operationTimeoutMs, packet);

        if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
        {
            continue;
        }

        CHR (hr);

        if (IsKeyReport (packet))
        {
            NoteKey (packet);
        }

        if (packet.command != CassoLinkCodec::kReady)
        {
            continue;
        }

        current = (packet.payload.size() == 4 || packet.payload.size() == 9) && (Word) (packet.payload[0] | (packet.payload[1] << 8)) == LinkProgram::kProgramVersion;

        if (current)
        {
            ReadClient (packet);
        }
        else
        {
            hr = ServeReady (packet);
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadReply
//
//  The client's next packet in an operation, past a Ready sent before the
//  operation. After Cancel, a Ready ends the operation.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadReply (CassoLinkPacket & outPacket)
{
    HRESULT  hr      = S_OK;
    int      readies = 0;
    bool     report  = false;



    do
    {
        hr = ReadPacket (m_adtProServer.m_operationTimeoutMs, outPacket);
        CHR (hr);

        if (outPacket.command == CassoLinkCodec::kReady && m_cancelRequested.exchange (false))
        {
            m_readyPending = true;
            CBREx (false, HRESULT_FROM_WIN32 (ERROR_CANCELLED));
        }

        //  One Ready can cross the request, sent as the client's wait for a
        //  reply ran out; a second is the client idle, the request lost.
        readies += (outPacket.command == CassoLinkCodec::kReady) ? 1 : 0;

        //  A key, ahead of a Ready that crossed the request.
        report = !m_awaitingKey && IsKeyReport (outPacket);

        if (report)
        {
            NoteKey (outPacket);
        }

        if (readies > 1)
        {
            m_readyPending = true;
            CBREx (false, kLostRequest);
        }
    }
    while (outPacket.command == CassoLinkCodec::kReady || report);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadStatus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadStatus (const CassoLinkPacket & packet, CassoLinkStatus & outStatus)
{
    HRESULT  hr     = S_OK;
    bool     status = packet.command == CassoLinkCodec::kStatus && packet.payload.size() == 5;



    CBREx (status, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outStatus.operation = packet.payload[0];
    outStatus.drive     = packet.payload[1];
    outStatus.track     = packet.payload[2];
    outStatus.done      = packet.payload[3];
    outStatus.error     = packet.payload[4];

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendPacket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendPacket (Byte command, std::span<const Byte> payload)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  encoded;



    hr = CassoLinkCodec::Encode (command, payload, encoded);
    CHR (hr);

    hr = Send (encoded);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::DescribeAssemblyFailure
//
//  The activity line for an Apple-side program that did not assemble: its
//  first error, or, with no error text, that it does not fit below the shim.
//  A load that fails this way ends in a timeout on the Apple's side, so this
//  line is the only record of why.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::DescribeAssemblyFailure (const std::string & errors)
{
    std::string  first = errors.substr (0, errors.find ('\n'));



    return first.empty() ? std::string ("Casso's Apple program does not fit below the shim")
                         : "Casso's Apple program did not assemble: " + first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadProgram
//
//  Load program and the program's pages, each followed by the shim's bare
//  ACK or NAK. Sent to a running program instead, Load program alone
//  returns it to the shim.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadProgram()
{
    HRESULT            hr      = S_OK;
    std::string        errors;
    std::vector<Byte>  page (kSectorSize, 0);
    std::vector<Byte>  encoded;
    Byte               count[1] = {};
    size_t             pages    = 0;
    size_t             from     = 0;
    Byte               reply    = 0;
    bool               filled   = false;
    bool               acked    = false;
    bool               running  = false;
    CassoLinkPacket    stray;



    if (m_program.empty())
    {
        hr = LinkProgram::AssembleProgram (m_program, errors, m_programChip);

        if (FAILED (hr))
        {
            //  Assembled again next time, rather than a partial image sent.
            m_program.clear();
            if (m_adtProServer.m_activity)
            {
                m_adtProServer.m_activity (DescribeAssemblyFailure (errors));
            }
        }

        CHR (hr);
    }

    pages    = (m_program.size() + kSectorSize - 1) / kSectorSize;
    count[0] = (Byte) pages;

    hr = SendPacket (CassoLinkCodec::kLoadProgram, count);
    CHR (hr);

    //  A running program goes back to the shim, whose Ready comes next.
    BAIL_OUT_IF (m_client.programVersion != 0, S_OK);

    for (size_t i = 0; i < pages; i++)
    {
        from = i * kSectorSize;

        std::fill (page.begin(), page.end(), (Byte) 0);
        std::copy_n (m_program.begin() + (ptrdiff_t) from, std::min (kSectorSize, m_program.size() - from), page.begin());

        encoded.clear();

        hr = CassoLinkCodec::EncodeSectorData ((Byte) i, (Byte) i, page, encoded);
        CHR (hr);

        acked = false;

        for (int attempt = 0; attempt <= kMaxResends && !acked; attempt++)
        {
            hr = Send (encoded);
            CHR (hr);

            do
            {
                hr = Fill (1, m_adtProServer.m_operationTimeoutMs, filled);
                CHR (hr);
                CBREx (filled, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

                reply = m_adtProServer.m_buffer[0];
                running = reply == CassoLinkCodec::kStart && i + 1 == pages;

                //  A packet in place of the last page's ACK is the program's
                //  Ready: the program is running, and its Ready is left for
                //  the next operation to read. A packet in place of another
                //  page's, such as a second Ready from the shim, is read whole
                //  and dropped, so that no byte of it is taken for the reply:
                //  a Ready's check byte can be an ACK's.
                if (reply == CassoLinkCodec::kStart && !running)
                {
                    hr = ReadPacket (m_adtProServer.m_operationTimeoutMs, stray);
                    hr = (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)) ? S_OK : hr;
                    CHR (hr);

                    reply = 0;
                }
                else if (!running)
                {
                    Consume (1);
                }
            }
            while (!running && reply != CassoLinkCodec::kAck && reply != CassoLinkCodec::kNak);

            acked = running || reply == CassoLinkCodec::kAck;
        }

        CBREx (acked, HRESULT_FROM_WIN32 (ERROR_RETRY));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::GetModelName
//
//  From the identification bytes Apple's technical notes list for each
//  model.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkClient::GetModelName() const
{
    Byte  family  = romId[0];
    Byte  version = romId[1];
    Byte  release = romId[2];



    if (!hasRomId)
    {
        return std::wstring();
    }

    if (family == 0x38)
    {
        return (romId[3] == 0xAD) ? L"Apple ][" : L"Apple ][+";
    }

    if (family != 0x06)
    {
        return L"Apple II";
    }

    if (version == 0xEA)
    {
        return L"Apple //e";
    }

    if (version == 0xE0)
    {
        return (romId[4] != 0) ? L"Apple IIgs" : L"Apple //e enhanced";
    }

    switch (release)
    {
        case 0xFF:  return L"Apple //c";
        case 0x00:  return L"Apple //c (ROM 0)";
        case 0x03:  return L"Apple //c (ROM 3)";
        case 0x04:  return L"Apple //c (ROM 4)";
        case 0x05:  return L"Apple //c Plus";
        default:    return L"Apple //c";
    }
}
