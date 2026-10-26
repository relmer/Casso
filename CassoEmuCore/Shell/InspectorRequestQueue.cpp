#include "Pch.h"

#include "Shell/InspectorRequestQueue.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/SectorFieldWriter.h"





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
//  window posting while the copies are made never waits on them. An apply
//  that waited on an earlier pass is checked again first; a new one that has
//  to wait gets the reply Waiting, and its result comes on a later pass.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorRequestQueue::Drain (IInspectorDiskSource & source)
{
    vector<std::pair<uint64_t, InspectorRequest>>  requests;
    vector<InspectorReply>                         replies;
    vector<Waiting>                                stillWaiting;
    Notify                                         notify;



    {
        std::scoped_lock  lock (m_lock);

        requests.swap (m_requests);
    }

    for (Waiting & waiting : m_waiting)
    {
        InspectorReply  reply;

        if (TryApply (waiting.id, waiting.request, source, waiting.isRunForced, reply))
        {
            replies.push_back (std::move (reply));
        }
        else
        {
            stillWaiting.push_back (std::move (waiting));
        }
    }

    m_waiting = std::move (stillWaiting);

    for (const auto & [id, request] : requests)
    {
        if (request.kind == InspectorRequestKind::Apply)
        {
            Waiting         waiting { id, request, false };
            InspectorReply  reply;

            if (!TryApply (id, request, source, waiting.isRunForced, reply))
            {
                reply.applyOutcome = ApplyOutcome::Waiting;
                m_waiting.push_back (std::move (waiting));
            }

            replies.push_back (std::move (reply));
        }
        else if (request.kind == InspectorRequestKind::CancelApply)
        {
            for (Waiting & waiting : m_waiting)
            {
                InspectorReply  reply;

                reply.requestId    = waiting.id;
                reply.kind         = InspectorRequestKind::Apply;
                reply.drive        = waiting.request.drive;
                reply.mediaId      = waiting.request.mediaId;
                reply.applyKind    = waiting.request.applyKind;
                reply.applyOutcome = ApplyOutcome::Cancelled;
                replies.push_back (std::move (reply));

                //  A machine that was run only to get to the safe point is paused again.
                if (waiting.isRunForced)
                {
                    source.SetMachinePaused (true);
                }
            }

            m_waiting.clear();
        }
        else
        {
            replies.push_back (Answer (id, request, source));
        }
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
//  InspectorRequestQueue::HasWaiting
//
//  Emulation thread only, as Drain is.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorRequestQueue::HasWaiting() const
{
    return !m_waiting.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::RestoreRecord
//
////////////////////////////////////////////////////////////////////////////////

void InspectorRequestQueue::RestoreRecord (DiskImage & image, const TrackCopy & copy)
{
    if (copy.kind == TrackKind::Flux)
    {
        image.SetFluxTrack (copy.slot, copy.fluxBytes);
    }
    else
    {
        image.GetTrackBitsForWrite (copy.slot) = copy.bits;
        image.SetTrackBitCount (copy.slot, copy.bitCount);
    }

    image.MarkTrackChangedByWriter (copy.slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::IsSafePoint
//
//  A drive that is not turning is always at a safe point. While it turns,
//  only an apply that may pause the machine has one: no write latch since
//  the last pass and the write mode off, and the head outside every data
//  field the apply changes (FR-103).
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorRequestQueue::IsSafePoint (const InspectorRequest & request, const InspectorDriveMotion & motion)
{
    size_t  drive     = static_cast<size_t> (std::clamp (request.drive, 0, static_cast<int> (m_lastWriteNibbles.size()) - 1));
    bool    isWriting = motion.isWriting || motion.writeNibbles != m_lastWriteNibbles[drive];
    bool    isSafe    = !motion.isTurning;
    double  start     = 0.0;
    double  end       = 0.0;
    double  angle     = 0.0;



    m_lastWriteNibbles[drive] = motion.writeNibbles;

    if (!isSafe && request.applyMode != ApplyMode::WaitForStop && !isWriting)
    {
        isSafe = true;

        for (const EditedFieldSpan & span : request.spans)
        {
            if (span.slot != motion.slot)
            {
                continue;
            }

            start  = span.startTurn - std::floor (span.startTurn);
            end    = start + std::max (span.endTurn - span.startTurn, 0.0);
            angle  = motion.angle - std::floor (motion.angle);
            angle += (angle < start) ? 1.0 : 0.0;
            isSafe = isSafe && !(angle >= start && angle <= end);
        }
    }

    return isSafe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueue::TryApply
//
//  At a safe point, all in one pass so the guest never runs in between:
//  each track record is copied, changed (by the writer for an apply, from the
//  history's copies for an undo or redo), and the disk saved. A failed save
//  puts the copies back. A guest write since the window's view of a record
//  ends an apply's wait, and makes an undo or redo unavailable (FR-103,
//  FR-104, FR-107).
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorRequestQueue::TryApply (uint64_t id, const InspectorRequest & request, IInspectorDiskSource & source, bool & inOutIsRunForced, InspectorReply & outReply)
{
    InspectorDiskIdentity     identity;
    const DiskImage         * current   = source.GetImage (request.drive, identity);
    DiskImage               * image     = source.GetImageForEdit (request.drive);
    InspectorDriveMotion      motion    = source.GetMotion (request.drive);
    InspectorSaveResult       save;
    vector<int>               slots;
    vector<SectorWriteError>  errors;
    HRESULT                   hr        = S_OK;
    bool                      isDone    = true;
    bool                      isChanged = false;



    outReply.requestId    = id;
    outReply.kind         = InspectorRequestKind::Apply;
    outReply.drive        = request.drive;
    outReply.mediaId      = identity.mediaId;
    outReply.applyKind    = request.applyKind;
    outReply.applyOutcome = ApplyOutcome::NotApplied;

    for (const RecordWriteCount & count : request.expectedCounts)
    {
        isChanged = isChanged || (image != nullptr && image->GetGuestWriteCount (count.slot) != count.guestWriteCount);
    }

    if (current == nullptr || image == nullptr)
    {
        outReply.status = InspectorReplyStatus::NoDisk;
        outReply.reason = L"There is no disk in the drive";
    }
    else if (request.mediaId != identity.mediaId)
    {
        outReply.status = InspectorReplyStatus::DiskChanged;
        outReply.reason = L"The disk in the drive changed";
    }
    else if (isChanged)
    {
        outReply.applyOutcome = (request.applyKind == ApplyKind::Apply) ? ApplyOutcome::WaitEnded : ApplyOutcome::NotApplied;
        outReply.reason       = L"The guest wrote to a track this edit changes";
    }
    else if (image->IsWriteProtected())
    {
        outReply.reason = L"The disk is write-protected";
    }
    else if (!IsSafePoint (request, motion))
    {
        isDone = false;

        if (request.applyMode == ApplyMode::RunToSafePointStayPaused && motion.isPaused)
        {
            source.SetMachinePaused (false);
            inOutIsRunForced = true;
        }
    }
    else
    {
        for (const SectorWrite & write : request.writes)
        {
            slots.push_back (write.slot);
        }

        for (const std::shared_ptr<const TrackCopy> & copy : request.restore)
        {
            slots.push_back (copy->slot);
        }

        std::sort (slots.begin(), slots.end());
        slots.erase (std::unique (slots.begin(), slots.end()), slots.end());

        for (int slot : slots)
        {
            outReply.before.push_back (TrackCopy::MakeFromImage (*image, slot));
        }

        if (request.applyKind == ApplyKind::Apply)
        {
            hr = SectorFieldWriter::Write (*image, request.writes, SectorWritePolicy::Editor, errors);
        }
        else
        {
            for (const std::shared_ptr<const TrackCopy> & copy : request.restore)
            {
                RestoreRecord (*image, *copy);
            }
        }

        if (FAILED (hr))
        {
            outReply.writeErrors = errors;
            outReply.reason      = errors.empty() ? std::wstring (L"Not applied: the sectors could not be written")
                                                  : L"Not applied: " + TextEncoding::Utf8ToWide (SectorFieldWriter::Describe (errors.front()));
        }
        else
        {
            save = source.SaveDrive (request.drive);

            if (FAILED (save.hr))
            {
                for (const std::shared_ptr<const TrackCopy> & copy : outReply.before)
                {
                    RestoreRecord (*image, *copy);
                }

                outReply.reason = L"Not applied: the file could not be saved. " + save.reason;
            }
            else
            {
                for (int slot : slots)
                {
                    outReply.after.push_back (TrackCopy::MakeFromImage (*image, slot));
                    outReply.countsAfter.push_back ({ slot, image->GetGuestWriteCount (slot) });
                }

                outReply.applyOutcome = save.isPreservedCopy ? ApplyOutcome::AppliedToPreservedCopy : ApplyOutcome::Applied;
                outReply.savedPath    = save.path;
            }
        }

        //  In this mode the machine is paused afterward (FR-103).
        if (request.applyMode == ApplyMode::RunToSafePointStayPaused)
        {
            source.SetMachinePaused (true);
        }
    }

    return isDone;
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
    else if (request.kind == InspectorRequestKind::CopyAsInserted)
    {
        reply.sourceBytes = std::make_shared<const vector<Byte>> (image->GetSourceBytes());
        reply.fileName    = identity.fileName;
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
