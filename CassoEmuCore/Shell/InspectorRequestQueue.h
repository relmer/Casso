#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/SectorWrite.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IInspectorDiskSource
//
//  The drives as the emulation thread sees them, for InspectorRequestQueue.
//  GetImage gives the image in a drive with its identity, or nullptr when
//  the drive is empty or does not exist. Called only on the emulation thread.
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorDiskIdentity
{
    uint64_t     mediaId    = 0;
    std::string  fileName;
    uint64_t     fileSize   = 0;
    bool         isReadOnly = false;
};


//  A drive's motion on the emulation thread, for the safe point
//  of an apply (FR-103).
struct InspectorDriveMotion
{
    bool      isTurning    = false;
    bool      isWriting    = false;
    int       slot         = -1;
    double    angle        = 0.0;
    uint64_t  writeNibbles = 0;
    bool      isPaused     = false;
};


//  How a save of a drive's disk went (FR-104): to its file, to a preserved
//  copy when the file changed outside Casso, or not at all.
struct InspectorSaveResult
{
    HRESULT       hr              = S_OK;
    std::string   path;
    bool          isPreservedCopy = false;
    std::wstring  reason;
};


class IInspectorDiskSource
{
public:
    virtual ~IInspectorDiskSource() = default;

    virtual const DiskImage *  GetImage (int drive, InspectorDiskIdentity & outIdentity) = 0;

    //  For applying edits; a source that cannot change its disks returns
    //  nullptr and the apply is not made.
    virtual DiskImage *           GetImageForEdit  (int drive)        { (void) drive; return nullptr; }
    virtual InspectorDriveMotion  GetMotion        (int drive)        { (void) drive; return {}; }
    virtual InspectorSaveResult   SaveDrive        (int drive)        { (void) drive; return { E_NOTIMPL, {}, false, L"This program cannot save the disk" }; }
    virtual void                  SetMachinePaused (bool isPaused)    { (void) isPaused; }
    virtual vector<std::string>   GetMountedPaths  ()                 { return {}; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequest / InspectorReply
//
//  What the inspector window asks of the emulation thread, and what comes
//  back (contracts/emulation-thread.md). A request has the mediaId of the
//  disk it is about; a different disk in the drive gets DiskChanged and
//  nothing is done. A mediaId of 0 means whatever disk is in the drive,
//  which is how the window asks for its first copy.
//
////////////////////////////////////////////////////////////////////////////////

enum class InspectorRequestKind
{
    CopyDisk,
    CopyTracks,
    Export,

    //  The file's bytes as the disk went in, for a comparison (FR-117).
    CopyAsInserted,

    //  Apply, undo or redo an edit at a safe point (FR-100, FR-103, FR-107),
    //  and end a wait for one.
    Apply,
    CancelApply,

    //  The disk with the pending edits applied, serialized for "Save edited
    //  copy..." without changing the drive's disk (FR-101).
    SaveCopy,
};


enum class ApplyKind
{
    Apply,
    Undo,
    Redo,
};


//  When an apply is made while the drive turns (FR-103): once it
//  stops, at the next safe point with the machine running on, or at the
//  next safe point with the machine paused there.
enum class ApplyMode
{
    WaitForStop,
    PauseAtSafePoint,
    RunToSafePointStayPaused,
};


enum class ApplyOutcome
{
    None,
    Waiting,
    Applied,
    AppliedToPreservedCopy,
    NotApplied,
    Cancelled,
    WaitEnded,
};


//  Where a changed data field lies on its record, as turns from the index,
//  so an apply waits while the head is over it.
struct EditedFieldSpan
{
    int     slot      = -1;
    double  startTurn = 0.0;
    double  endTurn   = 0.0;
};


struct RecordWriteCount
{
    int       slot            = -1;
    uint64_t  guestWriteCount = 0;
};


enum class InspectorReplyStatus
{
    Done,
    DiskChanged,
    NoDisk,

    //  The file could not be opened at all; the reply says why (FR-054).
    Unopenable,
};


struct InspectorRequest
{
    InspectorRequestKind  kind         = InspectorRequestKind::CopyDisk;
    int                   drive        = 0;
    uint64_t              mediaId      = 0;
    vector<int>           slots;
    int                   quarterTrack = -1;

    ApplyKind                                 applyKind = ApplyKind::Apply;
    ApplyMode                                 applyMode = ApplyMode::WaitForStop;
    vector<SectorWrite>                       writes;
    vector<std::shared_ptr<const TrackCopy>>  restore;
    vector<RecordWriteCount>                  expectedCounts;
    vector<EditedFieldSpan>                   spans;
};


struct InspectorReply
{
    uint64_t                                    requestId = 0;
    InspectorRequestKind                        kind      = InspectorRequestKind::CopyDisk;
    int                                         drive     = 0;
    InspectorReplyStatus                        status    = InspectorReplyStatus::Done;
    uint64_t                                    mediaId   = 0;
    std::shared_ptr<const DiskCopy>             disk;
    vector<std::shared_ptr<const TrackCopy>>    tracks;
    std::shared_ptr<const vector<Byte>>         sourceBytes;
    std::string                                 fileName;
    std::wstring                                reason;

    ApplyKind                                 applyKind       = ApplyKind::Apply;
    ApplyOutcome                              applyOutcome    = ApplyOutcome::None;
    vector<std::shared_ptr<const TrackCopy>>  before;
    vector<std::shared_ptr<const TrackCopy>>  after;
    vector<RecordWriteCount>                  countsAfter;
    std::string                               savedPath;
    vector<SectorWriteError>                  writeErrors;
    bool                                      isMachinePaused = false;
    vector<std::string>                       mountedPaths;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue
//
//  Post works from any thread and gives the request an id. Drain runs on the
//  emulation thread at the end of a service pass and answers every request
//  posted so far; notify is then called once, on that thread, to wake the
//  window. TakeReplies works from any thread.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorRequestQueue
{
public:
    using Notify = std::function<void ()>;

    void      SetNotify   (Notify notify);
    uint64_t  Post        (InspectorRequest request);
    void      Drain       (IInspectorDiskSource & source);
    void      TakeReplies (vector<InspectorReply> & outReplies);
    bool      HasRequests () const;

    //  An apply waits here, across passes, for its safe point.
    bool      HasWaiting  () const;

    //  Puts a record back exactly as a copy has it, as the writer changes
    //  one, so a save writes it (FR-104, FR-107).
    static void  RestoreRecord (DiskImage & image, const TrackCopy & copy);

private:
    struct Waiting
    {
        uint64_t          id           = 0;
        InspectorRequest  request;
        bool              isRunForced  = false;
    };

    static InspectorReply  Answer (uint64_t id, const InspectorRequest & request, IInspectorDiskSource & source);
    static void            AnswerSaveCopy (const InspectorRequest & request, IInspectorDiskSource & source, InspectorReply & inOutReply);

    //  Makes an apply at a safe point; false while it waits for one.
    bool  TryApply (uint64_t id, const InspectorRequest & request, IInspectorDiskSource & source, bool & inOutIsRunForced, InspectorReply & outReply);
    bool  IsSafePoint (const InspectorRequest & request, const InspectorDriveMotion & motion);

    mutable std::mutex                             m_lock;
    vector<std::pair<uint64_t, InspectorRequest>>  m_requests;
    vector<InspectorReply>                         m_replies;
    uint64_t                                       m_nextId           = 1;
    Notify                                         m_notify;
    vector<Waiting>                                m_waiting;
    std::array<uint64_t, 2>                        m_lastWriteNibbles = {};
};
