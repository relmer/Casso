#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskCopy.h"





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


class IInspectorDiskSource
{
public:
    virtual ~IInspectorDiskSource() = default;

    virtual const DiskImage *  GetImage (int drive, InspectorDiskIdentity & outIdentity) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequest / InspectorReply
//
//  What the inspector window asks of the emulation thread, and what comes
//  back (contracts/emulation-thread.md). A request holds the mediaId of the
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
    std::wstring                                reason;
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

private:
    static InspectorReply  Answer (uint64_t id, const InspectorRequest & request, IInspectorDiskSource & source);

    mutable std::mutex                              m_lock;
    vector<std::pair<uint64_t, InspectorRequest>>   m_requests;
    vector<InspectorReply>                          m_replies;
    uint64_t                                        m_nextId = 1;
    Notify                                          m_notify;
};
