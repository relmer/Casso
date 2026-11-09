#include "Pch.h"

#include "CassoLink/Host/CassoLinkAdtProServer.h"
#include "CassoLink/CassoLinkHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::CassoLinkAdtProServer
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkAdtProServer::CassoLinkAdtProServer (CassoLinkHost & host)
    : m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::SetVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkAdtProServer::SetVirtualDrive (int drive, const std::string & name)
{
    if (drive == 1 || drive == 2)
    {
        m_driveNames[(size_t) drive - 1]  = name;
        m_driveLoaded[(size_t) drive - 1] = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServeOne
//
//  A byte that starts neither a framed packet nor a virtual-drive request is
//  noise on the line, and is skipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeOne (DWORD timeoutMs)
{
    HRESULT          hr     = S_OK;
    CassoLinkPacket  packet;
    bool             filled = false;



    hr = m_host.Fill (1, timeoutMs, filled);
    CHR (hr);
    BAIL_OUT_IF (!filled, S_OK);

    if (m_buffer[0] == CassoLinkCodec::kVirtualDrive && m_clients != CassoLinkHostTypes::Clients::Casso)
    {
        m_adtProClient = true;

        hr = ServeVirtualDrive();
        CHR (hr);
    }
    else if (m_buffer[0] == CassoLinkCodec::kStart)
    {
        hr = m_host.ReadPacket (m_replyTimeoutMs, packet);

        if (hr == S_OK && CassoLinkHost::IsKeyReport (packet))
        {
            m_host.NoteKey (packet);
        }
        else if (hr == S_OK)
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
        m_host.Consume (1);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::GetProDosDateTime
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 4> CassoLinkAdtProServer::GetProDosDateTime (const SYSTEMTIME & time)
{
    Word  date = (Word) (((time.wYear % 100) << 9) | (time.wMonth << 5) | time.wDay);



    return { (Byte) time.wMinute, (Byte) time.wHour, (Byte) (date & 0xFF), (Byte) (date >> 8) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::BuildDirectoryPage
//
//  ADTPro's client prints the text as it arrives, so the layout is ADTPro's:
//  the path line, 40 dashes with no return after them, and a name and a
//  return per file. The text ends with a 0 and the page is zero-filled.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> CassoLinkAdtProServer::BuildDirectoryPage (const std::string & path, const std::vector<std::string> & names, size_t page)
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
//  CassoLinkAdtProServer::MatchesFilespec
//
//  * matches any run of characters and ? any one, without regard to case.
//  On a mismatch the run the last * matches is extended by one character,
//  with no backtracking to earlier stars, so a filespec of many stars cannot
//  stall the link.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkAdtProServer::MatchesFilespec (const std::string & name, const std::string & filespec)
{
    size_t  n       = 0;
    size_t  f       = 0;
    size_t  star    = std::string::npos;
    size_t  mark    = 0;
    bool    matched = true;



    while (matched && n < name.size())
    {
        bool  literal = f < filespec.size() && filespec[f] != '*'
                     && (filespec[f] == '?' || toupper ((unsigned char) filespec[f]) == toupper ((unsigned char) name[n]));

        if (literal)
        {
            n++;
            f++;
        }
        else if (f < filespec.size() && filespec[f] == '*')
        {
            star = f++;
            mark = n;
        }
        else if (star != std::string::npos)
        {
            f = star + 1;
            n = ++mark;
        }
        else
        {
            matched = false;
        }
    }

    while (f < filespec.size() && filespec[f] == '*')
    {
        f++;
    }

    return filespec.empty() || (matched && f == filespec.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ResolveName
//
//  A name, or the token ADTPro's file chooser sends for the name picked
//  from the last listing: 1, the page counted from 1, and the screen row,
//  the first name being on row 3.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkAdtProServer::ResolveName (const std::vector<Byte> & payload, size_t & outEnd) const
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
//  CassoLinkAdtProServer::IsDosOrder
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkAdtProServer::IsDosOrder (const std::string & name, size_t size)
{
    std::string  extension = std::filesystem::path (name).extension().string();



    return size == CassoLinkHost::kDosImageSize && (_stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ToBlockOrder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkAdtProServer::ToBlockOrder (const std::string & name, const std::vector<Byte> & file, std::vector<Byte> & outBlocks)
{
    outBlocks = file;

    if (IsDosOrder (name, file.size()))
    {
        for (size_t sector = 0; sector < file.size() / CassoLinkHost::kSectorSize; sector++)
        {
            size_t  track = sector / kSectorsPerTrack;
            size_t  from  = (track * kSectorsPerTrack + s_kBlockSectorToDos[sector % kSectorsPerTrack]) * CassoLinkHost::kSectorSize;

            std::copy (file.begin() + (ptrdiff_t) from, file.begin() + (ptrdiff_t) (from + CassoLinkHost::kSectorSize), outBlocks.begin() + (ptrdiff_t) (sector * CassoLinkHost::kSectorSize));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ToFileOrder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkAdtProServer::ToFileOrder (const std::string & name, const std::vector<Byte> & blocks, std::vector<Byte> & outFile)
{
    outFile = blocks;

    if (IsDosOrder (name, blocks.size()))
    {
        for (size_t sector = 0; sector < blocks.size() / CassoLinkHost::kSectorSize; sector++)
        {
            size_t  track = sector / kSectorsPerTrack;
            size_t  to    = (track * kSectorsPerTrack + s_kBlockSectorToDos[sector % kSectorsPerTrack]) * CassoLinkHost::kSectorSize;

            std::copy (blocks.begin() + (ptrdiff_t) (sector * CassoLinkHost::kSectorSize), blocks.begin() + (ptrdiff_t) ((sector + 1) * CassoLinkHost::kSectorSize), outFile.begin() + (ptrdiff_t) to);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::SendHome
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::SendHome()
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  home;



    hr = CassoLinkCodec::Encode (CassoLinkCodec::kHome, {}, home);
    CHR (hr);

    hr = m_host.Send (home);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServePacket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServePacket (const CassoLinkPacket & packet)
{
    HRESULT  hr     = S_OK;
    bool     casso  = packet.command == CassoLinkCodec::kReady;
    bool     served = casso ? m_clients != CassoLinkHostTypes::Clients::AdtPro : m_clients != CassoLinkHostTypes::Clients::Casso;



    BAIL_OUT_IF (!served, S_OK);

    m_adtProClient = m_adtProClient || !casso;

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
            hr = m_host.ServeReady (packet);
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
//  CassoLinkAdtProServer::ReportRequest
//
//  ADTPro's requests that move or list files; a directory's later pages and
//  CassoLink's own packets are not reported.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkAdtProServer::ReportRequest (const CassoLinkPacket & packet, HRESULT result)
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
//  CassoLinkAdtProServer::ServeChangeDir
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeChangeDir (const CassoLinkPacket & packet)
{
    HRESULT      hr      = S_OK;
    size_t       end     = 0;
    std::string  name    = ReadName (packet.payload, end);
    HRESULT      hrCd    = m_host.m_folder.IsOpen() ? m_host.m_folder.ChangeDirectory (name) : HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND);
    bool         changed = SUCCEEDED (hrCd);



    hr = m_host.SendByte (changed ? CassoLinkHost::kReplyOk : CassoLinkHost::kReplyUnableToCd);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServeDirectory
//
//  Sorted without regard to case, as ADTPro's server sorts the list on
//  Windows. With no folder open, the page has no path and no files.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeDirectory (const CassoLinkPacket & packet)
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

    if (m_host.m_folder.IsOpen())
    {
        path = m_host.m_folder.GetPath();
        hr   = m_host.m_folder.List (names);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    std::erase_if (names, [&filespec] (const std::string & name) { return !MatchesFilespec (name, filespec); });
    std::ranges::sort (names, [] (const std::string & a, const std::string & b) { return _stricmp (a.c_str(), b.c_str()) < 0; });

    m_listing = names;

    hr = CassoLinkCodec::EncodeImageData ((Word) page, BuildDirectoryPage (path, names, page), wire);
    CHR (hr);

    hr = m_host.Send (wire);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServeSize
//
//  Size in blocks, LSB first, then the status. An image is a whole number of
//  512-byte blocks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeSize (const CassoLinkPacket & packet)
{
    HRESULT            hr     = S_OK;
    size_t             end    = 0;
    std::string        name   = ResolveName (packet.payload, end);
    std::vector<Byte>  image;
    Byte               status = CassoLinkHost::kReplyNotFound;
    size_t             blocks = 0;
    HRESULT            hrRead = ReadImage (name, image);



    if (SUCCEEDED (hrRead))
    {
        blocks = image.size() / kBlockSize;
        status = (!image.empty() && image.size() % kBlockSize == 0) ? CassoLinkHost::kReplyOk : CassoLinkHost::kReplyNotAnImage;
    }

    {
        const Byte  reply[] = { (Byte) (blocks & 0xFF), (Byte) (blocks >> 8), status };

        hr = m_host.Send (reply);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServeGet
//
//  Each packet goes out in reply to the client's acknowledgement of the
//  block to send next, and a NAK has the same block sent again. An
//  acknowledgement of the block past the end completes the transfer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeGet (const CassoLinkPacket & packet)
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

    hr = m_host.SendByte (found ? CassoLinkHost::kReplyOk : CassoLinkHost::kReplyNotFound);
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

        hr = m_host.ReadPacket (m_operationTimeoutMs, reply);
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
        giveUp    = resends > CassoLinkHost::kMaxResends;

        if (giveUp)
        {
            hr = SendHome();
            CHR (hr);
        }

        CBREx (!giveUp, HRESULT_FROM_WIN32 (ERROR_RETRY));

        count = std::min (perPacket, blocks - block);

        hr = CassoLinkCodec::EncodeImageData ((Word) block, std::span<const Byte> (image).subspan (block * kBlockSize, count * kBlockSize), wire);
        CHR (hr);

        hr = m_host.Send (wire);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServePut
//
//  The image is written only once the client's Summary arrives with every
//  block received, so no file is written for a transfer that fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServePut (const CassoLinkPacket & packet, bool batch)
{
    HRESULT            hr     = S_OK;
    size_t             end    = 0;
    std::string        name   = ReadName (packet.payload, end);
    size_t             blocks = (end + 1 < packet.payload.size()) ? ((size_t) packet.payload[end] | ((size_t) packet.payload[end + 1] << 8)) : 0;
    bool               open   = m_host.m_folder.IsOpen() && blocks != 0;
    std::vector<Byte>  image;
    std::vector<Byte>  file;



    hr = m_host.SendByte (open ? CassoLinkHost::kReplyOk : CassoLinkHost::kReplyNotFound);
    CHR (hr);
    BAIL_OUT_IF (!open, S_OK);

    name = batch ? GetBatchName (name, (blocks * kBlockSize == CassoLinkHost::kDosImageSize) ? ".dsk" : ".po")
                 : AddImageExtension (name, blocks * kBlockSize);

    image.assign (blocks * kBlockSize, 0);

    hr = ReceiveImage (image);
    CHR (hr);

    ToFileOrder (name, image, file);

    hr = m_host.m_folder.Write (name, file);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ServeNibblePut
//
//  A whole disk's nibbles, as ADTPro's Nibble send has them: each track's
//  capture as its blocks, then an ACK of the whole track, which the client
//  waits for before reading the next; the Summary ends it. The image is
//  written as a .nib once every track is in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeNibblePut (const CassoLinkPacket & packet, bool batch)
{
    HRESULT            hr   = S_OK;
    size_t             end  = 0;
    std::string        name = ReadName (packet.payload, end);
    bool               open = m_host.m_folder.IsOpen();
    std::vector<Byte>  capture (CassoLinkHost::kNibbleCapture, 0);
    std::vector<Byte>  file;
    std::vector<Byte>  track;
    CassoLinkPacket    summary;
    bool               nib     = false;



    hr = m_host.SendByte (open ? CassoLinkHost::kReplyOk : CassoLinkHost::kReplyNotFound);
    CHR (hr);
    BAIL_OUT_IF (!open, S_OK);

    nib  = name.size() >= 4 && _stricmp (name.c_str() + name.size() - 4, ".nib") == 0;
    name = batch ? GetBatchName (name, ".nib") : (nib ? name : name + ".nib");

    for (size_t t = 0; t < CassoLinkHost::kNibbleTracks; t++)
    {
        std::fill (capture.begin(), capture.end(), (Byte) 0);

        hr = ReceiveImage (capture, false);
        CHR (hr);

        track = ToNibbleTrack (capture);
        file.insert (file.end(), track.begin(), track.end());

        hr = m_host.SendByte (CassoLinkCodec::kAck);
        CHR (hr);
    }

    do
    {
        hr = m_host.ReadPacket (m_operationTimeoutMs, summary);
    }
    while (SUCCEEDED (hr) && summary.command != CassoLinkCodec::kSummary && summary.command != CassoLinkCodec::kHome);

    hr = m_host.m_folder.Write (name, file);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ToNibbleTrack
//
//  The track's period is where its first address field comes round again;
//  without one, as on an unformatted track, the capture's first
//  kNibbleTrackSize bytes are the track.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> CassoLinkAdtProServer::ToNibbleTrack (std::span<const Byte> capture)
{
    static constexpr size_t  s_kMatchLength = 64;
    static constexpr size_t  s_kMinPeriod   = 5800;
    const Byte               prologue[]     = { 0xD5, 0xAA, 0x96 };
    std::vector<Byte>   track (CassoLinkHost::kNibbleTrackSize, 0xFF);
    auto                found  = std::search (capture.begin(), capture.end(), std::begin (prologue), std::end (prologue));
    size_t              start  = (size_t) (found - capture.begin());
    size_t              period = CassoLinkHost::kNibbleTrackSize;



    if (start >= capture.size() || start + CassoLinkHost::kNibbleTrackSize + s_kMatchLength > capture.size())
    {
        std::copy_n (capture.begin(), std::min (capture.size(), CassoLinkHost::kNibbleTrackSize), track.begin());
        return track;
    }

    for (size_t p = s_kMinPeriod; p <= CassoLinkHost::kNibbleTrackSize; p++)
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
//  CassoLinkAdtProServer::ServeVirtualDrive
//
//  A request whose header check fails is skipped a byte at a time, and a
//  write whose block check fails has no reply, so the driver sends it
//  again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ServeVirtualDrive()
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



    hr = m_host.Fill (5, m_replyTimeoutMs, filled);
    CHR (hr);
    BAIL_OUT_IF (!filled, S_OK);

    kind     = m_buffer[1];
    headerOk = kind >= kDriveRead && kind <= kDriveRead3Drive2 && CassoLinkCodec::ComputeEor (std::span<const Byte> (m_buffer).first (4)) == m_buffer[4];

    if (!headerOk)
    {
        m_host.Consume (1);
        BAIL_OUT_IF (true, S_OK);
    }

    write = kind == CassoLinkHost::kDriveWrite1 || kind == kDriveWrite2;
    drive = (kind == kDriveWrite2 || kind == kDriveRead3Drive2) ? 1 : 0;
    block = (size_t) m_buffer[2] | ((size_t) m_buffer[3] << 8);

    if (write)
    {
        hr = m_host.Fill (5 + kBlockSize + 1, m_replyTimeoutMs, filled);
        CHR (hr);
        BAIL_OUT_IF (!filled, S_OK);

        std::copy (m_buffer.begin() + 5, m_buffer.begin() + 5 + (ptrdiff_t) kBlockSize, data.begin());
        dataOk = CassoLinkCodec::ComputeEor (data) == m_buffer[5 + kBlockSize];
    }

    reply.assign (m_buffer.begin(), m_buffer.begin() + 4);
    m_host.Consume (write ? 5 + kBlockSize + 1 : 5);
    BAIL_OUT_IF (write && !dataOk, S_OK);

    hr     = LoadVirtualDrive ((int) drive);
    loaded = SUCCEEDED (hr);
    BAIL_OUT_IF (!loaded, S_OK);

    if (write)
    {
        if ((block + 1) * kBlockSize <= m_driveImages[drive].size())
        {
            std::copy (data.begin(), data.end(), m_driveImages[drive].begin() + (ptrdiff_t) (block * kBlockSize));

            hr = WriteDriveBlock (drive, block, data);
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
            std::array<Byte, 4>  stamp = m_host.m_clock();

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

    hr = m_host.Send (reply);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::WriteDriveBlock
//
//  The block's bytes where the image file has them: one run in a block-order
//  image, and in a DOS-order one its two sectors, each where ToFileOrder
//  puts it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::WriteDriveBlock (size_t drive, size_t block, std::span<const Byte> data)
{
    HRESULT              hr   = S_OK;
    const std::string &  name = m_driveNames[drive];



    if (!IsDosOrder (name, m_driveImages[drive].size()))
    {
        hr = m_host.m_folder.WriteAt (name, (uint64_t) block * kBlockSize, data);
        CHR (hr);
    }
    else
    {
        for (size_t half = 0; half < kBlockSize / CassoLinkHost::kSectorSize; half++)
        {
            size_t  sector = block * (kBlockSize / CassoLinkHost::kSectorSize) + half;
            size_t  track  = sector / kSectorsPerTrack;
            size_t  to     = (track * kSectorsPerTrack + s_kBlockSectorToDos[sector % kSectorsPerTrack]) * CassoLinkHost::kSectorSize;

            hr = m_host.m_folder.WriteAt (name, to, data.subspan (half * CassoLinkHost::kSectorSize, CassoLinkHost::kSectorSize));
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ReceiveImage
//
//  A packet for blocks already received is acknowledged and its data
//  discarded: ADTPro's client, when an ACK is dropped, sends again under the
//  same starting block but with later blocks' data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ReceiveImage (std::vector<Byte> & image, bool untilSummary)
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

        hr = m_host.ReadPacket (m_operationTimeoutMs, packet);
        CBREx (hr != HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr);

        if (FAILED (hr))
        {
            naks++;
            giveUp = naks > CassoLinkHost::kMaxResends;

            if (giveUp)
            {
                hr = SendHome();
                CHR (hr);
            }

            CBREx (!giveUp, HRESULT_FROM_WIN32 (ERROR_RETRY));

            hr = m_host.SendByte (CassoLinkCodec::kNak);
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
        fits  = size != 0 && size % kBlockSize == 0 && (start * kBlockSize) + size <= image.size();

        if (!fits)
        {
            hr = m_host.SendByte (CassoLinkCodec::kNak);
            CHR (hr);
            continue;
        }

        if (!received[start])
        {
            std::copy (packet.payload.begin() + 2, packet.payload.end(), image.begin() + (ptrdiff_t) (start * kBlockSize));
            std::fill (received.begin() + (ptrdiff_t) start, received.begin() + (ptrdiff_t) (start + size / kBlockSize), true);
        }

        naks = 0;

        hr = m_host.SendByte (CassoLinkCodec::kAck);
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
//  CassoLinkAdtProServer::LoadVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::LoadVirtualDrive (int drive)
{
    HRESULT            hr    = S_OK;
    size_t             index = (size_t) drive;
    bool               open  = m_host.m_folder.IsOpen();
    std::vector<Byte>  file;



    BAIL_OUT_IF (m_driveLoaded[index], S_OK);
    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    hr = m_host.m_folder.Read (m_driveNames[index], file);
    CHR (hr);

    ToBlockOrder (m_driveNames[index], file, m_driveImages[index]);

    m_driveLoaded[index] = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ReadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkAdtProServer::ReadImage (const std::string & name, std::vector<Byte> & outFile)
{
    HRESULT  hr     = S_OK;
    bool     exists = m_host.m_folder.IsOpen() && m_host.m_folder.Exists (name);



    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = m_host.m_folder.Read (name, outFile);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::GetBatchName
//
//  The prefix with the first free four-digit number ahead of its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkAdtProServer::GetBatchName (const std::string & prefix, const std::string & extension)
{
    std::string  name = prefix + "0001" + extension;



    for (int n = 1; n < 10000; n++)
    {
        name = std::format ("{}{:04}{}", prefix, n, extension);

        if (!m_host.m_folder.Exists (name))
        {
            break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::AddImageExtension
//
//  A 140 KB disk gets .dsk unless it ends in .dsk or .do already, and any
//  other gets .po unless it ends in .po or .hdv, as from ADTPro's server.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkAdtProServer::AddImageExtension (const std::string & name, size_t bytes)
{
    std::string  extension = std::filesystem::path (name).extension().string();
    bool         dos       = bytes == CassoLinkHost::kDosImageSize;
    bool         suffixed  = dos ? (_stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0)
                                 : (_stricmp (extension.c_str(), ".po") == 0 || _stricmp (extension.c_str(), ".hdv") == 0);



    return suffixed ? name : name + (dos ? ".dsk" : ".po");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer::ReadName
//
//  A file name as ADTPro's client sends it: high-bit ASCII up to a 0.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkAdtProServer::ReadName (const std::vector<Byte> & payload, size_t & outEnd)
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
