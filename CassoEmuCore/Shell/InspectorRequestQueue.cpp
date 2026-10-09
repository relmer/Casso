#include "Pch.h"

#include "Shell/InspectorRequestQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::SetNotify
//
////////////////////////////////////////////////////////////////////////////////

void InspectorRequestQueue::SetNotify (Notify notify)
{
    std::scoped_lock  lock (m_lock);



    m_notify = std::move (notify);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::Post
//
////////////////////////////////////////////////////////////////////////////////

uint64_t InspectorRequestQueue::Post (InspectorRequest request)
{
    std::scoped_lock  lock (m_lock);
    uint64_t          id   = m_nextId++;



    m_requests.emplace_back (id, std::move (request));

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::Drain
//
//  The requests are taken under the lock and answered outside it, so a
//  window posting while the copies are made never waits on them.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorRequestQueue::Drain (IInspectorDiskSource & source)
{
    vector<std::pair<uint64_t, InspectorRequest>>  requests;
    vector<InspectorReply>                         replies;
    Notify                                         notify;



    {
        std::scoped_lock  lock (m_lock);

        requests.swap (m_requests);
    }

    for (const auto & [id, request] : requests)
    {
        replies.push_back (Answer (id, request, source));
    }

    if (!replies.empty())
    {
        std::scoped_lock  lock (m_lock);

        m_replies.insert (m_replies.end(), std::make_move_iterator (replies.begin()), std::make_move_iterator (replies.end()));
        notify = m_notify;
    }

    if (notify)
    {
        notify();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::TakeReplies
//
////////////////////////////////////////////////////////////////////////////////

void InspectorRequestQueue::TakeReplies (vector<InspectorReply> & outReplies)
{
    std::scoped_lock  lock (m_lock);



    outReplies.clear();
    outReplies.swap (m_replies);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::HasRequests
//
//  Lets the emulation thread skip Drain on the passes where nothing waits.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorRequestQueue::HasRequests() const
{
    std::scoped_lock  lock (m_lock);



    return !m_requests.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::Answer
//
////////////////////////////////////////////////////////////////////////////////

InspectorReply InspectorRequestQueue::Answer (uint64_t id, const InspectorRequest & request, IInspectorDiskSource & source)
{
    InspectorReply         reply;
    InspectorDiskIdentity  identity;
    const DiskImage *      image    = source.GetImage (request.drive, identity);
    int                    slot     = -1;



    reply.requestId = id;
    reply.kind      = request.kind;
    reply.drive     = request.drive;
    reply.mediaId   = identity.mediaId;

    if (image == nullptr)
    {
        reply.status = InspectorReplyStatus::NoDisk;
    }
    else if (request.mediaId != 0 && request.mediaId != identity.mediaId)
    {
        reply.status = InspectorReplyStatus::DiskChanged;
    }
    else if (request.kind == InspectorRequestKind::CopyDisk)
    {
        reply.disk = DiskCopy::MakeFromImage (*image, identity.mediaId, identity.fileName, identity.fileSize, identity.isReadOnly);
    }
    else if (request.kind == InspectorRequestKind::CopyTracks)
    {
        for (int s : request.slots)
        {
            reply.tracks.push_back (TrackCopy::MakeFromImage (*image, s));
        }
    }
    else if (request.quarterTrack >= 0 && request.quarterTrack < DiskImage::kQuarterTrackCount)
    {
        slot = image->ResolveQuarterTrack (request.quarterTrack);

        if (slot >= 0)
        {
            reply.tracks.push_back (TrackCopy::MakeFromImage (*image, slot));
        }
    }

    return reply;
}
