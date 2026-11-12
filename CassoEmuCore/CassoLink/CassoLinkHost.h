#pragma once

#include "Pch.h"

#include "CassoLink/Host/CassoLinkAdtProServer.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkFault.h"

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

    //  The CRC-16/XMODEM the program computed over itself as loaded, which
    //  the host compares with the image it sent. Earlier programs sent none.
    bool                 hasProgramCrc = false;
    Word                 programCrc    = 0;

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

//  A capture read's times: from its request to its first packet, which is
//  mostly the arm's step and the read, and from its first packet to its
//  Status, which is mostly the line; and the bytes of the encoding.
struct CaptureTiming
{
    double  diskMs    = 0.0;
    double  lineMs    = 0.0;
    size_t  sentBytes = 0;
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

    //  Loads in a row of a program whose CRC differs from the image, after
    //  which the link fails with HRESULT_FROM_WIN32 (ERROR_CRC).
    static constexpr int     kMaxProgramReloads  = 2;

    //  The time after the last packet sent or decoded at which the client's
    //  Readies are stale (AwaitReady), as it sends one a second, and the
    //  reads of the line that drop them.
    static constexpr DWORD   kStaleReadyMs       = 1500;
    static constexpr int     kDiscardReads       = 64;

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
    static constexpr Byte    kErrorChanged        = 0x70;   // Krunch: the directory isn't the one checked

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
    CassoLinkClient  GetClient       () const { return m_client; }

    //  How long since the last byte arrived from the Apple.
    DWORD    GetSilenceMs () const;

    void     SetActivitySink (ActivityFn sink) { m_adtProServer.m_activity = std::move (sink); }

    //  Each packet sent and received, as CassoLinkPacketText describes it, for
    //  a log; sent is false for one received. A pause's report of no key, once
    //  a second, and each Idle are left out, and a Ready appears only when it
    //  differs from the last one.
    using TraceFn = std::function<void (bool sent, const std::string & text)>;

    void     SetTraceSink    (TraceFn sink) { m_trace = std::move (sink); }

    //  The chip the Apple's program is built for: a 6551, or an Uthernet II's
    //  W5100.
    void     SetProgramChip (LinkChip chip) { m_programChip = chip; m_program.clear(); }

    //  Whether the program sent to the Apple has the Fault command, which a
    //  Debug build's links and the tests of recovery turn on.
    void     SetFaultProgram (bool faults) { m_faultProgram = faults; m_program.clear(); }

    //  A fault for what follows: a PC byte's offset is counted from now, and
    //  the Apple's fault is sent ahead of the request it is for, counted from
    //  now. ERROR_NOT_SUPPORTED for the Apple's when the program has no Fault
    //  command.
    HRESULT  ArmFault (const LinkFault & fault);

    //  The activity line sent when the Apple-side program does not assemble.
    static std::string  DescribeAssemblyFailure (const std::string & errors);

    //  How the last request ended: S_OK, or why its transfer failed.
    HRESULT  GetLastResult () const { return m_adtProServer.m_lastResult; }

    HRESULT  ServeOne (DWORD timeoutMs) { return m_adtProServer.ServeOne (timeoutMs); }

    //  Casso's client's operations. A sector is the number in its address
    //  field; data is 256 bytes per sector, in the order of `sectors`. A sector
    //  the Apple didn't find is zeros and has its entry false in outRead, and
    //  the Status error is NOT FOUND.
    HRESULT  ReadSectors  (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, CassoLinkStatus & outStatus);
    HRESULT  ReadSectors  (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead, CassoLinkStatus & outStatus);
    HRESULT  WriteSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::span<const Byte> data, CassoLinkStatus & outStatus);

    //  Each sector's CRC, read and computed on the Apple, so a read again takes a
    //  few bytes of the link. A sector not read has its entry false in outRead.
    HRESULT  ReadCrcs     (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead, CassoLinkStatus & outStatus);
    //  `gap` is the self-sync bytes after each sector; 0 is the program's own.
    HRESULT  FormatTrack  (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap = 0);

    //  A whole track, its 16 sectors in physical order, formatted under
    //  `volume` with their data in one revolution and read back.
    HRESULT  WriteTrack   (Byte drive, Byte track, Byte volume, std::span<const Byte> data, CassoLinkStatus & outStatus, Byte gap = 0);

    //  The drive's revolution in milliseconds, from the client's marker at
    //  each pass of sector 0 on `track`, over `revolutions` passes. Zero, with
    //  the Status's error, when the track has no sector 0 to time.
    HRESULT  TimeDrive    (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus);

    //  The Apple Pascal volume in the drive krunched on the Apple, after it
    //  checks that its directory's CRC is `directoryCrc`, the CRC of the
    //  directory the host checked. `progress` is called with the percentage
    //  and the files moved in each Krunch progress packet. Once the program has the request it
    //  reads nothing from the host but a Cancel, between files, so the request
    //  is sent again only when the Apple didn't take it; a Status lost after
    //  the Apple took it is ERROR_INVALID_DATA. A Cancel takes effect after
    //  the file being moved.
    using KrunchProgressFn = std::function<void (Byte percent, Byte filesMoved)>;

    HRESULT  Krunch       (Byte drive, Word directoryCrc, const KrunchProgressFn & progress, CassoLinkStatus & outStatus);

    //  The gap after each sector for a drive with this revolution: as long as
    //  lets 16 sectors fit in a revolution with a margin, from 4 to 40.
    static Byte  ComputeFormatGap (double revolutionMs);

    //  The //c's 65C02, at 14.318 MHz / 14 less a stretched cycle a scan line.
    static constexpr double  kCpuCyclesPerMs = 1020.484;

    //  The part of the user's operation the requests that follow cover, as
    //  percentages of the whole, for the bar on the program's status row; 0 and
    //  0, with which the bar is not redrawn, until set. Only a whole-disk transfer
    //  sets them, as the bar has a cell for each track.
    void     SetProgress    (Byte start, Byte end) { m_progressStart = start; m_progressEnd = end; }
    Byte     GetProgressEnd () const                { return m_progressEnd; }

    //  Whether the reads that follow are checks the PC reports itself, such as
    //  its read of each drive's catalog when it connects. Their requests set
    //  kQuietDrive in the drive, and no error is drawn on the Apple for them.
    static constexpr Byte  kQuietDrive = 0x80;

    void     SetQuiet       (bool quiet)            { m_quiet = quiet; }
    bool     IsQuiet        () const                { return m_quiet; }

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

    //  A Full Send's screen, in the Session screen packet: whether the session
    //  is active, its tone, and its rows' text. Not active clears the rows and
    //  ends the session.
    //  80 columns where the Apple has them, or 40. The screen is drawn again.
    HRESULT  SetColumns        (bool wide);

    //  The module the program has loaded, 0 and 0 for none; and a module
    //  loaded, chunk by chunk, then checked. EnsureModule loads it when the
    //  program has another, or none, as a disk operation overwrites it.
    HRESULT  QueryModule       (Byte & outId, Byte & outVersion);
    HRESULT  LoadModule        (std::span<const Byte> module);
    HRESULT  EnsureModule      (std::span<const Byte> module);

    //  A raw read of a quarter-track position by the capture module, which
    //  must be loaded: the bytes as the Apple stored them, a sync byte with
    //  its top bit clear. A packet that fails its CRC, arrives out of order
    //  or doesn't decode fails the read. The delay is the second read of each
    //  byte's: 0 for 8 cycles, 1 for 12, with kRecalibrate added to take
    //  the arm to track 0 first. With a position to wait on, not kNoSync,
    //  the read starts a fixed time after the marker passes there
    //  (Capture.s); a marker that doesn't pass is the Status's error.
    static constexpr Byte  kNoSync      = 0xFF;
    static constexpr Byte  kRecalibrate = 0x80;

    HRESULT  CaptureQuarterTrack (Byte drive, Byte position, Byte delay, Byte startPercent, Byte endPercent, Byte syncPosition, std::span<const Byte> marker,
                                  std::vector<Byte> & outBytes, CassoLinkStatus & outStatus, CaptureTiming & outTiming);

    //  The capture module for the program this host loads.
    HRESULT  BuildCaptureModule  (std::vector<Byte> & outModule) const;

    //  A request of the module loaded, and the first packet in reply.
    HRESULT  RequestModule     (Byte command, std::span<const Byte> payload, CassoLinkPacket & outReply);

    HRESULT  SendSessionScreen (bool active, Byte tone, const std::vector<std::string> & rows);

    //  Space or Q on the Apple, with no drive running there: its code.
    //  Any Cancel ends the wait with ERROR_CANCELLED.
    HRESULT  WaitForKey   (const std::vector<std::string> & lines, Byte & outKey);

    //  As WaitForKey, with every key the Apple's keyboard sends, for a line
    //  typed there.
    HRESULT  WaitForAnyKey (const std::vector<std::string> & lines, Byte & outKey);

    //  Whether Q was pressed during a Full Send, since the last call.
    bool     TakeQuit   () { bool  quit = m_quit; m_quit = false; return quit; }

    //  Whether V was pressed during a Full Send, since the last call.
    bool     TakeVerifyToggle () { bool  toggle = m_verifyToggle; m_verifyToggle = false; return toggle; }

    //  Whether F was pressed on the Apple's standard screen, since the last call.
    bool     TakeFullSendRequest () { bool  request = m_fullSendRequest; m_fullSendRequest = false; return request; }

    //  Ends CassoLink on the Apple, and control returns to BASIC or the
    //  monitor there; nothing follows on the line.
    HRESULT  Exit         ();

    //  Stops the client's running operation from another thread. The client
    //  sends Ready next, and the operation then returns ERROR_CANCELLED.
    HRESULT  Cancel       ();

private:
    HRESULT  Fill            (size_t count, DWORD timeoutMs, bool & outFilled);
    HRESULT  DiscardReceived ();
    void     Consume         (size_t count);
    HRESULT  ReadPacket      (DWORD timeoutMs, CassoLinkPacket & outPacket);
    void     TraceReceived   (const CassoLinkPacket & packet);
    HRESULT  AwaitRequest    ();
    HRESULT  Send            (std::span<const Byte> bytes);
    HRESULT  SendByte        (Byte value);

    HRESULT  ServeReady        (const CassoLinkPacket & packet);
    void     ReadClient        (const CassoLinkPacket & packet);
    HRESULT  CheckProgramCrc   (bool & outIntact);
    HRESULT  AssembleProgram   ();

    HRESULT  AwaitReady        ();
    HRESULT  ReadReply         (CassoLinkPacket & outPacket);
    HRESULT  WaitForKeyOnce    (const std::vector<std::string> & lines, Byte mode, Byte & outKey);
    Byte     GetRequestDrive   (Byte drive) const { return m_quiet ? (Byte) (drive | kQuietDrive) : drive; }
    HRESULT  ReadSectorsOnce   (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead, CassoLinkStatus & outStatus);
    HRESULT  ReadCrcsOnce      (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead, CassoLinkStatus & outStatus);
    HRESULT  FormatTrackOnce   (Byte drive, Byte track, Byte volume, CassoLinkStatus & outStatus, Byte gap);
    HRESULT  TimeDriveOnce     (Byte drive, Byte track, Byte revolutions, double & outRevolutionMs, CassoLinkStatus & outStatus);
    HRESULT  KrunchOnce        (Byte drive, Word directoryCrc, const KrunchProgressFn & progress, CassoLinkStatus & outStatus, bool & outTaken);
    HRESULT  SendSectorsOnce   (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                std::span<const Byte> data, CassoLinkStatus & outStatus, std::span<const Byte> trailer);

    static bool  IsKeyReport  (const CassoLinkPacket & packet) { return packet.command == CassoLinkCodec::kWaitForKey && packet.payload.size() == 1 && (packet.payload[0] == CassoLinkCodec::kKeyQuit || packet.payload[0] == CassoLinkCodec::kKeyFullSend || packet.payload[0] == CassoLinkCodec::kKeyVerify); }
    void         NoteKey      (const CassoLinkPacket & packet) { m_quit = m_quit || packet.payload[0] == CassoLinkCodec::kKeyQuit; m_fullSendRequest = m_fullSendRequest || packet.payload[0] == CassoLinkCodec::kKeyFullSend; m_verifyToggle = m_verifyToggle != (packet.payload[0] == CassoLinkCodec::kKeyVerify); }
    static bool  IsWholeReady (const CassoLinkPacket & packet) { size_t size = packet.payload.size(); return size == 4 || size == 9 || size == 11; }
    static bool  IsBlank      (std::span<const Byte> sector) { return std::all_of (sector.begin(), sector.end(), [] (Byte b) { return b == 0; }); }

    //  An operation sent again while its request is lost or a reply arrives
    //  damaged, up to kMaxResends times. The next attempt's AwaitReady skips
    //  what is left of the damaged reply.
    template <typename Attempt>
    HRESULT  Resend (Attempt attempt)
    {
        HRESULT  hr      = S_OK;
        bool     damaged = false;

        for (int sent = 0; sent <= kMaxResends; sent++)
        {
            hr      = attempt();
            damaged = hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);

            if (hr != kLostRequest && !damaged)
            {
                break;
            }

            if (m_adtProServer.m_activity)
            {
                m_adtProServer.m_activity (damaged ? "A reply from the Apple arrived damaged, so the request was sent again"
                                                   : "The Apple did not take a request, so it was sent again");
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
    bool                            m_hasClient       = false;
    CassoLinkClient                 m_client;
    std::vector<Byte>               m_program;
    LinkChip                        m_programChip     = LinkChip::Acia;
    int                             m_badProgramLoads = 0;
    std::atomic<bool>               m_cancelRequested { false };
    std::atomic<bool>               m_writing         { false };
    std::mutex                      m_sendLock;
    bool                            m_readyPending    = false;
    bool                            m_quit            = false;
    bool                            m_fullSendRequest = false;
    bool                            m_verifyToggle    = false;
    bool                            m_awaitingKey     = false;
    std::atomic<uint64_t>           m_lastHeardMs     { GetTickCount64() };
    std::atomic<uint64_t>           m_lastPacketMs    { 0 };   // the last packet sent or decoded
    Byte                            m_progressStart   = 0;
    Byte                            m_progressEnd     = 0;
    bool                            m_quiet           = false;
    TraceFn                         m_trace;
    std::string                     m_tracedReady;     // text of the last Ready traced
    bool                            m_faultProgram    = false;
    LinkByteFault                   m_pcFault;
    std::optional<LinkFault>        m_appleFault;
    int                             m_requests        = 0;   // requests sent since the Apple's fault was armed

    //  The host's parts, each with the state it uses.
    friend class           CassoLinkAdtProServer;
    CassoLinkAdtProServer  m_adtProServer        { *this };
};

