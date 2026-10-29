#pragma once

#include "Pch.h"

#include "CassoLink/Session/CassoLinkConnector.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/CassoLinkHost.h"
#include "Seams/ByteStreamSettings.h"
#include "CassoLink/CassoLinkSettings.h"

class IByteStream;
class IByteStreamFactory;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession
//
//  One open link: the stream, the host engine, a LinkedDisk per drive and a
//  queue of operations, run one at a time on the session's thread.
//  Every request returns at once with its queued operation.
//
//  The stream is opened when the session starts. With no Apple on it, the
//  line stays open and is served until the next attempt, kRetryMs later or
//  at once when a request or the Apple's Ready arrives, so ADTPro's client
//  is served between attempts; a line that drops is opened again at the
//  next attempt. Opening waits for the Apple's Ready at the bootstrap speed,
//  loads the program when its version differs, and switches to the link's
//  speed. While nothing is queued, Idle is sent in reply to each Ready.
//
//  A write first reads the catalog again, and fails when it differs from
//  the last listing of the drive, so a disk changed since it was listed is
//  reported rather than written over.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkSession : public CassoLinkSessionTypes
{
public:
    static constexpr DWORD  kIdlePollMs       = 100;

    //  The program sends Ready every 100 ms while idle; this long with no
    //  byte from it is an Apple that was reset, turned off or unplugged.
    static constexpr DWORD  kSilenceMs        = 3000;

    //  The folder is the one ADTPro's own clients are served from, between
    //  Casso's operations.
    CassoLinkSession (IByteStreamFactory & factory, ICassoLinkFolder & folder, IDiskFileIo & fileIo, ICassoLinkSessionSink & sink);
    ~CassoLinkSession ();

    HRESULT    Start  (const CassoLinkSettings & settings);

    //  How long opening waits for the Apple's Ready.
    void       SetConnectTimeout (DWORD timeoutMs) { m_connector.m_connectTimeoutMs = timeoutMs; }

    //  Starts the session on an Apple with no link program: the loader is
    //  typed in first, and the returned operation finishes when the
    //  program is running or the Apple does not answer.
    HRESULT    StartWithBootstrap (const CassoLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation) { return m_connector.StartWithBootstrap (settings, outOperation); }

    //  How long a disconnected session waits before connecting again.
    void       SetRetryInterval (DWORD retryMs) { m_connector.m_retryMs = retryMs; }

    //  The pauses after each line of the loader and each of its characters.
    void  SetBootstrapPause (DWORD pauseMs, DWORD charPauseUs = LinkBootstrap::kCharPauseUs) { m_connector.m_bootstrapPauseMs = pauseMs; m_connector.m_charPauseUs = charPauseUs; }
    void  Stop              ();

    //  Stops the running operation; queued ones remain.
    void       Cancel ();

    LinkState  GetState    () const { return m_connector.m_state; }

    //  Whether ADTPro's own client is on the line instead of Casso's. The
    //  session goes on serving it, and opening goes on waiting for Casso's
    //  program.
    bool  IsServingAdtPro () const { return m_connector.m_adtPro; }
    Byte  GetDriveMap     () const { return m_connector.m_driveMap; }

    //  The Apple's model, as its program reported it on connecting; empty
    //  before then.
    std::wstring  GetModelName () const { std::lock_guard<std::mutex>  lock (m_connector.m_lock); return m_connector.m_modelName; }

    //  Why the line last failed to open, such as a TCP port another program
    //  listens on; empty once it opens.
    std::string   GetOpenFailure () const { std::lock_guard<std::mutex>  lock (m_connector.m_lock); return m_connector.m_openFailure; }

    std::shared_ptr<LinkOperation>  Enumerate     (int drive, const FilePath & directory);
    std::shared_ptr<LinkOperation>  ReadFile      (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  WriteFile     (int drive, const FilePath & path, const FilePayload & payload);
    std::shared_ptr<LinkOperation>  Delete        (int drive, const FilePath & path);
    std::shared_ptr<LinkOperation>  Rename        (int drive, const FilePath & path, const std::string & newName);
    std::shared_ptr<LinkOperation>  SetAttributes (int drive, const FilePath & path, const FileAttributes & attributes);
    std::shared_ptr<LinkOperation>  MakeDirectory (int drive, const FilePath & path);

    //  A ProDOS directory and everything in it.
    std::shared_ptr<LinkOperation>  RemoveDirectory (int drive, const FilePath & path);

    //  Every track's standard sectors into an image file, in the format of
    //  its extension, replaced atomically and only when the read succeeds.
    std::shared_ptr<LinkOperation>  ReadDisk      (int drive, const std::string & imagePath, bool blankUnreadable);

    //  An image file onto the disk: every track formatted, written and read
    //  back. The caller has the user's confirmation first. An image whose
    //  tracks do not all decode to standard sectors fails before any track
    //  is written.
    std::shared_ptr<LinkOperation>  WriteDisk     (int drive, const std::string & imagePath, const std::string & formatted = std::string());

    //  Whether the drive has the link's boot disk, by its boot sector: a write
    //  over it, the Apple has no disk to start the link from.
    std::shared_ptr<LinkOperation>  CheckLinkDisk (int drive);

    //  The chip the Apple side talks through on a transport: the W5100 for
    //  an Uthernet II, a 6551 for the rest.
    static LinkChip  GetLinkChip (LinkTransport transport);

    //  Physical sector to DOS-logical and back, within a track.
    static int  GetLogicalSector  (int physical);
    static int  GetPhysicalSector (int logical);

    //  A Status error code as an HRESULT.
    static HRESULT  FromStatusError (Byte error);

    //  The Label text the program draws for an operation, or with
    //  `finished`, the past tense for the history line of one that succeeded.
    static std::string  DescribeOperation (const LinkOperation & operation, const std::optional<VolumeListing> & listing = std::nullopt, bool finished = false);

    //  A drive as the label has it: D2, and the disk's volume, as
    //  D2 (/GAMES) or D2 (VOL 254), once its catalog is read; the label has 38 characters.
    static std::string  DescribeDrive     (int drive, const std::optional<VolumeListing> & listing);

    //  Why the line failed, for the activity log and the status bar: the
    //  connection closed, no reply, or the system's text for any other error.
    static std::string  DescribeLineFailure (HRESULT hr);

private:

    std::shared_ptr<LinkOperation>  Queue (LinkOperationKind kind, int drive, const FilePath & path, std::shared_ptr<LinkOperation> operation = nullptr);

    void     Run           (std::stop_token stop);

    void     Disconnect    ();
    void     SetState      (LinkState state);

    HRESULT  Execute       (LinkOperation & operation);
    HRESULT  Revalidate    (int drive);
    HRESULT  CheckCatalog  (int drive);
    HRESULT  RunWrite      (LinkOperation & operation, const LinkedDisk::WriteOperation & write);
    void     Report        (LinkOperation & operation, int progress);

    static bool  IsSameCatalog (const VolumeListing & a, const VolumeListing & b);
    static bool  IsLineLost    (HRESULT hr);

    IByteStreamFactory                           & m_factory;
    ICassoLinkFolder                             & m_folder;
    IDiskFileIo                                  & m_fileIo;
    ICassoLinkSessionSink                        & m_sink;
    CassoLinkSettings                              m_settings;
    std::unique_ptr<CassoLinkHost>                 m_host;
    std::array<std::unique_ptr<DriveSource>, 2>    m_sources;
    std::array<std::unique_ptr<LinkedDisk>, 2>     m_disks;
    std::array<std::optional<VolumeListing>, 2>    m_listings;
    std::atomic<bool>                              m_stopping { false };   // Stop closed the line
    std::shared_ptr<LinkOperation>                 m_running;

    //  The session's parts, each with the state it uses.
    friend class CassoLinkConnector;
    friend class CassoLinkDiskTransfer;
    friend class CassoLinkSessionTypes::DriveSource;
    CassoLinkConnector     m_connector    { *this };
    CassoLinkDiskTransfer  m_diskTransfer { *this };
};

