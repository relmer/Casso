#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkCodec.h"

class IByteStream;





////////////////////////////////////////////////////////////////////////////////
//
//  IAppleLinkFolder
//
//  The image folder AppleLinkHost serves from: its files by name, read and
//  written whole. A write replaces the file in one step, so a failed transfer
//  writes nothing. With no folder open, as when Casso Explorer's tab is on
//  This PC, every request for a file or folder has ADTPro's unable status
//  for it.
//
////////////////////////////////////////////////////////////////////////////////

class IAppleLinkFolder
{
public:
    virtual ~IAppleLinkFolder () = default;

    virtual bool         IsOpen          () const                                                    = 0;

    //  As ADTPro's Directory prints it: the full path, with a trailing
    //  separator.
    virtual std::string  GetPath         () const                                                    = 0;
    virtual HRESULT      List            (std::vector<std::string> & outNames)                       = 0;
    virtual bool         Exists          (const std::string & name)                                  = 0;
    virtual HRESULT      Read            (const std::string & name, std::vector<Byte> & outBytes)     = 0;
    virtual HRESULT      Write           (const std::string & name, const std::vector<Byte> & bytes)  = 0;
    virtual HRESULT      ChangeDirectory (const std::string & name)                                  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkClient
//
//  Casso's client as its last Ready packet reported it, and the Status packet
//  that ends each of its operations. A shim with no program loaded reports
//  program version 0.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleLinkClient
{
    Word  programVersion = 0;
    Byte  shimVersion    = 0;
    Byte  driveMap       = 0;
};

struct AppleLinkStatus
{
    Byte  operation = 0;
    Byte  drive     = 0;
    Byte  track     = 0;
    Byte  done      = 0;
    Byte  error     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkHost
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

class AppleLinkHost
{
public:
    //  The four bytes Read3 sends ahead of a block: minute, hour, then the
    //  ProDOS date, low byte first.
    using ClockFn = std::function<std::array<Byte, 4> ()>;

    static constexpr DWORD   kReplyTimeoutMs     = 3000;
    static constexpr int     kMaxResends         = 5;
    static constexpr size_t  kBlockSize          = 512;
    static constexpr size_t  kDirectoryPage      = 1024;
    static constexpr size_t  kNamesPerPage       = 15;
    static constexpr size_t  kSectorSize         = 256;
    static constexpr size_t  kSectorsPerTrack    = 16;
    static constexpr size_t  kDosImageSize       = 143360;
    static constexpr DWORD   kOperationTimeoutMs = 30000;

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

    //  Virtual-drive request kinds.
    static constexpr Byte    kDriveRead        = 0x01;
    static constexpr Byte    kDriveWrite1      = 0x02;
    static constexpr Byte    kDriveRead3Drive1 = 0x03;
    static constexpr Byte    kDriveWrite2      = 0x04;
    static constexpr Byte    kDriveRead3Drive2 = 0x05;

    AppleLinkHost (std::shared_ptr<IByteStream> stream, IAppleLinkFolder & folder, ClockFn clock);

    //  The images virtual drives 1 and 2 serve, by name in the folder;
    //  ADTPro's defaults until set.
    void     SetVirtualDrive (int drive, const std::string & name);
    void     SetReplyTimeout (DWORD timeoutMs) { m_replyTimeoutMs = timeoutMs; }

    //  How long an operation for Casso's client waits for each packet. The
    //  default allows for a machine paused in a debugger.
    void     SetOperationTimeout (DWORD timeoutMs) { m_operationTimeoutMs = timeoutMs; }

    //  Casso's client, once one has sent Ready.
    bool             HasClient () const { return m_hasClient; }

    //  Whether ADTPro's own client has sent a request since the host was
    //  made.
    bool             HasAdtProClient () const { return m_adtProClient; }
    AppleLinkClient  GetClient () const { return m_client; }

    //  How long since the last byte arrived from the Apple.
    DWORD    GetSilenceMs () const;

    //  How the last request ended: S_OK, or why its transfer failed.
    HRESULT  GetLastResult () const { return m_lastResult; }

    HRESULT  ServeOne (DWORD timeoutMs);

    //  Casso's client's operations. A sector is the number in its address
    //  field; data is 256 bytes per sector, in the order of `sectors`.
    HRESULT  ReadSectors  (Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & outData, AppleLinkStatus & outStatus);
    HRESULT  WriteSectors (Byte drive, Byte track, std::span<const Byte> sectors, std::span<const Byte> data, AppleLinkStatus & outStatus);
    HRESULT  FormatTrack  (Byte drive, Byte track, Byte volume, AppleLinkStatus & outStatus);

    //  A whole track, its 16 sectors in physical order, formatted under
    //  `volume` with their data in one revolution and read back.
    HRESULT  WriteTrack   (Byte drive, Byte track, Byte volume, std::span<const Byte> data, AppleLinkStatus & outStatus);

    //  The part of the user's operation the requests that follow cover, as
    //  percentages of the whole, for the program's status row; 0 and 100
    //  until set.
    void     SetProgress  (Byte start, Byte end) { m_progressStart = start; m_progressEnd = end; }

    //  The client acknowledges at the old speed and then switches; the
    //  caller switches the line after this returns.
    HRESULT  SetSpeed     (Byte control);

    //  The user's operation as text for the program's screen, in capitals
    //  and cut to AppleLinkCodec::kLabelLength. The program draws it and
    //  sends Ready, which the next operation takes.
    HRESULT  SendLabel    (const std::string & text);

    //  Idle, sent unprompted. A shim or program whose Ready went out before
    //  the host connected is in its receive loop; Idle ends that loop, and
    //  Ready is sent again.
    HRESULT  PromptReady  () { return SendPacket (AppleLinkCodec::kIdle, {}); }

    //  Stops the client's running operation from another thread. The client
    //  sends Ready next, and the operation then returns ERROR_CANCELLED.
    HRESULT  Cancel       ();

    //  The ProDOS date and time for Read3 from a local time.
    static std::array<Byte, 4>  GetProDosDateTime (const SYSTEMTIME & time);

    //  A Directory page: the folder's path and a page of its files, as
    //  ADTPro's client prints it. The last byte is 1 when another page
    //  follows.
    static std::vector<Byte>    BuildDirectoryPage (const std::string & path, const std::vector<std::string> & names, size_t page);

    //  An image file as 512-byte blocks, and back. A .dsk or .do of 143,360
    //  bytes is in DOS 3.3 sector order, as ADTPro's server reads and writes
    //  one, so each block's two sectors are gathered from where DOS 3.3 puts
    //  them; any other image is in block order already.
    static bool                 IsDosOrder         (const std::string & name, size_t size);
    static void                 ToBlockOrder       (const std::string & name, const std::vector<Byte> & file, std::vector<Byte> & outBlocks);
    static void                 ToFileOrder        (const std::string & name, const std::vector<Byte> & blocks, std::vector<Byte> & outFile);

private:
    HRESULT  Fill            (size_t count, DWORD timeoutMs, bool & outFilled);
    void     Consume         (size_t count);
    HRESULT  ReadPacket      (DWORD timeoutMs, AppleLinkPacket & outPacket);
    HRESULT  Send            (std::span<const Byte> bytes);
    HRESULT  SendByte        (Byte value);
    HRESULT  SendHome        ();

    HRESULT  ServePacket       (const AppleLinkPacket & packet);
    HRESULT  ServeChangeDir    (const AppleLinkPacket & packet);
    HRESULT  ServeDirectory    (const AppleLinkPacket & packet);
    HRESULT  ServeSize         (const AppleLinkPacket & packet);
    HRESULT  ServeGet          (const AppleLinkPacket & packet);
    HRESULT  ServePut          (const AppleLinkPacket & packet, bool batch);
    HRESULT  ServeVirtualDrive ();
    HRESULT  ServeReady        (const AppleLinkPacket & packet);

    HRESULT  AwaitReady        ();
    HRESULT  ReadReply         (AppleLinkPacket & outPacket);
    HRESULT  ReadStatus        (const AppleLinkPacket & packet, AppleLinkStatus & outStatus);
    HRESULT  SendPacket        (Byte command, std::span<const Byte> payload);
    HRESULT  LoadProgram       ();
    HRESULT  SendSectors       (Byte command, std::vector<Byte> request, Byte track, std::span<const Byte> sectors,
                                std::span<const Byte> data, AppleLinkStatus & outStatus);

    HRESULT  ReceiveImage      (std::vector<Byte> & image);
    HRESULT  LoadVirtualDrive  (int drive);
    HRESULT  ReadImage         (const std::string & name, std::vector<Byte> & outFile);
    std::string  GetBatchName  (const std::string & prefix);

    static std::string  ReadName (const std::vector<Byte> & payload, size_t & outEnd);

    //  The DOS 3.3 sector for each of a track's 16 sectors in block order:
    //  block n of a track is sectors 2n and 2n + 1.
    static constexpr size_t  s_kBlockSectorToDos[16] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

    std::shared_ptr<IByteStream>  m_stream;
    IAppleLinkFolder            & m_folder;
    ClockFn                       m_clock;
    std::vector<Byte>             m_buffer;
    std::array<std::string, 2>    m_driveNames = { "Virtual.po", "Virtual2.po" };
    std::array<std::vector<Byte>, 2>  m_driveImages;
    std::array<bool, 2>               m_driveLoaded        = {};
    DWORD                             m_replyTimeoutMs     = kReplyTimeoutMs;
    DWORD                             m_operationTimeoutMs = kOperationTimeoutMs;
    bool                              m_hasClient          = false;
    std::atomic<bool>                 m_adtProClient       { false };
    AppleLinkClient                   m_client;
    std::vector<Byte>                 m_program;
    std::atomic<bool>                 m_cancelRequested { false };
    std::atomic<bool>                 m_writing         { false };
    std::mutex                        m_sendLock;
    bool                              m_readyPending   = false;
    std::atomic<HRESULT>              m_lastResult { S_OK };
    std::atomic<uint64_t>             m_lastHeardMs { GetTickCount64() };
    Byte                              m_progressStart  = 0;
    Byte                              m_progressEnd    = 100;
};
