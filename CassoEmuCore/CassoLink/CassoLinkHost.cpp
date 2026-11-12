#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/FaultInjectingByteStream.h"
#include "CassoLink/CassoLinkPacketText.h"
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
//  CassoLinkHost::DiscardReceived
//
//  The bytes received and not yet read, and those the line has now, up to
//  kDiscardReads reads of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::DiscardReceived()
{
    HRESULT  hr  = S_OK;
    Byte     chunk[4096];
    size_t   got = sizeof (chunk);



    m_adtProServer.m_buffer.clear();

    for (int read = 0; read < kDiscardReads && got > 0; read++)
    {
        hr = m_stream->Read (chunk, sizeof (chunk), 0, got);
        CHR (hr);
    }

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

        if (complete)
        {
            m_lastPacketMs = GetTickCount64();
            TraceReceived (outPacket);
        }

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



    m_lastPacketMs = GetTickCount64();

    if (m_pcFault.IsArmed())
    {
        return FaultInjectingByteStream::WriteWithFault (*m_stream, m_pcFault, bytes, m_adtProServer.m_activity);
    }

    return m_stream->Write (bytes.data(), bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendByte
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendByte (Byte value)
{
    return Send (std::span<const Byte> (&value, 1));
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
    std::vector<bool>  read;



    return ReadSectors (drive, track, sectors, outData, read, outStatus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead, CassoLinkStatus & outStatus)
{
    return Resend ([&] () { return ReadSectorsOnce (drive, track, sectors, outData, outRead, outStatus); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadCrcs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadCrcs (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead, CassoLinkStatus & outStatus)
{
    return Resend ([&] () { return ReadCrcsOnce (drive, track, sectors, outCrcs, outRead, outStatus); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadCrcsOnce
//
//  Three bytes a sector: its CRC, low byte first, and whether it was read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadCrcsOnce (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead, CassoLinkStatus & outStatus)
{
    constexpr size_t   kEntryBytes = 3;
    HRESULT            hr          = S_OK;
    std::vector<Byte>  request     = { GetRequestDrive (drive), track, (Byte) sectors.size() };
    CassoLinkPacket    packet;
    bool               valid       = false;



    outCrcs.assign (sectors.size(), 0);
    outRead.assign (sectors.size(), false);
    outStatus = {};

    request.insert (request.end(), sectors.begin(), sectors.end());
    request.insert (request.end(), { m_progressStart, m_progressEnd });

    hr = AwaitRequest();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kReadCrcs, request);
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

        valid = packet.command == CassoLinkCodec::kSectorCrcs && packet.payload.size() == sectors.size() * kEntryBytes;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        for (size_t i = 0; i < sectors.size(); i++)
        {
            outCrcs[i] = (Word) (packet.payload[i * kEntryBytes] | (packet.payload[i * kEntryBytes + 1] << 8));
            outRead[i] = packet.payload[i * kEntryBytes + 2] != 0;
        }
    }

Error:
    return hr;
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

HRESULT CassoLinkHost::ReadSectorsOnce (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead, CassoLinkStatus & outStatus)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  request   = { GetRequestDrive (drive), track, (Byte) sectors.size() };
    CassoLinkPacket    packet;
    std::vector<Byte>  one (kSectorSize);
    Byte               gotTrack  = 0;
    Byte               gotSector = 0;
    size_t             next      = 0;
    bool               isData    = false;
    bool               matches   = false;



    outData.assign (sectors.size() * kSectorSize, 0);
    outRead.assign (sectors.size(), false);
    outStatus = {};

    request.insert (request.end(), sectors.begin(), sectors.end());
    request.insert (request.end(), { m_progressStart, m_progressEnd });

    hr = AwaitRequest();
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

        isData = packet.command == CassoLinkCodec::kSectorData;
        CBREx (isData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = CassoLinkCodec::ParseSectorData (packet, gotTrack, gotSector, one);
        CHR (hr);

        //  The sectors not read are skipped, so the packet is the next of the
        //  request's sectors with its number.
        while (next < sectors.size() && sectors[next] != gotSector)
        {
            next++;
        }

        matches = gotTrack == track && next < sectors.size();
        CBREx (matches, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        std::copy (one.begin(), one.end(), outData.begin() + (ptrdiff_t) (next * kSectorSize));
        outRead[next] = true;
        next++;
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

    hr = AwaitRequest();
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
//  CassoLinkHost::Krunch
//
//  Two Readies in a row after Krunch progress are a Status lost on the line:
//  the Apple took the request, and sending it again would run a second
//  krunch against a directory that no longer matches.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::Krunch (Byte drive, Word directoryCrc, const KrunchProgressFn & progress, CassoLinkStatus & outStatus)
{
    HRESULT  hr    = S_OK;
    bool     taken = false;
    bool     lost  = true;



    for (int sent = 0; sent <= kMaxResends && lost; sent++)
    {
        hr   = KrunchOnce (drive, directoryCrc, progress, outStatus, taken);
        lost = hr == kLostRequest && !taken;

        if (lost && m_adtProServer.m_activity)
        {
            m_adtProServer.m_activity ("The Apple did not take a request, so it was sent again");
        }
    }

    hr = (hr == kLostRequest && taken) ? HRESULT_FROM_WIN32 (ERROR_INVALID_DATA) : hr;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::KrunchOnce
//
//  A damaged packet is dropped and the next one read, as the Apple sends
//  nothing again. outTaken is set by the first Krunch progress, which the
//  Apple sends once it has read the directory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::KrunchOnce (Byte drive, Word directoryCrc, const KrunchProgressFn & progress, CassoLinkStatus & outStatus, bool & outTaken)
{
    HRESULT          hr         = S_OK;
    const Byte       request[]  = { drive, (Byte) (directoryCrc & 0xFF), (Byte) (directoryCrc >> 8) };
    CassoLinkPacket  packet;
    bool             isProgress = false;



    outStatus = {};
    outTaken  = false;

    hr = AwaitRequest();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kKrunch, request);
    CHR (hr);

    for (;;)
    {
        hr = ReadReply (packet);

        if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
        {
            continue;
        }

        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        isProgress = packet.command == CassoLinkCodec::kKrunchProgress && packet.payload.size() == 2;
        CBREx (isProgress, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outTaken = true;

        if (progress)
        {
            progress (packet.payload[0], packet.payload[1]);
        }
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

    hr = AwaitRequest();
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

    hr = AwaitRequest();
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
//  CassoLinkHost::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SetColumns (bool wide)
{
    HRESULT  hr = S_OK;



    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kSetColumns, std::vector<Byte> { (Byte) (wide ? 1 : 0) });
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendSessionScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendSessionScreen (bool active, Byte tone, const std::vector<std::string> & rows)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  screen = { (Byte) (active ? 1 : 0), tone };



    for (const std::string & row : rows)
    {
        for (size_t i = 0; i < row.size() && i < CassoLinkCodec::kSessionRowLength; i++)
        {
            Byte  ch      = (Byte) row[i];
            Byte  inverse = ch & 0x80;

            ch &= 0x7F;

            screen.push_back ((ch >= ' ' && ch <= '~') ? (Byte) (toupper (ch) | inverse) : (Byte) '?');
        }

        screen.push_back (0);
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

HRESULT CassoLinkHost::WaitForKey (const std::vector<std::string> & lines, Byte & outKey)
{
    return Resend ([&] () { return WaitForKeyOnce (lines, CassoLinkCodec::kWaitKeys, outKey); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WaitForAnyKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WaitForAnyKey (const std::vector<std::string> & lines, Byte & outKey)
{
    return Resend ([&] () { return WaitForKeyOnce (lines, CassoLinkCodec::kWaitTyped, outKey); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WaitForKeyOnce
//
//  The program sends Key with no code once a second while no key is pressed,
//  so an Apple that was reset or unplugged times out as in any operation. Each
//  line of the prompt ends in a zero.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WaitForKeyOnce (const std::vector<std::string> & lines, Byte mode, Byte & outKey)
{
    HRESULT            hr     = S_OK;
    CassoLinkPacket    packet;
    bool               isKey  = false;
    std::vector<Byte>  prompt = { mode };



    outKey = CassoLinkCodec::kKeyNone;

    for (const std::string & line : lines)
    {
        for (size_t i = 0; i < line.size() && i < CassoLinkCodec::kSessionRowLength; i++)
        {
            prompt.push_back ((line[i] >= ' ' && line[i] <= '~') ? (Byte) toupper ((unsigned char) line[i]) : (Byte) '?');
        }

        prompt.push_back (0);
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (CassoLinkCodec::kWaitForKey, prompt);
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
//  A client of another version is sent the current program, and so is one
//  whose CRC differs from the image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeReady (const CassoLinkPacket & packet)
{
    HRESULT  hr      = S_OK;
    bool     whole   = IsWholeReady (packet);
    bool     current = false;
    bool     intact  = false;



    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ReadClient (packet);

    current = m_client.programVersion == LinkProgram::kProgramVersion;
    intact  = current;

    if (current)
    {
        hr = CheckProgramCrc (intact);
        CHR (hr);
    }

    m_badProgramLoads = (current && !intact) ? m_badProgramLoads + 1 : 0;

    if (m_badProgramLoads > 0 && m_adtProServer.m_activity)
    {
        m_adtProServer.m_activity (std::format ("The program on the Apple failed its CRC check: it computed {:04X}, and the image sent is {:04X}",
                                                m_client.programCrc, CassoLinkCodec::ComputeCrc (m_program)));
    }

    //  A program that fails again after it is loaded again points to the
    //  Apple's memory, which another load would not fix.
    CBREx (m_badProgramLoads <= kMaxProgramReloads, HRESULT_FROM_WIN32 (ERROR_CRC));

    if (intact)
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
//  CassoLinkHost::CheckProgramCrc
//
//  Whether the CRC in the client's Ready matches the image of the program
//  for its chip. A Ready with no CRC, from an earlier program, passes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::CheckProgramCrc (bool & outIntact)
{
    HRESULT  hr = S_OK;



    outIntact = true;

    BAIL_OUT_IF (!m_client.hasProgramCrc, S_OK);

    hr = AssembleProgram();
    CHR (hr);

    outIntact = CassoLinkCodec::ComputeCrc (m_program) == m_client.programCrc;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadClient
//
//  A Ready's versions and drives, and the program's ROM identification and
//  CRC; the payload is 4, 9 or 11 bytes.
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
    m_client.hasRomId       = packet.payload.size() >= 9;
    m_client.hasProgramCrc  = packet.payload.size() == 11;
    m_client.programCrc     = m_client.hasProgramCrc ? (Word) (packet.payload[9] | (packet.payload[10] << 8)) : (Word) 0;
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

    //  After this PC has neither sent nor read a packet for longer than the
    //  client waits for a reply, its Readies have piled up here, and the
    //  client is sending another: a request sent on an old one never arrives,
    //  as the client's chip doesn't take a byte while it sends. They are
    //  dropped, and the next one, just sent, is waited for. A read's Status
    //  counts, so the Ready right after it is used.
    if (!current && m_lastPacketMs != 0 && GetTickCount64() - m_lastPacketMs > kStaleReadyMs)
    {
        hr = DiscardReceived();
        CHR (hr);
    }

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

        current = IsWholeReady (packet) && (Word) (packet.payload[0] | (packet.payload[1] << 8)) == LinkProgram::kProgramVersion;

        if (current)
        {
            ReadClient (packet);

            hr = CheckProgramCrc (current);
            CHR (hr);
        }

        if (!current)
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
//  CassoLinkHost::ArmFault
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ArmFault (const LinkFault & fault)
{
    HRESULT  hr = S_OK;



    CBREx (fault.source == LinkFault::Source::Pc || fault.source == LinkFault::Source::Apple, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (fault.source == LinkFault::Source::Apple)
    {
        CBREx (m_faultProgram, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

        m_appleFault = fault;
        m_requests   = 0;
    }
    else
    {
        std::lock_guard<std::mutex>  lock (m_sendLock);

        m_pcFault.Arm (fault);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::AwaitRequest
//
//  AwaitReady for an operation's request, with the Apple's fault sent first
//  when it is for this request. A sector's fault is sent ahead of the first,
//  and the Apple applies it when it reads the sector.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::AwaitRequest()
{
    HRESULT            hr  = S_OK;
    bool               due = false;
    std::vector<Byte>  payload;



    hr = AwaitReady();
    CHR (hr);

    due = m_appleFault.has_value() && (m_appleFault->IsSectorFault() || m_appleFault->request == m_requests);

    if (due)
    {
        payload = LinkFault::MakePayload (*m_appleFault);

        hr = SendPacket (CassoLinkCodec::kFault, payload);
        CHR (hr);

        if (m_adtProServer.m_activity)
        {
            m_adtProServer.m_activity ("Sent a fault to the Apple: " + LinkFault::Describe (*m_appleFault));
        }

        m_appleFault.reset();

        hr = AwaitReady();
        CHR (hr);
    }

    m_requests++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::TraceReceived
//
//  An idle Apple sends the same Ready in reply to each Idle, so a Ready is
//  traced only when it differs from the last one traced.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::TraceReceived (const CassoLinkPacket & packet)
{
    bool         noKey = packet.command == CassoLinkCodec::kWaitForKey && packet.payload.size() == 1 && packet.payload[0] == CassoLinkCodec::kKeyNone;
    std::string  text;



    if (!m_trace || noKey)
    {
        return;
    }

    text = CassoLinkPacketText::Describe (packet.command, packet.payload);

    if (packet.command == CassoLinkCodec::kReady)
    {
        if (text == m_tracedReady)
        {
            return;
        }

        m_tracedReady = text;
    }

    m_trace (false, text);
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

    if (m_trace && command != CassoLinkCodec::kIdle)
    {
        m_trace (true, CassoLinkPacketText::Describe (command, payload));
    }

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
//  CassoLinkHost::AssembleProgram
//
//  The program for the client's chip, assembled once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::AssembleProgram()
{
    HRESULT      hr = S_OK;
    std::string  errors;



    BAIL_OUT_IF (!m_program.empty(), S_OK);

    hr = LinkProgram::AssembleProgram (m_program, errors, m_programChip, m_faultProgram);

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

Error:
    return hr;
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



    hr = AssembleProgram();
    CHR (hr);

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
