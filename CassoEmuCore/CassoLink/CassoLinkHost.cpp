#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
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
//  CassoLinkHost::SetVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::SetVirtualDrive (int drive, const std::string & name)
{
    if (drive == 1 || drive == 2)
    {
        m_driveNames[(size_t) drive - 1]  = name;
        m_driveLoaded[(size_t) drive - 1] = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeOne
//
//  A byte that starts neither a framed packet nor a virtual-drive request is
//  noise on the line, and is skipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeOne (DWORD timeoutMs)
{
    HRESULT          hr     = S_OK;
    CassoLinkPacket  packet;
    bool             filled = false;



    hr = Fill (1, timeoutMs, filled);
    CHR (hr);
    BAIL_OUT_IF (!filled, S_OK);

    if (m_buffer[0] == CassoLinkCodec::kVirtualDrive)
    {
        m_adtProClient = true;

        hr = ServeVirtualDrive();
        CHR (hr);
    }
    else if (m_buffer[0] == CassoLinkCodec::kStart)
    {
        hr = ReadPacket (m_replyTimeoutMs, packet);

        if (hr == S_OK)
        {
            hr           = ServePacket (packet);
            m_lastResult = hr;
            CBREx (hr != HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE), hr);
        }

        //  A damaged command is resent by the client once it times out, and
        //  one cut short by a timeout has nothing to serve. A transfer that
        //  failed is in m_lastResult, and the host goes on serving.
        hr = S_OK;
    }
    else
    {
        Consume (1);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::GetProDosDateTime
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 4> CassoLinkHost::GetProDosDateTime (const SYSTEMTIME & time)
{
    Word  date = (Word) (((time.wYear % 100) << 9) | (time.wMonth << 5) | time.wDay);



    return { (Byte) time.wMinute, (Byte) time.wHour, (Byte) (date & 0xFF), (Byte) (date >> 8) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::BuildDirectoryPage
//
//  ADTPro's client prints the text as it arrives, so the layout is ADTPro's:
//  the path line, 40 dashes with no return after them, and a name and a
//  return per file. The text ends with a 0 and the page is zero-filled.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> CassoLinkHost::BuildDirectoryPage (const std::string & path, const std::vector<std::string> & names, size_t page)
{
    std::string        header = (path.size() <= 66) ? "DIRECTORY OF: " + path : path;
    std::string        text;
    std::vector<Byte>  bytes (kDirectoryPage, 0);
    size_t             first  = page * kNamesPerPage;
    size_t             last   = std::min (names.size(), first + kNamesPerPage);
    bool               more   = last < names.size();



    //  The header fills at most the first two of the client's 40-column
    //  lines, keeping its end when it is longer. A return ends it when it
    //  stops short of a line's end, and a second one follows a one-line
    //  header, so the names start on the screen's fourth row, where the
    //  client's file chooser counts from.
    header = (header.size() > 80) ? header.substr (header.size() - 80) : header;
    header = (header.size() < 80) ? header + "\r" : header;
    header = (header.size() <= 40) ? header + "\r" : header;

    text = header + std::string (40, '-');

    if (names.empty())
    {
        text += "NO FILES";
        more  = false;
    }

    //  A name that fills its line has no return after it; a longer one is
    //  cut to 38 characters and "..".
    for (size_t i = first; i < last; i++)
    {
        std::string  name = (names[i].size() > 40) ? names[i].substr (0, 38) + ".." : names[i];

        text += (name.size() < 40) ? name + "\r" : name;
    }

    text.resize (std::min (text.size(), kDirectoryPage - 2));
    std::copy (text.begin(), text.end(), bytes.begin());

    //  The text's 0, then whether another page follows.
    bytes[text.size() + 1] = more ? 1 : 0;

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::MatchesFilespec
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkHost::MatchesFilespec (const std::string & name, const std::string & filespec)
{
    std::function<bool (size_t, size_t)>  match;



    match = [&] (size_t n, size_t f) -> bool
    {
        if (f == filespec.size())
        {
            return n == name.size();
        }

        if (filespec[f] == '*')
        {
            return match (n, f + 1) || (n < name.size() && match (n + 1, f));
        }

        return n < name.size() && (filespec[f] == '?' || toupper ((unsigned char) filespec[f]) == toupper ((unsigned char) name[n])) && match (n + 1, f + 1);
    };

    return filespec.empty() || match (0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ResolveName
//
//  A name, or the token ADTPro's file chooser sends for the name picked
//  from the last listing: 1, the page counted from 1, and the screen row,
//  the first name being on row 3.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::ResolveName (const std::vector<Byte> & payload, size_t & outEnd) const
{
    std::string  name  = ReadName (payload, outEnd);
    size_t       index = 0;
    bool         token = payload.size() >= 3 && payload[0] == 0x01 && payload[1] >= 1 && payload[2] >= 3;



    if (!token)
    {
        return name;
    }

    index = kNamesPerPage * ((size_t) payload[1] - 1) + ((size_t) payload[2] - 3);

    return (index < m_listing.size()) ? m_listing[index] : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::IsDosOrder
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkHost::IsDosOrder (const std::string & name, size_t size)
{
    std::string  extension = std::filesystem::path (name).extension().string();



    return size == kDosImageSize && (_stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ToBlockOrder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::ToBlockOrder (const std::string & name, const std::vector<Byte> & file, std::vector<Byte> & outBlocks)
{
    outBlocks = file;

    if (IsDosOrder (name, file.size()))
    {
        for (size_t sector = 0; sector < file.size() / kSectorSize; sector++)
        {
            size_t  track = sector / kSectorsPerTrack;
            size_t  from  = (track * kSectorsPerTrack + s_kBlockSectorToDos[sector % kSectorsPerTrack]) * kSectorSize;

            std::copy (file.begin() + (ptrdiff_t) from, file.begin() + (ptrdiff_t) (from + kSectorSize), outBlocks.begin() + (ptrdiff_t) (sector * kSectorSize));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ToFileOrder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::ToFileOrder (const std::string & name, const std::vector<Byte> & blocks, std::vector<Byte> & outFile)
{
    outFile = blocks;

    if (IsDosOrder (name, blocks.size()))
    {
        for (size_t sector = 0; sector < blocks.size() / kSectorSize; sector++)
        {
            size_t  track = sector / kSectorsPerTrack;
            size_t  to    = (track * kSectorsPerTrack + s_kBlockSectorToDos[sector % kSectorsPerTrack]) * kSectorSize;

            std::copy (blocks.begin() + (ptrdiff_t) (sector * kSectorSize), blocks.begin() + (ptrdiff_t) ((sector + 1) * kSectorSize), outFile.begin() + (ptrdiff_t) to);
        }
    }
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

    while (m_buffer.size() < count)
    {
        hr = m_stream->Read (chunk, sizeof (chunk), timeoutMs, got);
        CHR (hr);

        closed = got == 0 && m_stream->IsClosed();
        CBREx (!closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));
        BAIL_OUT_IF (got == 0, S_OK);

        m_lastHeardMs = GetTickCount64();
        m_buffer.insert (m_buffer.end(), chunk, chunk + got);
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
    m_buffer.erase (m_buffer.begin(), m_buffer.begin() + (ptrdiff_t) std::min (count, m_buffer.size()));
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
        hr = CassoLinkCodec::Decode (m_buffer, consumed, outPacket, complete);
        Consume (consumed);
        CHR (hr);
        BAIL_OUT_IF (complete, S_OK);

        hr = Fill (m_buffer.size() + 1, timeoutMs, filled);
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
//  CassoLinkHost::SendHome
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendHome()
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  home;



    hr = CassoLinkCodec::Encode (CassoLinkCodec::kHome, {}, home);
    CHR (hr);

    hr = Send (home);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServePacket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServePacket (const CassoLinkPacket & packet)
{
    HRESULT  hr = S_OK;



    m_adtProClient = m_adtProClient || packet.command != CassoLinkCodec::kReady;

    switch (packet.command)
    {
        case CassoLinkCodec::kCd:        hr = ServeChangeDir (packet);       break;
        case CassoLinkCodec::kDirectory: hr = ServeDirectory (packet);       break;
        case CassoLinkCodec::kSize:      hr = ServeSize (packet);            break;
        case CassoLinkCodec::kGet:       hr = ServeGet (packet);             break;
        case CassoLinkCodec::kPut:       hr = ServePut (packet, false);      break;
        case CassoLinkCodec::kBatch:     hr = ServePut (packet, true);       break;
        case CassoLinkCodec::kNibble:    hr = ServeNibblePut (packet, false); break;
        case CassoLinkCodec::kMultiple:  hr = ServeNibblePut (packet, true);  break;

        //  ADTPro's server has no reply to the client's Home, which the client
        //  sends at its start and after an abort; Send File loads ADTPro's
        //  own bootstrap, which Casso does not serve.
        case CassoLinkCodec::kHome:
        case CassoLinkCodec::kSendFile:
            break;

        case CassoLinkCodec::kReady:
            hr = ServeReady (packet);
            break;

        default:
            break;
    }

    ReportRequest (packet, hr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReportRequest
//
//  ADTPro's requests that move or list files; a directory's later pages and
//  CassoLink's own packets are not reported.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::ReportRequest (const CassoLinkPacket & packet, HRESULT result)
{
    size_t       end    = 0;
    std::string  name;
    std::string  text;



    if (!m_activity)
    {
        return;
    }

    switch (packet.command)
    {
        case CassoLinkCodec::kCd:
            name = ReadName (packet.payload, end);
            text = std::format ("ADTPro: changed to the folder {}", name);
            break;

        case CassoLinkCodec::kDirectory:
            text = (packet.payload.empty() || packet.payload[0] == 0) ? std::string ("ADTPro: listed the folder") : std::string();
            break;

        case CassoLinkCodec::kGet:
            name = ResolveName (packet.payload, end);
            text = std::format ("ADTPro: sent {} to the Apple", name);
            break;

        case CassoLinkCodec::kPut:
        case CassoLinkCodec::kBatch:
        case CassoLinkCodec::kNibble:
        case CassoLinkCodec::kMultiple:
            name = ReadName (packet.payload, end);
            text = std::format ("ADTPro: received {} from the Apple", name);
            break;

        default:
            break;
    }

    if (!text.empty())
    {
        m_activity (FAILED (result) ? text + " (failed)" : text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeChangeDir
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeChangeDir (const CassoLinkPacket & packet)
{
    HRESULT      hr      = S_OK;
    size_t       end     = 0;
    std::string  name    = ReadName (packet.payload, end);
    HRESULT      hrCd    = m_folder.IsOpen() ? m_folder.ChangeDirectory (name) : HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND);
    bool         changed = SUCCEEDED (hrCd);



    hr = SendByte (changed ? kReplyOk : kReplyUnableToCd);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeDirectory
//
//  Sorted without regard to case, as ADTPro's server sorts the list on
//  Windows. With no folder open, the page has no path and no files.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeDirectory (const CassoLinkPacket & packet)
{
    HRESULT                   hr       = S_OK;
    size_t                    page     = packet.payload.empty() ? 0 : packet.payload.back();
    size_t                    end      = 0;
    std::string               filespec = ReadName (packet.payload, end);
    std::string               path;
    std::vector<std::string>  names;
    std::vector<Byte>         wire;



    //  The filespec, ending in a 0, comes first, and the page is the last
    //  byte; a request with only the page has no filespec.
    filespec = (packet.payload.size() < 2) ? std::string() : filespec;

    if (m_folder.IsOpen())
    {
        path = m_folder.GetPath();
        hr   = m_folder.List (names);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    std::erase_if (names, [&filespec] (const std::string & name) { return !MatchesFilespec (name, filespec); });
    std::ranges::sort (names, [] (const std::string & a, const std::string & b) { return _stricmp (a.c_str(), b.c_str()) < 0; });

    m_listing = names;

    hr = CassoLinkCodec::EncodeImageData ((Word) page, BuildDirectoryPage (path, names, page), wire);
    CHR (hr);

    hr = Send (wire);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeSize
//
//  Size in blocks, LSB first, then the status. An image is a whole number of
//  512-byte blocks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeSize (const CassoLinkPacket & packet)
{
    HRESULT            hr     = S_OK;
    size_t             end    = 0;
    std::string        name   = ResolveName (packet.payload, end);
    std::vector<Byte>  image;
    Byte               status = kReplyNotFound;
    size_t             blocks = 0;
    HRESULT            hrRead = ReadImage (name, image);



    if (SUCCEEDED (hrRead))
    {
        blocks = image.size() / kBlockSize;
        status = (!image.empty() && image.size() % kBlockSize == 0) ? kReplyOk : kReplyNotAnImage;
    }

    {
        const Byte  reply[] = { (Byte) (blocks & 0xFF), (Byte) (blocks >> 8), status };

        hr = Send (reply);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeGet
//
//  Each packet goes out in reply to the client's acknowledgement of the
//  block to send next, and a NAK has the same block sent again. An
//  acknowledgement of the block past the end completes the transfer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeGet (const CassoLinkPacket & packet)
{
    HRESULT            hr        = S_OK;
    size_t             end       = 0;
    std::string        name      = ResolveName (packet.payload, end);
    size_t             perPacket = (end < packet.payload.size() && packet.payload[end] != 0) ? packet.payload[end] : 1;
    std::vector<Byte>  file;
    std::vector<Byte>  image;
    bool               found     = false;
    size_t             blocks    = 0;
    size_t             lastBlock = SIZE_MAX;
    int                resends   = 0;
    bool               giveUp    = false;
    HRESULT            hrRead    = S_OK;



    hrRead = ReadImage (name, file);
    found  = SUCCEEDED (hrRead);

    hr = SendByte (found ? kReplyOk : kReplyNotFound);
    CHR (hr);
    BAIL_OUT_IF (!found, S_OK);

    ToBlockOrder (name, file, image);
    blocks = image.size() / kBlockSize;

    for (;;)
    {
        CassoLinkPacket    reply;
        std::vector<Byte>  wire;
        size_t             block = 0;
        size_t             count = 0;

        hr = ReadPacket (m_operationTimeoutMs, reply);
        CBREx (hr != HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr);

        if (FAILED (hr) || reply.command != CassoLinkCodec::kAcknowledge || reply.payload.size() < 3)
        {
            BAIL_OUT_IF (SUCCEEDED (hr) && (reply.command == CassoLinkCodec::kSummary || reply.command == CassoLinkCodec::kHome), S_OK);
            hr = S_OK;
            continue;
        }

        block = (size_t) reply.payload[1] | ((size_t) reply.payload[2] << 8);
        BAIL_OUT_IF (block >= blocks, S_OK);

        resends   = (block == lastBlock) ? resends + 1 : 0;
        lastBlock = block;
        giveUp    = resends > kMaxResends;

        if (giveUp)
        {
            hr = SendHome();
            CHR (hr);
        }

        CBREx (!giveUp, HRESULT_FROM_WIN32 (ERROR_RETRY));

        count = std::min (perPacket, blocks - block);

        hr = CassoLinkCodec::EncodeImageData ((Word) block, std::span<const Byte> (image).subspan (block * kBlockSize, count * kBlockSize), wire);
        CHR (hr);

        hr = Send (wire);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServePut
//
//  The image is written only once the client's Summary arrives with every
//  block received, so no file is written for a transfer that fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServePut (const CassoLinkPacket & packet, bool batch)
{
    HRESULT            hr     = S_OK;
    size_t             end    = 0;
    std::string        name   = ReadName (packet.payload, end);
    size_t             blocks = (end + 1 < packet.payload.size()) ? ((size_t) packet.payload[end] | ((size_t) packet.payload[end + 1] << 8)) : 0;
    bool               open   = m_folder.IsOpen() && blocks != 0;
    std::vector<Byte>  image;
    std::vector<Byte>  file;



    hr = SendByte (open ? kReplyOk : kReplyNotFound);
    CHR (hr);
    BAIL_OUT_IF (!open, S_OK);

    name = batch ? GetBatchName (name, (blocks * kBlockSize == kDosImageSize) ? ".dsk" : ".po")
                 : AddImageExtension (name, blocks * kBlockSize);

    image.assign (blocks * kBlockSize, 0);

    hr = ReceiveImage (image);
    CHR (hr);

    ToFileOrder (name, image, file);

    hr = m_folder.Write (name, file);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeNibblePut
//
//  A whole disk's nibbles, as ADTPro's Nibble send has them: each track's
//  capture as its blocks, then an ACK of the whole track, which the client
//  waits for before reading the next; the Summary ends it. The image is
//  written as a .nib once every track is in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeNibblePut (const CassoLinkPacket & packet, bool batch)
{
    HRESULT            hr      = S_OK;
    size_t             end     = 0;
    std::string        name    = ReadName (packet.payload, end);
    bool               open    = m_folder.IsOpen();
    std::vector<Byte>  capture (kNibbleCapture, 0);
    std::vector<Byte>  file;
    std::vector<Byte>  track;
    CassoLinkPacket    summary;
    bool               nib     = false;



    hr = SendByte (open ? kReplyOk : kReplyNotFound);
    CHR (hr);
    BAIL_OUT_IF (!open, S_OK);

    nib  = name.size() >= 4 && _stricmp (name.c_str() + name.size() - 4, ".nib") == 0;
    name = batch ? GetBatchName (name, ".nib") : (nib ? name : name + ".nib");

    for (size_t t = 0; t < kNibbleTracks; t++)
    {
        std::fill (capture.begin(), capture.end(), (Byte) 0);

        hr = ReceiveImage (capture, false);
        CHR (hr);

        track = ToNibbleTrack (capture);
        file.insert (file.end(), track.begin(), track.end());

        hr = SendByte (CassoLinkCodec::kAck);
        CHR (hr);
    }

    do
    {
        hr = ReadPacket (m_operationTimeoutMs, summary);
    }
    while (SUCCEEDED (hr) && summary.command != CassoLinkCodec::kSummary && summary.command != CassoLinkCodec::kHome);

    hr = m_folder.Write (name, file);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ToNibbleTrack
//
//  The track's period is where its first address field comes round again;
//  without one, as on an unformatted track, the capture's first
//  kNibbleTrackSize bytes are the track.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> CassoLinkHost::ToNibbleTrack (std::span<const Byte> capture)
{
    static constexpr size_t  s_kMatchLength = 64;
    static constexpr size_t  s_kMinPeriod   = 5800;
    const Byte               prologue[]     = { 0xD5, 0xAA, 0x96 };
    std::vector<Byte>   track (kNibbleTrackSize, 0xFF);
    auto                found      = std::search (capture.begin(), capture.end(), std::begin (prologue), std::end (prologue));
    size_t              start      = (size_t) (found - capture.begin());
    size_t              period     = kNibbleTrackSize;



    if (start >= capture.size() || start + kNibbleTrackSize + s_kMatchLength > capture.size())
    {
        std::copy_n (capture.begin(), std::min (capture.size(), kNibbleTrackSize), track.begin());
        return track;
    }

    for (size_t p = s_kMinPeriod; p <= kNibbleTrackSize; p++)
    {
        if (std::equal (capture.begin() + (ptrdiff_t) start, capture.begin() + (ptrdiff_t) (start + s_kMatchLength), capture.begin() + (ptrdiff_t) (start + p)))
        {
            period = p;
            break;
        }
    }

    std::copy_n (capture.begin() + (ptrdiff_t) start, period, track.begin());
    return track;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeVirtualDrive
//
//  A request whose header check fails is skipped a byte at a time, and a
//  write whose block check fails has no reply, so the driver sends it
//  again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeVirtualDrive()
{
    HRESULT            hr       = S_OK;
    Byte               kind     = 0;
    bool               headerOk = false;
    bool               write    = false;
    bool               dataOk   = false;
    bool               loaded   = false;
    bool               filled   = false;
    size_t             drive    = 0;
    size_t             block    = 0;
    std::vector<Byte>  data (kBlockSize, 0);
    std::vector<Byte>  reply;
    std::vector<Byte>  file;



    hr = Fill (5, m_replyTimeoutMs, filled);
    CHR (hr);
    BAIL_OUT_IF (!filled, S_OK);

    kind     = m_buffer[1];
    headerOk = kind >= kDriveRead && kind <= kDriveRead3Drive2 && CassoLinkCodec::ComputeEor (std::span<const Byte> (m_buffer).first (4)) == m_buffer[4];

    if (!headerOk)
    {
        Consume (1);
        BAIL_OUT_IF (true, S_OK);
    }

    write = kind == kDriveWrite1 || kind == kDriveWrite2;
    drive = (kind == kDriveWrite2 || kind == kDriveRead3Drive2) ? 1 : 0;
    block = (size_t) m_buffer[2] | ((size_t) m_buffer[3] << 8);

    if (write)
    {
        hr = Fill (5 + kBlockSize + 1, m_replyTimeoutMs, filled);
        CHR (hr);
        BAIL_OUT_IF (!filled, S_OK);

        std::copy (m_buffer.begin() + 5, m_buffer.begin() + 5 + (ptrdiff_t) kBlockSize, data.begin());
        dataOk = CassoLinkCodec::ComputeEor (data) == m_buffer[5 + kBlockSize];
    }

    reply.assign (m_buffer.begin(), m_buffer.begin() + 4);
    Consume (write ? 5 + kBlockSize + 1 : 5);
    BAIL_OUT_IF (write && !dataOk, S_OK);

    hr     = LoadVirtualDrive ((int) drive);
    loaded = SUCCEEDED (hr);
    BAIL_OUT_IF (!loaded, S_OK);

    if (write)
    {
        if ((block + 1) * kBlockSize <= m_driveImages[drive].size())
        {
            std::copy (data.begin(), data.end(), m_driveImages[drive].begin() + (ptrdiff_t) (block * kBlockSize));

            ToFileOrder (m_driveNames[drive], m_driveImages[drive], file);

            hr = m_folder.Write (m_driveNames[drive], file);
            CHR (hr);
        }

        //  ADTPro's server ends the reply with the block's check, not the
        //  header's.
        reply.push_back (CassoLinkCodec::ComputeEor (data));
    }
    else
    {
        if (kind != kDriveRead)
        {
            std::array<Byte, 4>  stamp = m_clock();

            reply.insert (reply.end(), stamp.begin(), stamp.end());
        }

        reply.push_back (CassoLinkCodec::ComputeEor (reply));

        if ((block + 1) * kBlockSize <= m_driveImages[drive].size())
        {
            std::copy (m_driveImages[drive].begin() + (ptrdiff_t) (block * kBlockSize),
                       m_driveImages[drive].begin() + (ptrdiff_t) ((block + 1) * kBlockSize), data.begin());
        }

        reply.insert (reply.end(), data.begin(), data.end());
        reply.push_back (CassoLinkCodec::ComputeEor (data));
    }

    hr = Send (reply);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReceiveImage
//
//  A packet for blocks already received is acknowledged and its data
//  discarded: ADTPro's client, when an ACK is dropped, sends again under the
//  same starting block but with later blocks' data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReceiveImage (std::vector<Byte> & image, bool untilSummary)
{
    HRESULT            hr       = S_OK;
    size_t             blocks   = image.size() / kBlockSize;
    std::vector<bool>  received (blocks, false);
    int                naks     = 0;
    bool               giveUp   = false;
    bool               complete = false;



    for (;;)
    {
        CassoLinkPacket  packet;
        size_t           start = 0;
        size_t           size  = 0;
        bool             fits  = false;

        hr = ReadPacket (m_operationTimeoutMs, packet);
        CBREx (hr != HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr);

        if (FAILED (hr))
        {
            naks++;
            giveUp = naks > kMaxResends;

            if (giveUp)
            {
                hr = SendHome();
                CHR (hr);
            }

            CBREx (!giveUp, HRESULT_FROM_WIN32 (ERROR_RETRY));

            hr = SendByte (CassoLinkCodec::kNak);
            CHR (hr);
            continue;
        }

        if (packet.command == CassoLinkCodec::kSummary)
        {
            complete = std::ranges::all_of (received, [] (bool b) { return b; });
            CBREx (complete, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
            BAIL_OUT_IF (true, S_OK);
        }

        CBREx (packet.command != CassoLinkCodec::kHome, E_ABORT);

        if (packet.command != CassoLinkCodec::kImageData)
        {
            continue;
        }

        start = (size_t) packet.payload[0] | ((size_t) packet.payload[1] << 8);
        size  = packet.payload.size() - 2;
        fits  = size % kBlockSize == 0 && (start * kBlockSize) + size <= image.size();

        if (!fits)
        {
            hr = SendByte (CassoLinkCodec::kNak);
            CHR (hr);
            continue;
        }

        if (!received[start])
        {
            std::copy (packet.payload.begin() + 2, packet.payload.end(), image.begin() + (ptrdiff_t) (start * kBlockSize));
            std::fill (received.begin() + (ptrdiff_t) start, received.begin() + (ptrdiff_t) (start + size / kBlockSize), true);
        }

        naks = 0;

        hr = SendByte (CassoLinkCodec::kAck);
        CHR (hr);

        //  A nibble track ends when its blocks are in, with no Summary.
        complete = std::ranges::all_of (received, [] (bool b) { return b; });
        BAIL_OUT_IF (!untilSummary && complete, S_OK);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::LoadVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::LoadVirtualDrive (int drive)
{
    HRESULT            hr     = S_OK;
    size_t             index  = (size_t) drive;
    bool               open   = m_folder.IsOpen();
    std::vector<Byte>  file;



    BAIL_OUT_IF (m_driveLoaded[index], S_OK);
    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    hr = m_folder.Read (m_driveNames[index], file);
    CHR (hr);

    ToBlockOrder (m_driveNames[index], file, m_driveImages[index]);

    m_driveLoaded[index] = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadImage (const std::string & name, std::vector<Byte> & outFile)
{
    HRESULT  hr     = S_OK;
    bool     exists = m_folder.IsOpen() && m_folder.Exists (name);



    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = m_folder.Read (name, outFile);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::GetBatchName
//
//  The prefix with the first free four-digit number ahead of its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::GetBatchName (const std::string & prefix, const std::string & extension)
{
    std::string  name = prefix + "0001" + extension;



    for (int n = 1; n < 10000; n++)
    {
        name = std::format ("{}{:04}{}", prefix, n, extension);

        if (!m_folder.Exists (name))
        {
            break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::AddImageExtension
//
//  A 140 KB disk gets .dsk unless it ends in .dsk or .do already, and any
//  other gets .po unless it ends in .po or .hdv, as from ADTPro's server.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::AddImageExtension (const std::string & name, size_t bytes)
{
    std::string  extension = std::filesystem::path (name).extension().string();
    bool         dos       = bytes == kDosImageSize;
    bool         suffixed  = dos ? (_stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0)
                                 : (_stricmp (extension.c_str(), ".po") == 0 || _stricmp (extension.c_str(), ".hdv") == 0);



    return suffixed ? name : name + (dos ? ".dsk" : ".po");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadName
//
//  A file name as ADTPro's client sends it: high-bit ASCII up to a 0.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::ReadName (const std::vector<Byte> & payload, size_t & outEnd)
{
    std::string  name;
    size_t       i = 0;



    while (i < payload.size() && payload[i] != 0)
    {
        name += (char) (payload[i] & 0x7F);
        i++;
    }

    outEnd = std::min (i + 1, payload.size());
    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus)
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

HRESULT CassoLinkHost::WriteTrack (Byte drive, Byte track, Byte volume, std::span<const Byte> data, CassoLinkStatus & outStatus)
{
    std::array<Byte, 16>  physicals = {};



    for (size_t i = 0; i < physicals.size(); i++)
    {
        physicals[i] = (Byte) i;
    }

    return SendSectors (CassoLinkCodec::kWriteTrack, { drive, track, volume }, track, physicals, data, outStatus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendSectors
//
//  A write's request, then each sector the client requests by an
//  Acknowledge, until its Status.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendSectors (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                    std::span<const Byte> data, CassoLinkStatus & outStatus)
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

        hr = CassoLinkCodec::EncodeSectorData (track, sectors[index], data.subspan (index * kSectorSize, kSectorSize), encoded);
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
//  CassoLinkHost::FormatTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::FormatTrack (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { drive, track, volume, m_progressStart, m_progressEnd };
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
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::ReadClient (const CassoLinkPacket & packet)
{
    m_client.programVersion = (Word) (packet.payload[0] | (packet.payload[1] << 8));
    m_client.shimVersion    = packet.payload[2];
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
        hr = ReadPacket (m_operationTimeoutMs, packet);

        if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
        {
            continue;
        }

        CHR (hr);

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
    HRESULT  hr = S_OK;



    do
    {
        hr = ReadPacket (m_operationTimeoutMs, outPacket);
        CHR (hr);

        if (outPacket.command == CassoLinkCodec::kReady && m_cancelRequested.exchange (false))
        {
            m_readyPending = true;
            CBREx (false, HRESULT_FROM_WIN32 (ERROR_CANCELLED));
        }
    }
    while (outPacket.command == CassoLinkCodec::kReady);

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



    if (m_program.empty())
    {
        hr = LinkProgram::AssembleProgram (m_program, errors);
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
                hr = Fill (1, m_operationTimeoutMs, filled);
                CHR (hr);
                CBREx (filled, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

                reply = m_buffer[0];
                running = reply == CassoLinkCodec::kStart && i + 1 == pages;

                //  A packet in place of the last page's ACK is the program's
                //  Ready: the program is running, and its Ready is left for
                //  the next operation to read.
                if (!running)
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
