#pragma once

#include "Pch.h"

#include "CassoLink/LinkedDisk.h"

class CassoLinkSession;





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
    SetAttributes,
    MakeDirectory,
    RemoveDirectory,
    ReadDisk,
    WriteDisk,
    CheckLinkDisk,
    Bootstrap,
    SaveLink,
    FullSend,
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
    FileAttributes                   attributes;
    FilePayload                      payload;

    std::atomic<int>                 progress { 0 };
    std::atomic<LinkOperationState>  state    { LinkOperationState::Queued };
    HRESULT                          result   = S_OK;
    std::string                      message;
    SYSTEMTIME                       started  = {};   // local time, once it is running

    VolumeListing                    listing;
    VolumeKind                       volumeKind = VolumeKind::Unknown;
    FilePayload                      file;

    //  Full send: imagePath is the folder and newName the chosen extension.
    //  Read disk and Write disk: the image file, and for a read, whether an
    //  unreadable sector is written as zeros rather than failing the read.
    //  unreadable lists those sectors, as track and DOS 3.3 sector.
    std::string                      imagePath;
    bool                             blankUnreadable = false;

    //  Write disk for a format: the new volume, as VOL 254 or /NAME, so the
    //  label for a format differs from one for an image written.
    std::string                      formatted;
    std::vector<std::pair<int, int>> unreadable;

    //  Check link disk: whether the drive has the link's boot disk.
    bool                             linkDisk = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICassoLinkSessionSink
//
//  Called on the session's thread. The window's implementation posts each
//  call to its own thread, so the line is never waited on there.
//
////////////////////////////////////////////////////////////////////////////////

class ICassoLinkSessionSink
{
public:
    virtual ~ICassoLinkSessionSink () = default;

    virtual void  OnStateChanged (LinkState state)                                    = 0;
    virtual void  OnProgress     (const std::shared_ptr<LinkOperation> & operation)   = 0;
    virtual void  OnDone         (const std::shared_ptr<LinkOperation> & operation)   = 0;

    //  A line for the activity log: a connection made or dropped, an
    //  operation finished, or a request of ADTPro's served.
    virtual void  OnActivity     (const std::string & text)                            { UNREFERENCED_PARAMETER (text); }

    //  An operation finished, for the log with the time it started.
    virtual void  OnFinished     (const LinkOperation & operation, const std::string & text) { UNREFERENCED_PARAMETER (operation); OnActivity (text); }

    //  The folder and extension of a full send started from the Apple, with
    //  the folder ready to use; false when there is no folder to send to.
    virtual bool  TryGetFullSendTarget (std::string & outFolder, std::string & outExtension) { UNREFERENCED_PARAMETER (outFolder); UNREFERENCED_PARAMETER (outExtension); return false; }

    //  An operation the Apple started, so the window can track and cancel it.
    virtual void  OnOperationQueued (const std::shared_ptr<LinkOperation> & operation) { UNREFERENCED_PARAMETER (operation); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSessionTypes
//
//  The types CassoLinkSession and its parts share. CassoLinkSession derives from it, so
//  each is also CassoLinkSession::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkSessionTypes
{
    static constexpr DWORD  kConnectTimeoutMs = 20000;

    static constexpr DWORD  kRetryMs          = 5000;

    static constexpr int    kDriveCount       = 2;

    //  A drive's sectors over the link, for its LinkedDisk.
    class DriveSource : public ILinkedSectorSource
    {
    public:
        DriveSource (CassoLinkSession & session, int drive) : m_session (session), m_drive (drive) {}

        HRESULT  ReadSectors  (std::span<const size_t> sectors, std::vector<Byte> & outData) override;
        HRESULT  WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)  override;

    private:
        CassoLinkSession  & m_session;
        int                 m_drive;
    };
};
