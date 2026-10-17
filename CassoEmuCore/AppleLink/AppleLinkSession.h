#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkHost.h"
#include "AppleLink/AppleLinkSettings.h"
#include "AppleLink/LinkedDisk.h"
#include "AppleLink/LinkBootstrap.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "Seams/ByteStreamSettings.h"

class IByteStream;
class IByteStreamFactory;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkState, LinkOperation
//
//  The session's state, and one queued request with its result. An
//  operation's progress, result and state change on the session's thread;
//  the window reads them when the sink posts the operation to it.
//
////////////////////////////////////////////////////////////////////////////////

enum class LinkState
{
    Closed,
    Searching,
    Bootstrapping,
    Opening,
    WaitingForReady,
    LoadingProgram,
    Ready,
    Busy,
    Disconnected,
};

enum class LinkOperationKind
{
    Enumerate,
    ReadFile,
    WriteFile,
    Delete,
    Rename,
    MakeDirectory,
    RemoveDirectory,
    ReadDisk,
    WriteDisk,
    CheckLinkDisk,
    Bootstrap,
    SaveLink,
};

enum class LinkOperationState
{
    Queued,
    Running,
    Done,
    Failed,
    Canceled,
};

struct LinkOperation
{
    uint64_t                         id       = 0;
    LinkOperationKind                kind     = LinkOperationKind::Enumerate;
    int                              drive    = 1;
    FilePath                         path;
    std::string                      newName;
    FilePayload                      payload;

    std::atomic<int>                 progress { 0 };
    std::atomic<LinkOperationState>  state    { LinkOperationState::Queued };
    HRESULT                          result   = S_OK;
    std::string                      message;

    VolumeListing                    listing;
    VolumeKind                       volumeKind = VolumeKind::Unknown;
    FilePayload                      file;

    //  Read disk and Write disk: the image file, and for a read, whether an
    //  unreadable sector is written as zeros rather than failing the read.
    //  unreadable lists those sectors, as track and DOS 3.3 sector.
    std::string                      imagePath;
    bool                             blankUnreadable = false;
    std::vector<std::pair<int, int>> unreadable;

    //  Check link disk: whether the drive has the link's boot disk.
    bool                             linkDisk = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IAppleLinkSessionSink
//
//  Called on the session's thread. The window's implementation posts each
//  call to its own thread, so the line is never waited on there.
//
////////////////////////////////////////////////////////////////////////////////

class IAppleLinkSessionSink
{
public:
    virtual ~IAppleLinkSessionSink () = default;

    virtual void  OnStateChanged (LinkState state)                                    = 0;
    virtual void  OnProgress     (const std::shared_ptr<LinkOperation> & operation)   = 0;
    virtual void  OnDone         (const std::shared_ptr<LinkOperation> & operation)   = 0;

    //  A line for the activity log: a connection made or dropped, an
    //  operation finished, or a request of ADTPro's served.
    virtual void  OnActivity     (const std::string & text)                            { UNREFERENCED_PARAMETER (text); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession
//
//  One open link: the stream, the host engine, a LinkedDisk per drive and a
//  queue of operations, run one at a time on the session's own thread
//  (FR-098). Every request returns at once with its queued operation.
//
//  The stream is opened when the session starts. With no Apple answering,
//  or after the line drops, it is opened again every kRetryMs, and at once
//  when a request arrives. Opening waits for the Apple's
//  Ready at the bootstrap speed, loads the program when its version
//  differs, and switches to the link's speed. While nothing is queued, Idle
//  is sent in reply to each Ready.
//
//  A write first reads the catalog again, and fails when it differs from
//  the last listing of the drive, so a disk changed since it was listed is
//  reported rather than written over.
//
////////////////////////////////////////////////////////////////////////////////

class AppleLinkSession
{
public:
    static constexpr DWORD  kConnectTimeoutMs = 20000;
    static constexpr DWORD  kIdlePollMs       = 100;
    static constexpr DWORD  kPromptMs         = 2000;
    static constexpr DWORD  kRetryMs          = 5000;

    //  The program sends Ready every 100 ms while idle; this long with no
    //  byte from it is an Apple that was reset, turned off or unplugged.
    static constexpr DWORD  kSilenceMs        = 3000;
    static constexpr int    kDriveCount       = 2;

    //  The folder is the one ADTPro's own clients are served from, between
    //  Casso's operations.
    AppleLinkSession (IByteStreamFactory & factory, IAppleLinkFolder & folder, IDiskFileIo & fileIo, IAppleLinkSessionSink & sink);
    ~AppleLinkSession ();

    HRESULT    Start  (const AppleLinkSettings & settings);

    //  How long opening waits for the Apple's Ready.
    void       SetConnectTimeout (DWORD timeoutMs) { m_connectTimeoutMs = timeoutMs; }

    //  Starts the session on an Apple with no link program: the loader is
    //  typed in first (FR-092), and the returned operation finishes when the
    //  program is running or the Apple does not answer.
    HRESULT    StartWithBootstrap (const AppleLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation);

    //  How long a disconnected session waits before connecting again.
    void       SetRetryInterval (DWORD retryMs) { m_retryMs = retryMs; }

    //  The pause after each line of the loader.
    void       SetBootstrapPause (DWORD pauseMs) { m_bootstrapPauseMs = pauseMs; }
    void       Stop   ();

    //  Stops the running operation; queued ones remain.
    void       Cancel ();

    LinkState  GetState    () const { return m_state; }

    //  Whether ADTPro's own client is on the line instead of Casso's. The
    //  session goes on serving it, and opening goes on waiting for Casso's
    //  program.
    bool       IsServingAdtPro () const { return m_adtPro; }
    Byte       GetDriveMap () const { return m_driveMap; }

    //  The Apple's model, as its program reported it on connecting; empty
    //  before then.
    std::wstring  GetModelName () const { std::lock_guard<std::mutex>  lock (m_lock); return m_modelName; }

    std::shared_ptr<LinkOperation>  Enumerate     (int drive, const FilePath & directory);
    std::shared_ptr<LinkOperation>  ReadFile      (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  WriteFile     (int drive, const FilePath & path, const FilePayload & payload);
    std::shared_ptr<LinkOperation>  Delete        (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  Rename        (int drive, const FilePath & path, const std::string & newName);
    std::shared_ptr<LinkOperation>  MakeDirectory (int drive, const FilePath & path);

    //  A ProDOS directory and everything in it.
    std::shared_ptr<LinkOperation>  RemoveDirectory (int drive, const FilePath & path);

    //  Every track's standard sectors into an image file, in the format of
    //  its extension, replaced atomically and only when the read succeeds
    //  (FR-086, FR-087).
    std::shared_ptr<LinkOperation>  ReadDisk      (int drive, const std::string & imagePath, bool blankUnreadable);

    //  An image file onto the disk: every track formatted, written and read
    //  back. The caller has the user's confirmation first. An image whose
    //  tracks do not all decode to standard sectors fails before any track
    //  is written.
    std::shared_ptr<LinkOperation>  WriteDisk     (int drive, const std::string & imagePath);

    //  Whether the drive has the link's boot disk, by its boot sector: a write
    //  over it, the Apple has no disk to start the link from.
    std::shared_ptr<LinkOperation>  CheckLinkDisk (int drive);

    //  Physical sector to DOS-logical and back, within a track.
    static int  GetLogicalSector  (int physical);
    static int  GetPhysicalSector (int logical);

    //  A Status error code as an HRESULT.
    static HRESULT  FromStatusError (Byte error);

    //  The Label text the program draws for an operation (FR-090).
    static std::string  DescribeOperation (const LinkOperation & operation, const std::optional<VolumeListing> & listing = std::nullopt);

    //  A drive as the label has it: D2, and the disk's volume, as
    //  D2 (/GAMES) or D2 (VOL 254), once its catalog is read; the label has 38 characters.
    static std::string  DescribeDrive     (int drive, const std::optional<VolumeListing> & listing);

private:
    //  A drive's sectors over the link, for its LinkedDisk.
    class DriveSource : public ILinkedSectorSource
    {
    public:
        DriveSource (AppleLinkSession & session, int drive) : m_session (session), m_drive (drive) {}

        HRESULT  ReadSectors  (std::span<const size_t> sectors, std::vector<Byte> & outData) override;
        HRESULT  WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)  override;

    private:
        AppleLinkSession  & m_session;
        int                 m_drive;
    };

    std::shared_ptr<LinkOperation>  Queue (LinkOperationKind kind, int drive, const FilePath & path, std::shared_ptr<LinkOperation> operation = nullptr);

    void     Run           (std::stop_token stop);
    HRESULT  Connect       (std::stop_token stop, DWORD openAt = 0);
    HRESULT  RunBootstrap  (std::stop_token stop, LinkOperation & operation);
    HRESULT  OpenStream    (DWORD baud);
    HRESULT  AwaitProgram  (std::stop_token stop, DWORD timeoutMs, bool & outReady);
    void     Disconnect    ();
    bool     IsPeerGone    ();
    void     SetState      (LinkState state);

    HRESULT  Execute       (LinkOperation & operation);
    HRESULT  Revalidate    (int drive);
    HRESULT  CheckCatalog  (int drive);
    HRESULT  RunWrite      (LinkOperation & operation, const LinkedDisk::WriteOperation & write);
    HRESULT  ReadWholeDisk (LinkOperation & operation);
    HRESULT  WriteWholeDisk (LinkOperation & operation);
    HRESULT  ReadBootSector (LinkOperation & operation);
    HRESULT  ReadTrack     (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable);
    void     Report        (LinkOperation & operation, int progress);

    static bool  IsSameCatalog (const VolumeListing & a, const VolumeListing & b);
    static bool  IsLineLost    (HRESULT hr);

    IByteStreamFactory                          & m_factory;
    IAppleLinkFolder                            & m_folder;
    IDiskFileIo                                 & m_fileIo;
    IAppleLinkSessionSink                       & m_sink;
    AppleLinkSettings                             m_settings;
    std::wstring                                  m_port;
    std::shared_ptr<IByteStream>                  m_stream;
    std::unique_ptr<AppleLinkHost>                m_host;
    std::array<std::unique_ptr<DriveSource>, 2>   m_sources;
    std::array<std::unique_ptr<LinkedDisk>, 2>    m_disks;
    std::array<std::optional<VolumeListing>, 2>   m_listings;
    std::atomic<LinkState>                        m_state    { LinkState::Closed };
    std::atomic<Byte>                             m_driveMap { 0 };
    std::atomic<bool>                             m_adtPro   { false };
    mutable std::mutex                          m_lock;
    std::condition_variable_any                 m_wake;
    std::deque<std::shared_ptr<LinkOperation>>  m_queue;
    std::shared_ptr<LinkOperation>              m_running;
    uint64_t                                    m_nextId           = 1;
    DWORD                                       m_connectTimeoutMs = kConnectTimeoutMs;
    DWORD                                       m_bootstrapPauseMs = LinkBootstrap::kLinePauseMs;
    DWORD                                       m_retryMs          = kRetryMs;
    bool                                        m_peerSeen         = false;
    std::wstring                                m_modelName;
    std::shared_ptr<LinkOperation>              m_bootstrap;
    uint64_t                                    m_commitTag        = 1;
    std::jthread                                m_thread;
};
