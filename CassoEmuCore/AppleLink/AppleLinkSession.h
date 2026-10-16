#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkHost.h"
#include "AppleLink/LinkedDisk.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "Seams/ByteStreamSettings.h"

class IByteStream;
class IByteStreamFactory;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSettings
//
//  One link, as CassoExplorerPrefs.json stores it. The address is host:port
//  for the TCP transports; for Emulated it is Casso's own serial port on
//  127.0.0.1.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleLinkSettings
{
    std::string    name;
    LinkTransport  transport         = LinkTransport::Serial;
    std::string    port;
    std::string    address;
    DWORD          baud              = 115200;
    DWORD          bootstrapBaud     = 9600;
    bool           hardwareHandshake = false;
};





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
    ReadDisk,
    WriteDisk,
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
    FilePayload                      file;

    //  Read disk and Write disk: the image file, and for a read, whether an
    //  unreadable sector is written as zeros rather than failing the read.
    //  unreadable lists those sectors, as track and DOS 3.3 sector.
    std::string                      imagePath;
    bool                             blankUnreadable = false;
    std::vector<std::pair<int, int>> unreadable;
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
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession
//
//  One open link: the stream, the host engine, a LinkedDisk per drive and a
//  queue of operations, run one at a time on the session's own thread
//  (FR-098). Every request returns at once with its queued operation.
//
//  The stream is opened when the session starts, and again when a request
//  arrives after the line drops. Opening waits for the Apple's
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
    static constexpr int    kDriveCount       = 2;

    //  The folder is the one ADTPro's own clients are served from, between
    //  Casso's operations.
    AppleLinkSession (IByteStreamFactory & factory, IAppleLinkFolder & folder, IDiskFileIo & fileIo, IAppleLinkSessionSink & sink);
    ~AppleLinkSession ();

    HRESULT    Start  (const AppleLinkSettings & settings);

    //  How long opening waits for the Apple's Ready.
    void       SetConnectTimeout (DWORD timeoutMs) { m_connectTimeoutMs = timeoutMs; }
    void       Stop   ();

    //  Stops the running operation; queued ones remain.
    void       Cancel ();

    LinkState  GetState    () const { return m_state; }
    Byte       GetDriveMap () const { return m_driveMap; }

    std::shared_ptr<LinkOperation>  Enumerate     (int drive, const FilePath & directory);
    std::shared_ptr<LinkOperation>  ReadFile      (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  WriteFile     (int drive, const FilePath & path, const FilePayload & payload);
    std::shared_ptr<LinkOperation>  Delete        (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  Rename        (int drive, const FilePath & path, const std::string & newName);
    std::shared_ptr<LinkOperation>  MakeDirectory (int drive, const FilePath & path);

    //  Every track's standard sectors into an image file, in the format of
    //  its extension, replaced atomically and only when the read succeeds
    //  (FR-086, FR-087).
    std::shared_ptr<LinkOperation>  ReadDisk      (int drive, const std::string & imagePath, bool blankUnreadable);

    //  An image file onto the disk: every track formatted, written and read
    //  back. The caller has the user's confirmation first. An image whose
    //  tracks do not all decode to standard sectors fails before any track
    //  is written.
    std::shared_ptr<LinkOperation>  WriteDisk     (int drive, const std::string & imagePath);

    //  Physical sector to DOS-logical and back, within a track.
    static int  GetLogicalSector  (int physical);
    static int  GetPhysicalSector (int logical);

    //  A Status error code as an HRESULT.
    static HRESULT  FromStatusError (Byte error);

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
    HRESULT  Connect       (std::stop_token stop);
    HRESULT  OpenStream    (DWORD baud);
    HRESULT  AwaitProgram  (std::stop_token stop, DWORD timeoutMs, bool & outReady);
    void     Disconnect    ();
    void     SetState      (LinkState state);

    HRESULT  Execute       (LinkOperation & operation);
    HRESULT  Revalidate    (int drive);
    HRESULT  CheckCatalog  (int drive);
    HRESULT  RunWrite      (LinkOperation & operation, const LinkedDisk::WriteOperation & write);
    HRESULT  ReadWholeDisk (LinkOperation & operation);
    HRESULT  WriteWholeDisk (LinkOperation & operation);
    HRESULT  ReadTrack     (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable);
    void     Report        (LinkOperation & operation, int progress);

    static bool  IsSameCatalog (const VolumeListing & a, const VolumeListing & b);
    static bool  IsLineLost    (HRESULT hr);

    IByteStreamFactory                          & m_factory;
    IAppleLinkFolder                            & m_folder;
    IDiskFileIo                                 & m_fileIo;
    IAppleLinkSessionSink                       & m_sink;
    AppleLinkSettings                             m_settings;
    std::shared_ptr<IByteStream>                  m_stream;
    std::unique_ptr<AppleLinkHost>                m_host;
    std::array<std::unique_ptr<DriveSource>, 2>   m_sources;
    std::array<std::unique_ptr<LinkedDisk>, 2>    m_disks;
    std::array<std::optional<VolumeListing>, 2>   m_listings;
    std::atomic<LinkState>                        m_state    { LinkState::Closed };
    std::atomic<Byte>                             m_driveMap { 0 };
    std::mutex                                  m_lock;
    std::condition_variable_any                 m_wake;
    std::deque<std::shared_ptr<LinkOperation>>  m_queue;
    std::shared_ptr<LinkOperation>              m_running;
    uint64_t                                    m_nextId           = 1;
    DWORD                                       m_connectTimeoutMs = kConnectTimeoutMs;
    uint64_t                                    m_commitTag        = 1;
    std::jthread                                m_thread;
};
