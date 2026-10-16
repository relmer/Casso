#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkCodec.h"
#include "AppleLink/AppleLinkHost.h"
#include "AppleLink/AppleLinkSession.h"
#include "AppleLink/LinkProgram.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeApple
//
//  The Apple end of a link, in C++: Casso's client as the program speaks
//  it -- Ready, Idle, Load program, Set speed, Read and Write sectors,
//  Format track, Status and Cancel -- over two drive images in
//  DOS-logical buffers. Each connection is served on its own thread until
//  the line closes, and the drives outlast the connection.
//
//  As a shim, it reports program version 0 and loads the program it is
//  sent before answering as the program.
//
////////////////////////////////////////////////////////////////////////////////

class FakeApple
{
public:
    std::array<std::vector<Byte>, 2>  drives;
    std::array<bool, 2>               writeProtected = {};
    std::atomic<bool>                 startAsShim    { false };
    std::atomic<bool>                 nakFirstPage   { false };

    //  As an Apple whose Ready went out before the line was open: silent
    //  until the host sends it something.
    std::atomic<bool>                 readySentEarly { false };
    std::atomic<int>                  sectorDelayMs  { 0 };
    std::atomic<int>                  sectorsRead    { 0 };
    std::atomic<int>                  sectorsWritten { 0 };
    std::atomic<int>                  cancels        { 0 };

    //  Sectors the drive cannot read, as track and physical sector.
    std::set<std::pair<int, int>>     badSectors;
    std::vector<Byte>                 loadedProgram;
    std::mutex                        lock;

    ~FakeApple()
    {
        Detach();
    }

    //  Serves a new connection's Apple end.
    void  Attach (std::shared_ptr<InProcessByteStream> end)
    {
        Detach();

        m_end    = end;
        m_thread = std::jthread ([this, end] (std::stop_token stop) { Serve (stop, *end); });
    }

    //  Drops the line, as a cable pulled.
    void  Detach()
    {
        if (m_end)
        {
            m_end->Close();
        }

        if (m_thread.joinable())
        {
            m_thread.request_stop();
            m_thread.join();
        }

        m_end.reset();
    }

private:
    struct Line
    {
        InProcessByteStream  & stream;
        std::vector<Byte>      buffer;
    };

    static void  Send (Line & line, std::span<const Byte> bytes)
    {
        line.stream.Write (bytes.data(), bytes.size());
    }

    static void  SendPacket (Line & line, Byte command, std::vector<Byte> payload)
    {
        std::vector<Byte>  wire;

        AppleLinkCodec::Encode (command, payload, wire);
        Send (line, wire);
    }

    //  False on a timeout or a closed line.
    static bool  Receive (std::stop_token stop, Line & line, AppleLinkPacket & outPacket, DWORD timeoutMs)
    {
        Byte    chunk[4096];
        size_t  got      = 0;
        size_t  consumed = 0;
        bool    complete = false;
        auto    until    = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);

        while (!stop.stop_requested() && std::chrono::steady_clock::now() < until)
        {
            if (!line.buffer.empty())
            {
                HRESULT  hr = AppleLinkCodec::Decode (line.buffer, consumed, outPacket, complete);

                line.buffer.erase (line.buffer.begin(), line.buffer.begin() + (ptrdiff_t) consumed);

                if (SUCCEEDED (hr) && complete)
                {
                    return true;
                }

                if (FAILED (hr))
                {
                    continue;
                }
            }

            HRESULT  read = line.stream.Read (chunk, sizeof (chunk), 20, got);

            if (FAILED (read) || (got == 0 && line.stream.IsClosed()))
            {
                return false;
            }

            line.buffer.insert (line.buffer.end(), chunk, chunk + got);
        }

        return false;
    }

    static size_t  Index (int track, int physical)
    {
        return (size_t) (track * 16 + AppleLinkSession::GetLogicalSector (physical)) * 256;
    }

    void  Serve (std::stop_token stop, InProcessByteStream & stream)
    {
        Line             line { stream, {} };
        Word             version = startAsShim ? 0 : LinkProgram::kProgramVersion;
        AppleLinkPacket  packet;
        bool             silent  = readySentEarly;

        while (!stop.stop_requested() && !stream.IsClosed())
        {
            if (!silent)
            {
                SendPacket (line, AppleLinkCodec::kReady, { (Byte) (version & 0xFF), (Byte) (version >> 8), LinkProgram::kShimVersion, 0x03 });
            }

            silent = false;

            if (!Receive (stop, line, packet, 3000))
            {
                continue;
            }

            switch (packet.command)
            {
                case AppleLinkCodec::kIdle:
                    std::this_thread::sleep_for (std::chrono::milliseconds (2));
                    break;

                case AppleLinkCodec::kLoadProgram:
                    version = (version == 0 && Load (stop, line, packet.payload.at (0))) ? LinkProgram::kProgramVersion : 0;
                    break;

                case AppleLinkCodec::kSetSpeed:
                    SendPacket (line, AppleLinkCodec::kAcknowledge, { AppleLinkCodec::kAck, 0, 0 });
                    break;

                case AppleLinkCodec::kReadSectors:
                    ReadSectors (stop, line, packet.payload);
                    break;

                case AppleLinkCodec::kWriteSectors:
                    WriteSectors (stop, line, packet.payload);
                    break;

                case AppleLinkCodec::kFormatTrack:
                    FormatTrack (line, packet.payload);
                    break;

                default:
                    break;
            }
        }
    }

    bool  Load (std::stop_token stop, Line & line, size_t pages)
    {
        AppleLinkPacket    packet;
        std::vector<Byte>  page (256);
        std::vector<Byte>  program;
        Byte               track  = 0;
        Byte               sector = 0;
        bool               naked  = false;

        for (size_t i = 0; i < pages; )
        {
            if (!Receive (stop, line, packet, 3000))
            {
                return false;
            }

            HRESULT  parsed = AppleLinkCodec::ParseSectorData (packet, track, sector, page);

            if (FAILED (parsed))
            {
                return false;
            }

            if (nakFirstPage && !naked)
            {
                naked = true;
                Send (line, std::vector<Byte> { AppleLinkCodec::kNak });
                continue;
            }

            program.insert (program.end(), page.begin(), page.end());
            Send (line, std::vector<Byte> { AppleLinkCodec::kAck });
            i++;
        }

        std::lock_guard<std::mutex>  guard (lock);

        loadedProgram = program;
        return true;
    }

    void  ReadSectors (std::stop_token stop, Line & line, const std::vector<Byte> & request)
    {
        int                drive = request.at (0);
        int                track = request.at (1);
        size_t             count = request.at (2);
        std::vector<Byte>  wire;
        size_t             done  = 0;

        if (drive < 1 || drive > 2 || drives[(size_t) drive - 1].empty())
        {
            SendPacket (line, AppleLinkCodec::kStatus, { 1, (Byte) drive, (Byte) track, 0, AppleLinkHost::kErrorDrive });
            return;
        }

        for (done = 0; done < count && !stop.stop_requested(); done++)
        {
            Byte  physical = request.at (3 + done);

            if (badSectors.contains ({ track, physical }))
            {
                SendPacket (line, AppleLinkCodec::kStatus, { 1, (Byte) drive, (Byte) track, (Byte) done, AppleLinkHost::kErrorNotFound });
                return;
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (sectorDelayMs.load()));

            {
                std::lock_guard<std::mutex>  guard (lock);
                auto                         at = drives[(size_t) drive - 1].begin() + (ptrdiff_t) Index (track, physical);

                wire.clear();
                AppleLinkCodec::EncodeSectorData ((Byte) track, physical, std::span<const Byte> (at, at + 256), wire);
            }

            Send (line, wire);
            sectorsRead++;

            //  Any byte from the host during a read is Cancel.
            if (line.stream.GetReadableCount() > 0 || !line.buffer.empty())
            {
                AppleLinkPacket  ignored;

                Receive (stop, line, ignored, 100);
                cancels++;
                return;
            }
        }

        SendPacket (line, AppleLinkCodec::kStatus, { 1, (Byte) drive, (Byte) track, (Byte) done, AppleLinkHost::kErrorNone });
    }

    void  WriteSectors (std::stop_token stop, Line & line, const std::vector<Byte> & request)
    {
        int                drive = request.at (0);
        int                track = request.at (1);
        size_t             count = request.at (2);
        AppleLinkPacket    packet;
        std::vector<Byte>  data (256);
        Byte               gotTrack  = 0;
        Byte               gotSector = 0;
        size_t             done      = 0;

        if (writeProtected[(size_t) drive - 1])
        {
            SendPacket (line, AppleLinkCodec::kStatus, { 2, (Byte) drive, (Byte) track, 0, AppleLinkHost::kErrorWriteProtected });
            return;
        }

        for (done = 0; done < count; done++)
        {
            Byte  physical = request.at (3 + done);

            SendPacket (line, AppleLinkCodec::kAcknowledge, { AppleLinkCodec::kAck, physical, (Byte) track });

            if (!Receive (stop, line, packet, 3000))
            {
                return;
            }

            if (packet.command == AppleLinkCodec::kCancel)
            {
                cancels++;
                return;
            }

            AppleLinkCodec::ParseSectorData (packet, gotTrack, gotSector, data);

            {
                std::lock_guard<std::mutex>  guard (lock);

                std::copy (data.begin(), data.end(), drives[(size_t) drive - 1].begin() + (ptrdiff_t) Index (track, physical));
            }

            sectorsWritten++;
        }

        SendPacket (line, AppleLinkCodec::kStatus, { 2, (Byte) drive, (Byte) track, (Byte) done, AppleLinkHost::kErrorNone });
    }

    void  FormatTrack (Line & line, const std::vector<Byte> & request)
    {
        int  drive = request.at (0);
        int  track = request.at (1);

        {
            std::lock_guard<std::mutex>  guard (lock);
            auto                         at = drives[(size_t) drive - 1].begin() + (ptrdiff_t) (track * 16 * 256);

            std::fill (at, at + 16 * 256, (Byte) 0);
        }

        SendPacket (line, AppleLinkCodec::kStatus, { 3, (Byte) drive, (Byte) track, 1, AppleLinkHost::kErrorNone });
    }

    std::shared_ptr<InProcessByteStream>  m_end;
    std::jthread                          m_thread;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeAppleFactory
//
//  Every TCP connection the session makes is a new in-process line whose
//  Apple end FakeApple serves. A COM port or a listener is not offered.
//
////////////////////////////////////////////////////////////////////////////////

class FakeAppleFactory : public IByteStreamFactory
{
public:
    explicit FakeAppleFactory (FakeApple & apple) : m_apple (apple) {}

    std::atomic<int>  connections { 0 };

    //  The Apple end of the last line, when answer was clear.
    std::shared_ptr<InProcessByteStream>  GetUnansweredEnd () const { return m_unanswered; }
    std::atomic<bool> answer      { true };

    HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override
    {
        outPorts.clear();
        return S_OK;
    }

    HRESULT  OpenComPort (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override
    {
        return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    }

    HRESULT  ListenTcp (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override
    {
        return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
    }

    HRESULT  ConnectTcp (const std::string &, uint16_t, std::unique_ptr<IByteStream> & outStream) override
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<InProcessByteStream>  appleEnd;

        InProcessByteStream::CreatePair (pcEnd, appleEnd);
        connections++;

        if (answer)
        {
            m_apple.Attach (appleEnd);
        }

        m_unanswered = appleEnd;
        outStream    = std::make_unique<SharedStream> (pcEnd);
        return S_OK;
    }

private:
    //  An owned handle on a shared in-process end.
    class SharedStream : public IByteStream
    {
    public:
        explicit SharedStream (std::shared_ptr<InProcessByteStream> end) : m_end (std::move (end)) {}

        HRESULT  Write            (const Byte * data, size_t count) override                          { return m_end->Write (data, count); }
        HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return m_end->Read (buffer, capacity, timeoutMs, outRead); }
        size_t   GetReadableCount () override                                                           { return m_end->GetReadableCount(); }
        bool     IsClosed         () const override                                                     { return m_end->IsClosed(); }
        void     Close            () override                                                           { m_end->Close(); }

    private:
        std::shared_ptr<InProcessByteStream>  m_end;
    };

    FakeApple                             & m_apple;
    std::shared_ptr<InProcessByteStream>    m_unanswered;
};
