#pragma once

#include "Pch.h"

#include "CassoLink/Host/CassoLinkAdtProServer.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/CassoLinkCodec.h"

class IByteStream;





////////////////////////////////////////////////////////////////////////////////
//
//  ICassoLinkFolder
//
//  The image folder CassoLinkHost serves from: its files by name, read and
//  written whole. A write replaces the file in one step, so a failed transfer
//  writes nothing. With no folder open, as when Casso Explorer's tab is on
//  This PC, every request for a file or folder has ADTPro's unable status
//  for it.
//
////////////////////////////////////////////////////////////////////////////////

class ICassoLinkFolder
{
public:
    virtual ~ICassoLinkFolder () = default;

    virtual bool         IsOpen          () const                                                    = 0;

    //  As ADTPro's Directory prints it: the full path, with a trailing
    //  separator.
    virtual std::string  GetPath         () const                                                    = 0;
    virtual HRESULT      List            (std::vector<std::string> & outNames)                       = 0;
    virtual bool         Exists          (const std::string & name)                                  = 0;
    virtual HRESULT      Read            (const std::string & name, std::vector<Byte> & outBytes)     = 0;
    virtual HRESULT      Write           (const std::string & name, const std::vector<Byte> & bytes)  = 0;

    //  Bytes written over the file's at offset, for a VSDrive write of one block.
    virtual HRESULT      WriteAt         (const std::string & name, uint64_t offset, std::span<const Byte> bytes) = 0;
    virtual HRESULT      ChangeDirectory (const std::string & name)                                  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient
//
//  Casso's client as its last Ready packet reported it, and the Status packet
//  that ends each of its operations. A shim with no program loaded reports
//  program version 0.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkClient
{
    Word  programVersion = 0;
    Byte  shimVersion    = 0;
    Byte  driveMap       = 0;

    //  The program's Ready has the ROM's identification bytes: $FBB3,
    //  $FBC0, $FBBF, $FB1E, and 1 for a IIgs. The shim's has none.
    bool                 hasRomId = false;
    std::array<Byte, 5>  romId    = {};

    //  The model the bytes identify, as Apple //c (ROM 4); empty without them.
    std::wstring  GetModelName () const;
};

struct CassoLinkStatus
{
    Byte  operation = 0;
    Byte  drive     = 0;
    Byte  track     = 0;
    Byte  done      = 0;
    Byte  error     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost
//
//  The PC end of the link for ADTPro's own clients: CD, Directory, Size, Get,
//  Put, Batch, Summary and Home, and virtual drives over 512-byte blocks.
//  Nibble and Multiple have ADTPro's unable reply, as raw tracks are the
//  protected-disk follow-on's; Send File has Home in reply, as the bootstrap
//  files are ADTPro's and not Casso's.
//
//  ServeOne waits for one request and serves it, including the whole
//  transfer a Get, Put or Batch starts. A transfer ends when the client
//  sends its Summary or acknowledges the block past the image's end; it fails
//  when a packet is resent kMaxResends times without success, the host then
//  sending Home as ADTPro's does, or when no byte arrives within the reply
//  timeout. A failed transfer writes nothing, and its result is stored for
//  GetLastResult while the host goes on serving; only a broken stream ends
//  ServeOne with a failure.
//
//  Casso's own client sends Ready instead of ADTPro's commands. ServeOne
//  sends Idle in reply, or the current program when the client's version
//  differs. The operations below wait for the client's next Ready
//  and send it the operation in reply; the program is loaded first in the
//  same way. Each returns S_OK with the client's Status once the client
//  finished, and a disk error, such as a write-protected disk, is the
//  Status's error code. A failure of the link itself is the HRESULT.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkHost : public CassoLinkHostTypes
{
public:

    static constexpr int     kMaxResends         = 5;

    //  A request the client did not take: it sent Ready again, idle, in
    //  place of a reply. HRESULT_FROM_WIN32 (ERROR_RETRY).
    static constexpr HRESULT kLostRequest        = (HRESULT) 0x800704D5;
    static constexpr size_t  kSectorSize         = 256;
    static constexpr size_t  kDosImageSize       = 143360;

    //  A nibble send: 35 tracks, each a capture of 26 blocks with two
    //  revolutions, and a .nib track of one revolution.
    static constexpr size_t  kNibbleTracks       = 35;
    static constexpr size_t  kNibbleCapture      = 13312;
    static constexpr size_t  kNibbleTrackSize    = 6656;

    //  Casso's client's Status error codes.
    static constexpr Byte    kErrorNone           = 0x00;
    static constexpr Byte    kErrorWriteProtected = 0x10;
    static constexpr Byte    kErrorNotFound       = 0x20;
    static constexpr Byte    kErrorData           = 0x30;
    static constexpr Byte    kErrorVerify         = 0x40;
    static constexpr Byte    kErrorLink           = 0x50;
    static constexpr Byte    kErrorDrive          = 0x60;

    //  One-byte replies to commands, ADTPro's values.
    static constexpr Byte    kReplyOk          = 0x00;
    static constexpr Byte    kReplyNotFound    = 0x02;
    static constexpr Byte    kReplyNotAnImage  = 0x04;
    static constexpr Byte    kReplyUnableToCd  = 0x06;

    static constexpr Byte    kDriveWrite1      = 0x02;
    static constexpr Byte    kDriveRead3Drive1 = 0x03;

    CassoLinkHost (std::shared_ptr<IByteStream> stream, ICassoLinkFolder & folder, ClockFn clock);

    //  The images virtual drives 1 and 2 serve, by name in the folder;
    //  ADTPro's defaults until set.
    void     SetVirtualDrive (int drive, const std::string & name) { m_adtProServer.SetVirtualDrive (drive, name); }
    void     SetClients      (Clients clients) { m_adtProServer.m_clients = clients; }
    void     SetReplyTimeout (DWORD timeoutMs) { m_adtProServer.m_replyTimeoutMs = timeoutMs; }

    //  How long an operation for Casso's client waits for each packet, and an
    //  ADTPro transfer for the client's next packet. The default allows for a
    //  machine paused in a debugger, and for ADTPro's client reading or
    //  writing its disk between packets.
    void     SetOperationTimeout (DWORD timeoutMs) { m_adtProServer.m_operationTimeoutMs = timeoutMs; }

    //  Casso's client, once one has sent Ready.
    bool             HasClient () const { return m_hasClient; }

    //  Whether ADTPro's own client has sent a request since the host was
    //  made.
    bool             HasAdtProClient () const { return m_adtProServer.m_adtProClient; }
    CassoLinkClient  GetClient () const { return m_client; }

    //  How long since the last byte arrived from the Apple.
    DWORD    GetSilenceMs () const;

    void     SetActivitySink (ActivityFn sink) { m_adtProServer.m_activity = std::move (sink); }

    //  The chip the Apple's program is built for: a 6551, or an Uthernet II's
    //  W5100.
    void     SetProgramChip (LinkChip chip) { m_programChip = chip; m_program.clear(); }

    //  The activity line sent when the Apple-side program does not assemble.
    static std::string  DescribeAssemblyFailure (const std::string & errors);

    //  How the last request ended: S_OK, or why its transfer failed.
    HRESULT  GetLastResult () const { return m_adtProServer.m_lastResult; }

    HRESULT  ServeOne (DWORD timeoutMs) { return m_adtProServer.ServeOne (timeoutMs); }

    //  Casso's client's operations. A sector is the number in its address
    //  field; data is 256 bytes per sector, in the order of `sectors`.
    HRESULT  ReadSectors  (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus);
    HRESULT  WriteSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::span<const Byte> data, CassoLinkStatus & outStatus);
    //  `gap` is the self-sync bytes after each sector; 0 is the program's own.
    HRESULT  FormatTrack  (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap = 0);

    //  A whole track, its 16 sectors in physical order, formatted under
    //  `volume` with their data in one revolution and read back.
    HRESULT  WriteTrack   (Byte drive, Byte track, Byte volume, std::span<const Byte> data, CassoLinkStatus & outStatus, Byte gap = 0);

    //  The drive's revolution in milliseconds, from the client's marker at
    //  each pass of sector 0 on `track`, over `revolutions` passes. Zero, with
    //  the Status's error, when the track has no sector 0 to time.
    HRESULT  TimeDrive    (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus);

    //  The gap after each sector for a drive with this revolution: as long as
    //  lets 16 sectors fit in a revolution with a margin, from 4 to 40.
    static Byte  ComputeFormatGap (double revolutionMs);

    //  The //c's 65C02, at 14.318 MHz / 14 less a stretched cycle a scan line.
    static constexpr double  kCpuCyclesPerMs = 1020.484;

    //  The part of the user's operation the requests that follow cover, as
    //  percentages of the whole, for the program's status row; 0 and 100
    //  until set.
    void     SetProgress  (Byte start, Byte end) { m_progressStart = start; m_progressEnd = end; }

    //  The client acknowledges at the old speed and then switches; the
    //  caller switches the line after this returns.
    HRESULT  SetSpeed     (Byte control);

    //  The user's operation as text for the program's screen, in capitals
    //  and cut to CassoLinkCodec::kLabelLength. The program draws it and
    //  sends Ready, which the next operation takes.
    HRESULT  SendLabel    (const std::string & text);

    //  A line for the program's history, newest at the top: the PC's time,
    //  as the Apple may have no clock, then the text, as SendLabel cuts it,
    //  to CassoLinkCodec::kLogLength.
    HRESULT  SendLog      (int hour, int minute, const std::string & text);

    //  Idle, sent unprompted. A shim or program whose Ready went out before
    //  the host connected is in its receive loop; Idle ends that loop, and
    //  Ready is sent again.
    HRESULT  PromptReady  () { return SendPacket (CassoLinkCodec::kIdle, {}); }

    //  Ends CassoLink on the Apple, and control returns to BASIC or the
    //  monitor there; nothing follows on the line.
    HRESULT  Exit         ();

    //  Stops the client's running operation from another thread. The client
    //  sends Ready next, and the operation then returns ERROR_CANCELLED.
    HRESULT  Cancel       ();

private:
    HRESULT  Fill            (size_t count, DWORD timeoutMs, bool & outFilled);
    void     Consume         (size_t count);
    HRESULT  ReadPacket      (DWORD timeoutMs, CassoLinkPacket & outPacket);
    HRESULT  Send            (std::span<const Byte> bytes);
    HRESULT  SendByte        (Byte value);

    HRESULT  ServeReady        (const CassoLinkPacket & packet);
    void     ReadClient        (const CassoLinkPacket & packet);

    HRESULT  AwaitReady        ();
    HRESULT  ReadReply         (CassoLinkPacket & outPacket);
    HRESULT  ReadSectorsOnce   (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus);
    HRESULT  FormatTrackOnce   (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap);
    HRESULT  TimeDriveOnce     (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus);
    HRESULT  SendSectorsOnce   (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                std::span<const Byte> data, CassoLinkStatus & outStatus, std::span<const Byte> trailer);

    //  An operation sent again while its request is lost, up to
    //  kMaxResends times.
    template <typename Attempt>
    HRESULT  Resend (Attempt attempt)
    {
        HRESULT  hr = S_OK;

        for (int sent = 0; sent <= kMaxResends; sent++)
        {
            hr = attempt();

            if (hr != kLostRequest)
            {
                break;
            }

            if (m_adtProServer.m_activity)
            {
                m_adtProServer.m_activity ("The Apple did not take a request, so it was sent again");
            }
        }

        return hr;
    }

    HRESULT  ReadStatus        (const CassoLinkPacket & packet, CassoLinkStatus & outStatus);
    HRESULT  SendPacket        (Byte command, std::span<const Byte> payload);
    HRESULT  LoadProgram       ();
    HRESULT  SendSectors       (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                std::span<const Byte> data, CassoLinkStatus & outStatus, std::span<const Byte> trailer = {});

    std::shared_ptr<IByteStream>    m_stream;
    ICassoLinkFolder              & m_folder;
    ClockFn                         m_clock;
    bool                            m_hasClient   = false;
    CassoLinkClient                 m_client;
    std::vector<Byte>               m_program;
    LinkChip                        m_programChip = LinkChip::Acia;
    std::atomic<bool>                 m_cancelRequested { false };
    std::atomic<bool>                 m_writing         { false };
    std::mutex                        m_sendLock;
    bool                              m_readyPending   = false;
    std::atomic<uint64_t>             m_lastHeardMs { GetTickCount64() };
    Byte                              m_progressStart  = 0;
    Byte                              m_progressEnd    = 100;

    //  The host's parts, each with the state it uses.
    friend class CassoLinkAdtProServer;
    CassoLinkAdtProServer  m_adtProServer { *this };
};

