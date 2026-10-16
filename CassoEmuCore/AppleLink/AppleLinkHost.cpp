#include "Pch.h"

#include "AppleLink/AppleLinkHost.h"
#include "AppleLink/LinkProgram.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::AppleLinkHost
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkHost::AppleLinkHost (std::shared_ptr<IByteStream> stream, IAppleLinkFolder & folder, ClockFn clock) :
    m_stream (std::move (stream)),
    m_folder (folder),
    m_clock  (std::move (clock))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::SetVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkHost::SetVirtualDrive (int drive, const std::string & name)
{
    if (drive == 1 || drive == 2)
    {
        m_driveNames[(size_t) drive - 1]  = name;
        m_driveLoaded[(size_t) drive - 1] = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServeOne
//
//  A byte that starts neither a framed packet nor a virtual-drive request is
//  noise on the line, and is skipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeOne (DWORD timeoutMs)
{
    HRESULT          hr     = S_OK;
    AppleLinkPacket  packet;
    bool             filled = false;



    hr = Fill (1, timeoutMs, filled);
    CHR (hr);
    BAIL_OUT_IF (!filled, S_OK);

    if (m_buffer[0] == AppleLinkCodec::kVirtualDrive)
    {
        m_adtProClient = true;

        hr = ServeVirtualDrive();
        CHR (hr);
    }
    else if (m_buffer[0] == AppleLinkCodec::kStart)
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
//  AppleLinkHost::GetProDosDateTime
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 4> AppleLinkHost::GetProDosDateTime (const SYSTEMTIME & time)
{
    Word  date = (Word) (((time.wYear % 100) << 9) | (time.wMonth << 5) | time.wDay);



    return { (Byte) time.wMinute, (Byte) time.wHour, (Byte) (date & 0xFF), (Byte) (date >> 8) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::BuildDirectoryPage
//
//  ADTPro's client prints the text as it arrives, so the layout is ADTPro's:
//  the path line, 40 dashes with no return after them, and a name and a
//  return per file. The text ends with a 0 and the page is zero-filled.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> AppleLinkHost::BuildDirectoryPage (const std::string & path, const std::vector<std::string> & names, size_t page)
{
    std::string        text  = "DIRECTORY OF: " + path + "\r" + std::string (40, '-');
    std::vector<Byte>  bytes (kDirectoryPage, 0);
    size_t             first = page * kNamesPerPage;
    size_t             last  = std::min (names.size(), first + kNamesPerPage);



    for (size_t i = first; i < last; i++)
    {
        text += names[i] + "\r";
    }

    text.resize (std::min (text.size(), kDirectoryPage - 2));
    std::copy (text.begin(), text.end(), bytes.begin());

    bytes[kDirectoryPage - 1] = last < names.size() ? 1 : 0;

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::IsDosOrder
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkHost::IsDosOrder (const std::string & name, size_t size)
{
    std::string  extension = std::filesystem::path (name).extension().string();



    return size == kDosImageSize && (_stricmp (extension.c_str(), ".dsk") == 0 || _stricmp (extension.c_str(), ".do") == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ToBlockOrder
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkHost::ToBlockOrder (const std::string & name, const std::vector<Byte> & file, std::vector<Byte> & outBlocks)
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
//  AppleLinkHost::ToFileOrder
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkHost::ToFileOrder (const std::string & name, const std::vector<Byte> & blocks, std::vector<Byte> & outFile)
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
//  AppleLinkHost::Fill
//
//  outFilled is clear when the timeout passes first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::Fill (size_t count, DWORD timeoutMs, bool & outFilled)
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

        m_buffer.insert (m_buffer.end(), chunk, chunk + got);
    }

    outFilled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::Consume
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkHost::Consume (size_t count)
{
    m_buffer.erase (m_buffer.begin(), m_buffer.begin() + (ptrdiff_t) std::min (count, m_buffer.size()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ReadPacket
//
//  ERROR_TIMEOUT when the packet is not whole within the timeout, and
//  ERROR_INVALID_DATA for a damaged packet, whose bytes are dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReadPacket (DWORD timeoutMs, AppleLinkPacket & outPacket)
{
    HRESULT  hr       = S_OK;
    size_t   consumed = 0;
    bool     complete = false;
    bool     filled   = false;



    for (;;)
    {
        hr = AppleLinkCodec::Decode (m_buffer, consumed, outPacket, complete);
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
//  AppleLinkHost::Send
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::Send (std::span<const Byte> bytes)
{
    std::lock_guard<std::mutex>  lock (m_sendLock);



    return m_stream->Write (bytes.data(), bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::SendByte
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::SendByte (Byte value)
{
    std::lock_guard<std::mutex>  lock (m_sendLock);



    return m_stream->Write (&value, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::SendHome
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::SendHome()
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  home;



    hr = AppleLinkCodec::Encode (AppleLinkCodec::kHome, {}, home);
    CHR (hr);

    hr = Send (home);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServePacket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServePacket (const AppleLinkPacket & packet)
{
    HRESULT  hr = S_OK;



    m_adtProClient = m_adtProClient || packet.command != AppleLinkCodec::kReady;

    switch (packet.command)
    {
        case AppleLinkCodec::kCd:        hr = ServeChangeDir (packet);       break;
        case AppleLinkCodec::kDirectory: hr = ServeDirectory (packet);       break;
        case AppleLinkCodec::kSize:      hr = ServeSize (packet);            break;
        case AppleLinkCodec::kGet:       hr = ServeGet (packet);             break;
        case AppleLinkCodec::kPut:       hr = ServePut (packet, false);      break;
        case AppleLinkCodec::kBatch:     hr = ServePut (packet, true);       break;

        case AppleLinkCodec::kNibble:
        case AppleLinkCodec::kMultiple:
            hr = SendByte (kReplyNotFound);
            break;

        case AppleLinkCodec::kHome:
        case AppleLinkCodec::kSendFile:
            hr = SendHome();
            break;

        case AppleLinkCodec::kReady:
            hr = ServeReady (packet);
            break;

        default:
            break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServeChangeDir
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeChangeDir (const AppleLinkPacket & packet)
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
//  AppleLinkHost::ServeDirectory
//
//  Sorted without regard to case, as ADTPro's server sorts the list on
//  Windows. With no folder open, the page has no path and no files.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeDirectory (const AppleLinkPacket & packet)
{
    HRESULT                   hr    = S_OK;
    size_t                    page  = packet.payload.empty() ? 0 : packet.payload[0];
    std::string               path;
    std::vector<std::string>  names;
    std::vector<Byte>         wire;



    if (m_folder.IsOpen())
    {
        path = m_folder.GetPath();
        hr   = m_folder.List (names);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    std::ranges::sort (names, [] (const std::string & a, const std::string & b) { return _stricmp (a.c_str(), b.c_str()) < 0; });

    hr = AppleLinkCodec::EncodeImageData ((Word) page, BuildDirectoryPage (path, names, page), wire);
    CHR (hr);

    hr = Send (wire);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServeSize
//
//  Size in blocks, LSB first, then the status. An image is a whole number of
//  512-byte blocks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeSize (const AppleLinkPacket & packet)
{
    HRESULT            hr     = S_OK;
    size_t             end    = 0;
    std::string        name   = ReadName (packet.payload, end);
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
//  AppleLinkHost::ServeGet
//
//  Each packet goes out in reply to the client's acknowledgement of the
//  block to send next, and a NAK has the same block sent again. An
//  acknowledgement of the block past the end completes the transfer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeGet (const AppleLinkPacket & packet)
{
    HRESULT            hr        = S_OK;
    size_t             end       = 0;
    std::string        name      = ReadName (packet.payload, end);
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
        AppleLinkPacket    reply;
        std::vector<Byte>  wire;
        size_t             block = 0;
        size_t             count = 0;

        hr = ReadPacket (m_replyTimeoutMs, reply);
        CBREx (hr != HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr);

        if (FAILED (hr) || reply.command != AppleLinkCodec::kAcknowledge || reply.payload.size() < 3)
        {
            BAIL_OUT_IF (SUCCEEDED (hr) && (reply.command == AppleLinkCodec::kSummary || reply.command == AppleLinkCodec::kHome), S_OK);
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

        hr = AppleLinkCodec::EncodeImageData ((Word) block, std::span<const Byte> (image).subspan (block * kBlockSize, count * kBlockSize), wire);
        CHR (hr);

        hr = Send (wire);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServePut
//
//  The image is written only once the client's Summary arrives with every
//  block received, so no file is written for a transfer that fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServePut (const AppleLinkPacket & packet, bool batch)
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

    if (batch)
    {
        name = GetBatchName (name);
    }

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
//  AppleLinkHost::ServeVirtualDrive
//
//  A request whose header check fails is skipped a byte at a time, and a
//  write whose block check fails has no reply, so the driver sends it
//  again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeVirtualDrive()
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
    headerOk = kind >= kDriveRead && kind <= kDriveRead3Drive2 && AppleLinkCodec::ComputeEor (std::span<const Byte> (m_buffer).first (4)) == m_buffer[4];

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
        dataOk = AppleLinkCodec::ComputeEor (data) == m_buffer[5 + kBlockSize];
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
        reply.push_back (AppleLinkCodec::ComputeEor (data));
    }
    else
    {
        if (kind != kDriveRead)
        {
            std::array<Byte, 4>  stamp = m_clock();

            reply.insert (reply.end(), stamp.begin(), stamp.end());
        }

        reply.push_back (AppleLinkCodec::ComputeEor (reply));

        if ((block + 1) * kBlockSize <= m_driveImages[drive].size())
        {
            std::copy (m_driveImages[drive].begin() + (ptrdiff_t) (block * kBlockSize),
                       m_driveImages[drive].begin() + (ptrdiff_t) ((block + 1) * kBlockSize), data.begin());
        }

        reply.insert (reply.end(), data.begin(), data.end());
        reply.push_back (AppleLinkCodec::ComputeEor (data));
    }

    hr = Send (reply);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ReceiveImage
//
//  A packet for blocks already received is acknowledged and its data
//  discarded: ADTPro's client, when an ACK is dropped, sends again under the
//  same starting block but with later blocks' data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReceiveImage (std::vector<Byte> & image)
{
    HRESULT            hr       = S_OK;
    size_t             blocks   = image.size() / kBlockSize;
    std::vector<bool>  received (blocks, false);
    int                naks     = 0;
    bool               giveUp   = false;
    bool               complete = false;



    for (;;)
    {
        AppleLinkPacket  packet;
        size_t           start = 0;
        size_t           size  = 0;
        bool             fits  = false;

        hr = ReadPacket (m_replyTimeoutMs, packet);
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

            hr = SendByte (AppleLinkCodec::kNak);
            CHR (hr);
            continue;
        }

        if (packet.command == AppleLinkCodec::kSummary)
        {
            complete = std::ranges::all_of (received, [] (bool b) { return b; });
            CBREx (complete, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
            BAIL_OUT_IF (true, S_OK);
        }

        CBREx (packet.command != AppleLinkCodec::kHome, E_ABORT);

        if (packet.command != AppleLinkCodec::kImageData)
        {
            continue;
        }

        start = (size_t) packet.payload[0] | ((size_t) packet.payload[1] << 8);
        size  = packet.payload.size() - 2;
        fits  = size % kBlockSize == 0 && (start * kBlockSize) + size <= image.size();

        if (!fits)
        {
            hr = SendByte (AppleLinkCodec::kNak);
            CHR (hr);
            continue;
        }

        if (!received[start])
        {
            std::copy (packet.payload.begin() + 2, packet.payload.end(), image.begin() + (ptrdiff_t) (start * kBlockSize));
            std::fill (received.begin() + (ptrdiff_t) start, received.begin() + (ptrdiff_t) (start + size / kBlockSize), true);
        }

        naks = 0;

        hr = SendByte (AppleLinkCodec::kAck);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::LoadVirtualDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::LoadVirtualDrive (int drive)
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
//  AppleLinkHost::ReadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReadImage (const std::string & name, std::vector<Byte> & outFile)
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
//  AppleLinkHost::GetBatchName
//
//  The prefix with the first free four-digit number ahead of its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkHost::GetBatchName (const std::string & prefix)
{
    size_t       dot       = prefix.find_last_of ('.');
    std::string  stem      = (dot == std::string::npos) ? prefix : prefix.substr (0, dot);
    std::string  extension = (dot == std::string::npos) ? std::string() : prefix.substr (dot);
    std::string  name      = prefix;



    for (int n = 0; n < 10000; n++)
    {
        name = std::format ("{}{:04}{}", stem, n, extension);

        if (!m_folder.Exists (name))
        {
            break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ReadName
//
//  A file name as ADTPro's client sends it: high-bit ASCII up to a 0.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkHost::ReadName (const std::vector<Byte> & payload, size_t & outEnd)
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
//  AppleLinkHost::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReadSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, AppleLinkStatus & outStatus)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  request   = { drive, track, (Byte) sectors.size() };
    AppleLinkPacket    packet;
    Byte               gotTrack  = 0;
    Byte               gotSector = 0;
    size_t             received  = 0;
    bool               isData    = false;
    bool               matches   = false;



    outData.assign (sectors.size() * kSectorSize, 0);
    outStatus = {};

    request.insert (request.end(), sectors.begin(), sectors.end());

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (AppleLinkCodec::kReadSectors, request);
    CHR (hr);

    for (;;)
    {
        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == AppleLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        isData = packet.command == AppleLinkCodec::kSectorData && received < sectors.size();
        CBREx (isData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = AppleLinkCodec::ParseSectorData (packet, gotTrack, gotSector, std::span<Byte> (outData).subspan (received * kSectorSize, kSectorSize));
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
//  AppleLinkHost::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::WriteSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::span<const Byte> data, AppleLinkStatus & outStatus)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  request  = { drive, track, (Byte) sectors.size() };
    std::vector<Byte>  encoded;
    AppleLinkPacket    packet;
    size_t             index    = 0;
    size_t             sends    = 0;
    size_t             limit    = sectors.size() * (kMaxResends + 1);
    bool               sized    = data.size() == sectors.size() * kSectorSize;
    bool               isAck    = false;
    bool               listed   = false;



    outStatus = {};

    CBRAEx (sized, E_INVALIDARG);

    request.insert (request.end(), sectors.begin(), sectors.end());

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (AppleLinkCodec::kWriteSectors, request);
    CHR (hr);

    m_writing = true;

    //  Each Acknowledge from the client has the number of the sector to
    //  send, again after a damaged copy; a Status ends the operation.
    for (;;)
    {
        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == AppleLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        isAck = packet.command == AppleLinkCodec::kAcknowledge && packet.payload.size() == 3;
        CBREx (isAck, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if (m_cancelRequested)
        {
            hr = SendPacket (AppleLinkCodec::kCancel, {});
            CHR (hr);
            continue;
        }

        index = (size_t) (std::find (sectors.begin(), sectors.end(), packet.payload[1]) - sectors.begin());
        listed = index < sectors.size();
        CBREx (listed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        sends++;
        CBREx (sends <= limit, HRESULT_FROM_WIN32 (ERROR_RETRY));

        encoded.clear();

        hr = AppleLinkCodec::EncodeSectorData (track, sectors[index], data.subspan (index * kSectorSize, kSectorSize), encoded);
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
//  AppleLinkHost::FormatTrack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::FormatTrack (Byte drive, Byte track, Byte volume, AppleLinkStatus & outStatus)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { drive, track, volume };
    AppleLinkPacket  packet;



    outStatus = {};

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (AppleLinkCodec::kFormatTrack, request);
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
//  AppleLinkHost::SetSpeed
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::SetSpeed (Byte control)
{
    HRESULT          hr        = S_OK;
    const Byte       request[] = { control };
    AppleLinkPacket  packet;
    bool             acked     = false;



    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (AppleLinkCodec::kSetSpeed, request);
    CHR (hr);

    hr = ReadReply (packet);
    CHR (hr);

    acked = packet.command == AppleLinkCodec::kAcknowledge && !packet.payload.empty() && packet.payload[0] == AppleLinkCodec::kAck;
    CBREx (acked, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::SendLabel
//
//  Anything outside printable ASCII is drawn as a question mark.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::SendLabel (const std::string & text)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  label;



    for (char ch : text)
    {
        if (label.size() == AppleLinkCodec::kLabelLength)
        {
            break;
        }

        label.push_back ((ch >= ' ' && ch <= '~') ? (Byte) toupper ((unsigned char) ch) : (Byte) '?');
    }

    hr = AwaitReady();
    CHR (hr);

    hr = SendPacket (AppleLinkCodec::kLabel, label);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::Cancel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::Cancel()
{
    HRESULT  hr = S_OK;



    m_cancelRequested = true;

    //  While a client writes, its 6551 has room for only the first byte;
    //  WriteSectors sends Cancel in reply to the client's next Acknowledge.
    BAIL_OUT_IF (m_writing, S_OK);

    hr = SendPacket (AppleLinkCodec::kCancel, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ServeReady
//
//  A client of another version is sent the current program.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ServeReady (const AppleLinkPacket & packet)
{
    HRESULT  hr    = S_OK;
    bool     whole = packet.payload.size() == 4;



    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_client.programVersion = (Word) (packet.payload[0] | (packet.payload[1] << 8));
    m_client.shimVersion    = packet.payload[2];
    m_client.driveMap       = packet.payload[3];
    m_hasClient             = true;

    if (m_client.programVersion == LinkProgram::kProgramVersion)
    {
        hr = SendPacket (AppleLinkCodec::kIdle, {});
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
//  AppleLinkHost::AwaitReady
//
//  Waits for Ready from the current program, loading it first if the Ready
//  is from another version. Other packets, and damage between them, are
//  passed over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::AwaitReady()
{
    HRESULT          hr      = S_OK;
    AppleLinkPacket  packet;
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

        if (packet.command != AppleLinkCodec::kReady)
        {
            continue;
        }

        current = packet.payload.size() == 4 && (Word) (packet.payload[0] | (packet.payload[1] << 8)) == LinkProgram::kProgramVersion;

        if (current)
        {
            m_client.programVersion = LinkProgram::kProgramVersion;
            m_client.shimVersion    = packet.payload[2];
            m_client.driveMap       = packet.payload[3];
            m_hasClient             = true;
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
//  AppleLinkHost::ReadReply
//
//  The client's next packet in an operation, past a Ready sent before the
//  operation. After Cancel, a Ready ends the operation.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReadReply (AppleLinkPacket & outPacket)
{
    HRESULT  hr = S_OK;



    do
    {
        hr = ReadPacket (m_operationTimeoutMs, outPacket);
        CHR (hr);

        if (outPacket.command == AppleLinkCodec::kReady && m_cancelRequested.exchange (false))
        {
            m_readyPending = true;
            CBREx (false, HRESULT_FROM_WIN32 (ERROR_CANCELLED));
        }
    }
    while (outPacket.command == AppleLinkCodec::kReady);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::ReadStatus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::ReadStatus (const AppleLinkPacket & packet, AppleLinkStatus & outStatus)
{
    HRESULT  hr     = S_OK;
    bool     status = packet.command == AppleLinkCodec::kStatus && packet.payload.size() == 5;



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
//  AppleLinkHost::SendPacket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::SendPacket (Byte command, std::span<const Byte> payload)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  encoded;



    hr = AppleLinkCodec::Encode (command, payload, encoded);
    CHR (hr);

    hr = Send (encoded);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost::LoadProgram
//
//  Load program and the program's pages, each followed by the shim's bare
//  ACK or NAK. Sent to a running program instead, Load program alone
//  returns it to the shim.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkHost::LoadProgram()
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



    if (m_program.empty())
    {
        hr = LinkProgram::AssembleProgram (m_program, errors);
        CHR (hr);
    }

    pages    = (m_program.size() + kSectorSize - 1) / kSectorSize;
    count[0] = (Byte) pages;

    hr = SendPacket (AppleLinkCodec::kLoadProgram, count);
    CHR (hr);

    //  A running program goes back to the shim, whose Ready comes next.
    BAIL_OUT_IF (m_client.programVersion != 0, S_OK);

    for (size_t i = 0; i < pages; i++)
    {
        from = i * kSectorSize;

        std::fill (page.begin(), page.end(), (Byte) 0);
        std::copy_n (m_program.begin() + (ptrdiff_t) from, std::min (kSectorSize, m_program.size() - from), page.begin());

        encoded.clear();

        hr = AppleLinkCodec::EncodeSectorData ((Byte) i, (Byte) i, page, encoded);
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
                Consume (1);
            }
            while (reply != AppleLinkCodec::kAck && reply != AppleLinkCodec::kNak);

            acked = reply == AppleLinkCodec::kAck;
        }

        CBREx (acked, HRESULT_FROM_WIN32 (ERROR_RETRY));
    }

Error:
    return hr;
}
