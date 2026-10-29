#include "Pch.h"

#include "DiskStoreReload.h"
#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::NoteExternalChange
//
//  Somebody says this image changed.
//
//  IT RECORDS AND RETURNS. The watcher calls this from its own thread and the
//  message channel from the UI thread, and only the thread that has the
//  store's token may read an image or swap it under a running guest. So the
//  change goes into an inbox, and ApplyPendingReload matches it to the bays.
//
//  A PATH NO BAY HOLDS IS NOT AN ERROR. A directory watch reports every file
//  under it, and an intent may be stated for an image nothing has mounted; both
//  are ordinary and both end here, silently.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::NoteExternalChange (const string & path, ExternalChangeIntent intent)
{
    int64_t  now = m_store.GetNowMs();



    {
        std::lock_guard<std::mutex>  lock (m_pendingMutex);



        m_changeInbox[path] = NotedChange { now, intent };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ApplyPendingReload
//
//  Act on whatever has settled, on the thread that owns disk writes.
//
//  CALLED OFTEN AND DOES NOTHING ALMOST EVERY TIME. It is pumped from the
//  controller's idle callback and from motor spindown, so the common path is a
//  loop over sixteen bays that finds nothing pending and returns.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ApplyPendingReload()
{
    int      readySlot[DiskImageStore::kSlotCount * DiskImageStore::kDriveCount]  = {};
    int      readyDrive[DiskImageStore::kSlotCount * DiskImageStore::kDriveCount] = {};
    int      readyCount                           = 0;
    int      slot                                 = 0;
    int      drive                                = 0;
    int      i                                    = 0;
    int64_t  now                                  = m_store.GetNowMs();



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    //  A replay must not take up a changed file: the disk it swaps in is not
    //  the one history recorded. The change stays pending for the live run.
    if (m_store.m_isReplaying)
    {
        return;
    }

    DrainChangeInbox();

    //  ONE CLOCK READING FOR THE WHOLE WALK, not one per bay. This runs sixty
    //  times a second forever, and the walk is a handful of comparisons; the
    //  clock read per bay cost far more than the work itself.
    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            const Entry &  entry = m_store.m_entries[slot][drive];

            if (entry.mounted
             && entry.image != nullptr
             && entry.sharedState.IsSettled (now))
            {
                readySlot[readyCount]  = slot;
                readyDrive[readyCount] = drive;
                readyCount++;
            }
        }
    }

    for (i = 0; i < readyCount; i++)
    {
        ApplyPendingReloadToBay (readySlot[i], readyDrive[i]);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::DrainChangeInbox
//
//  Moves the changes noted since the last drain into the bays that have each
//  path, on the thread that has the store's token. Only mounted bays match; a
//  retained disk is not in a bay, so no bay's record changes for it. The lock is
//  taken only to swap the inbox, so a watcher thread never waits on a bay.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::DrainChangeInbox()
{
    int  slot  = 0;
    int  drive = 0;



    {
        std::lock_guard<std::mutex>  lock (m_pendingMutex);



        m_drainedChanges.swap (m_changeInbox);
    }

    for (const auto & [path, change] : m_drainedChanges)
    {
        for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
        {
            for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
            {
                Entry &  entry = m_store.m_entries[slot][drive];

                if (entry.mounted && MountedImageState::IsSamePath (entry.path, path))
                {
                    entry.sharedState.NoteChange (change.atMs, change.intent);
                }
            }
        }
    }

    m_drainedChanges.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ApplyPendingReloadToBay
//
//  One bay's settled change, from noticing it to acting on it.
//
//  THE READ HAPPENS BEFORE THE DECISION, and that ordering is what lets the
//  policy rank "cannot be used" above everything else: whether the new bytes
//  can be this disk is not knowable without reading them.
//
//  The file work -- whether another program has the file open, its identity,
//  the read and whether the file exists -- is one job on the I/O queue, and
//  the decision is made in its completion, by ApplyChangedFile. One job per
//  bay is in flight at a time: this runs every frame while a change is
//  settled.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ApplyPendingReloadToBay (int slot, int drive)
{
    Entry                    & entry  = m_store.GetEntry (slot, drive);
    ExternalChangeIntent       intent = ExternalChangeIntent::Unstated;
    shared_ptr<ReloadJob>      job;
    DiskFileOps                ops    = m_store.GetFileOps();



    if (m_isJobPosted[slot][drive] || !IsReloadDue (entry, intent))
    {
        return;
    }

    job           = std::make_shared<ReloadJob>();
    job->slot     = slot;
    job->drive    = drive;
    job->imageId  = entry.image->GetImageId();
    job->path     = entry.path;
    job->intent   = intent;
    job->recorded = entry.sharedState.GetIdentity();
    job->fileIo   = m_store.m_fileIo;
    job->stamp    = PreservedCopy::MakeStamp (m_store.m_timestamp ? m_store.m_timestamp() : time (nullptr));

    m_isJobPosted[slot][drive] = true;

    m_store.m_ioQueue->Post ([job, ops] { RunReloadWork (*job, ops); },
                             [this, job]
    {
        m_isJobPosted[job->slot][job->drive] = false;
        ApplyChangedFile (*job);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::RunReloadWork
//
//  I/O thread. A file another program has open is left for a later pass;
//  otherwise the file is stat'd, and read when it differs from the bay's
//  identity.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::RunReloadWork (ReloadJob & job, const DiskFileOps & ops)
{
    job.isHeldElsewhere = job.fileIo != nullptr && job.fileIo->IsHeldByAnotherProcess (job.path);

    if (!job.isHeldElsewhere)
    {
        job.isChanged = !job.recorded.Matches (ops.ReadIdentity (job.path));
    }

    if (job.isChanged)
    {
        job.hrRead   = ops.Read (job.path, job.bytes);
        job.current  = ops.ReadIdentity (job.path);
        job.isExists = ops.Exists (job.path);
        DiskFileOps::ProbeWritability (job.path, job.isReadOnly, job.isDenied);

        //  The name a question would reserve for the guest's version, found
        //  here because finding it probes the folder.
        (void) ops.FindFreePreservedPath (job.path, job.stamp, job.freeName);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ApplyChangedFile
//
//  The decision, after the file work. A CHANGE THAT TURNS OUT NOT TO BE ONE IS
//  DROPPED: a directory watch reports a write to any file in the folder and a
//  stated intent is only a hint, so the identity is what settles whether
//  anything happened. This is also what stops the store's commit from coming
//  back as another program's change.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ApplyChangedFile (ReloadJob & job)
{
    Entry                            & entry     = m_store.GetEntry (job.slot, job.drive);
    int                                slot      = job.slot;
    int                                drive     = job.drive;
    bool                               usable    = false;
    bool                               preserved = false;
    ExternalChangeIntent               intent    = job.intent;
    ChangeAuthor                       author    = ChangeAuthor::AnotherProgram;
    ChangeAction                       action    = ChangeAction::Ignore;
    vector<Byte>                     & bytes     = job.bytes;
    ExternalChangePolicy::Situation    situation;
    string                             savePath;



    if (!entry.mounted || entry.image == nullptr || entry.image->GetImageId() != job.imageId || job.isHeldElsewhere)
    {
        return;
    }

    if (!job.isChanged)
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
        return;
    }

    usable = SUCCEEDED (job.hrRead) && IsLoadable (entry.path, bytes);

    //  An open write on a flux track is a guest write that is not in the image
    //  yet. It is committed here, as FlushEntry does, so it counts toward the
    //  conflict and goes into the preserved copy rather than reading as clean
    //  and being spliced into the reloaded disk.
    entry.image->CommitPendingWrite();

    situation.changeSeen = true;
    situation.usable     = usable;
    situation.intent     = intent;

    //  The guest's unsaved writes are what turns a pick-up into a conflict.
    //  They were deferred outright while there was nowhere to preserve them;
    //  now there is, so the flag goes to the policy and the conflict is
    //  resolved rather than postponed.
    situation.guestDirty  = entry.image->IsDirty();

    action = ExternalChangePolicy::Decide (situation);

    //  A CONFLICT IS RESOLVED INTO AN ORDINARY CHANGE, HERE, BEFORE ANYTHING
    //  ELSE IS DECIDED. The guest's version goes to a file of its own straight
    //  away, which is the whole guarantee: once it is on disk nothing that
    //  follows can lose it, and what follows is then the same decision any
    //  external change gets.
    //
    //  THE FILE ALWAYS STAYS WITH WHOEVER CHANGED IT, and the guest's version
    //  is always the one that moves. The flush path applies the same rule from
    //  the other end, so the outcome no longer depends on which of the two
    //  found the collision first -- it used to, and the winner was whichever
    //  side happened to be quicker.
    if (action == ChangeAction::Conflict)
    {
        if (!PreserveGuestCopyForConflict (slot, drive, entry, savePath))
        {
            return;
        }

        situation.guestDirty = false;
        action               = ExternalChangePolicy::Decide (situation);
        preserved            = true;
    }

    //  A file that is simply gone gets its own sentence rather than sharing
    //  one with a file that is present and unreadable.
    if (action == ChangeAction::Unusable && !job.isExists)
    {
        action = ChangeAction::Deleted;
    }

    if (ExternalChangePolicy::NeedsAnAnswer (action))
    {
        AskAboutChange (slot, drive, entry, action, job.freeName);
        return;
    }

    if (m_store.m_decisionSink)
    {
        m_store.m_decisionSink (entry.path, action, preserved, savePath);
    }

    //  THE INTENT THAT DECIDED IS THE INTENT THAT ATTRIBUTES. `intent` was read
    //  when the change was found due, and the record it came from is refreshed
    //  by every later write -- a read taken now would credit this reload to
    //  whoever wrote most recently rather than to whoever asked for it.
    author = (intent == ExternalChangeIntent::Unstated) ? ChangeAuthor::AnotherProgram
                                                        : ChangeAuthor::CassoCli;

    CarryOutChangeAction (slot, drive, action, bytes, author, job.current);

    //  The file's writability, probed in the job, on the disk the bay has
    //  after the change.
    if (m_store.GetEntry (slot, drive).mounted)
    {
        m_store.SetFileWriteProtect (slot, drive, job.isReadOnly, job.isDenied);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::IsReloadDue
//
//  Whether the bay's noticed change is settled and real, with the intent
//  stated for it.
//
//  WHILE A QUESTION STANDS THE USER OWNS THE BAY, and every later change
//  waits behind their answer -- a stated intent included. Applying one
//  underneath an open dialog swapped the disk out from under it, so
//  "keep your current version" came to mean the other program's version.
//
//  NOTHING IS LOST BY WAITING. `NoteChange` keeps refreshing the pending
//  record, and the answer re-reads the file when it arrives, so what lands is
//  the newest bytes rather than whatever stood when the question went up. One
//  consequence: a restart requested while a question is on screen reloads
//  without restarting, because the answer consumes the record that carried
//  the request.
//
//  IT ALSO STOPS THE READING. The reload reads the whole image and trial-loads
//  it before it reaches the point where an outstanding question is noticed,
//  and the change stays pending by design for as long as the user reads --
//  sixty full reads and nibblizations a second until they answer.
//
//  A file another program has open is deferred rather than declined,
//  indefinitely and silently: the pick-up happens once it is closed. Both
//  writers in this system commit atomically, but a text editor or a copy tool
//  need not, and the quiet period alone does not cover one that takes its
//  time. RunReloadWork checks for one, on the I/O thread.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreReload::IsReloadDue (Entry & entry, ExternalChangeIntent & outIntent)
{
    bool  settled = false;
    bool  isDue   = false;



    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        settled   = entry.mounted
                 && entry.image != nullptr
                 && entry.sharedState.IsSettled (m_store.GetNowMs());
        outIntent = entry.sharedState.GetPending().intent;
    }

    isDue = settled && !entry.sharedState.IsAskOutstanding();

    return isDue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::IsLoadable
//
//  Whether the file's bytes can be this disk. A trial load into a throwaway
//  image is the only honest answer -- plenty of files that will not load
//  have the right extension and length.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreReload::IsLoadable (const string & path, const vector<Byte> & bytes)
{
    HRESULT     hr     = S_OK;
    DiskFormat  fmt    = DiskFormat::Dsk;
    bool        usable = false;



    hr = DiskImageStore::GetSourceFormatByExtension (path, fmt);

    if (SUCCEEDED (hr))
    {
        DiskImage  trial;

        trial.LoadFromBytes (fmt, bytes, path);
        usable = trial.IsLoaded();
    }

    return usable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::PreserveGuestCopyForConflict
//
//  Writes the guest's version to a separate file, so the change that
//  follows is no longer a conflict. False when the copy could not be written,
//  after the failure has been dealt with.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreReload::PreserveGuestCopyForConflict (int slot, int drive, Entry & entry, string & outSavePath)
{
    HRESULT  hr = S_OK;



    outSavePath = entry.sharedState.GetPreservedPath();

    hr = m_store.m_save.SaveLoadedImage (entry, outSavePath);

    entry.sharedState.SetPreservedPath (outSavePath);

    if (FAILED (hr))
    {
        //  Nothing is mounted and nothing is overwritten. The pending change
        //  is DROPPED RATHER THAN RETRIED: retrying costs a full image read
        //  and a failed write on every idle tick, sixty times a second for as
        //  long as the folder stays full. The next change to the file, or the
        //  next flush, tries again.
        {
            std::lock_guard<std::mutex>  guard (m_pendingMutex);

            entry.sharedState.ClearPending();
        }

        if (m_store.m_decisionSink)
        {
            m_store.m_decisionSink (entry.path, ChangeAction::Conflict, false, outSavePath);
        }

        //  ASKED RATHER THAN REPORTED, because "Save as..." is an answer and a
        //  notice has nowhere to put one. The bay is left with the question
        //  outstanding so a second failure does not stack a second dialog on
        //  the first.
        if (m_store.m_askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            entry.sharedState.SetAskOutstanding (
                m_store.m_askSink (slot, drive,
                           ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                             outSavePath, hr,
                                                             SaveFailureCause::ExternalChange)));
        }

        return false;
    }

    //  It is on disk under its own name now, so the bay carries nothing
    //  unsaved and the change that follows is no longer a conflict.
    entry.sharedState.SetPreservedWritten (true);

    entry.image->ClearDirty();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::AskAboutChange
//
//  Puts the question about a change to the user.
//
//  A question nothing can answer stays pending rather than resolving itself
//  by default. A headless host has no way to ask, and picking one of the
//  answers on the user's behalf is what the fallback exists for.
//
//  ONE QUESTION AT A TIME PER BAY. Asking cannot block -- the answer comes
//  from another thread -- so the change is still pending on the next idle
//  tick, and without this the user would be asked again sixty times a second
//  while reading the first one.
//
//  THE PRESERVED NAME IS RESERVED, NOT JUST CALCULATED. The name shown here is
//  the one the copy will take whoever writes it -- this bay's own flush may
//  get there first, while this question is still on screen. Working it out
//  again at that point produced a different name and left the dialog offering
//  a file nobody ever created.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::AskAboutChange (int slot, int drive, Entry & entry, ChangeAction action, const string & freeName)
{
    string  reserved;



    if (!m_store.m_askSink || entry.sharedState.IsAskOutstanding())
    {
        return;
    }

    entry.sharedState.SetAskedAction (action);

    reserved = entry.sharedState.GetPreservedPath();

    if (reserved.empty())
    {
        reserved = freeName;
        entry.sharedState.SetPreservedPath (reserved);
    }

    entry.sharedState.SetAskOutstanding (
        m_store.m_askSink (slot, drive,
                   ChangePrompt::Compose (entry.path, drive, action,
                                          reserved,
                                          entry.sharedState.IsPreservedWritten())));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ResolvePendingChange
//
//  The answer to a question this store asked.
//
//  THE BYTES ARE READ AGAIN HERE RATHER THAN KEPT FROM THE QUESTION. The user
//  may have run two more builds while the prompt sat on the screen, and the
//  version they mean is the one they just built -- not the one that was on disk
//  when the question was composed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ResolvePendingChange (int slot, int drive, ChangeAction chosen,
                                          const string & savePath)
{
    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    if (!DiskImageStore::IsValidBay (slot, drive))
    {
        return;
    }

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        //  AN ANSWER TO A QUESTION NOBODY IS ASKING ANY MORE IS DROPPED. Mount
        //  and eject both clear this flag, so a disk that left the drive while
        //  the dialog stood takes its question with it. Acting anyway carried
        //  the answer onto whatever disk went in next: "keep the one I have"
        //  moved that bay onto the departed disk's rescue copy, which the next
        //  flush then wrote over.
        if (!entry.sharedState.IsAskOutstanding())
        {
            return;
        }

        entry.sharedState.SetAskOutstanding (false);

        if (!entry.mounted || entry.image == nullptr)
        {
            return;
        }

        if (entry.sharedState.GetAskedAction() == ChangeAction::Conflict)
        {
            ResolveConflictAnswer (slot, drive, entry, chosen, savePath);
            return;
        }

        if (ExternalChangePolicy::IsFileLost (entry.sharedState.GetAskedAction()))
        {
            ResolveLostFileAnswer (slot, drive, entry, chosen, savePath);
            return;
        }

        ReadForAnswer (slot, drive, entry, chosen);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ReadForAnswer
//
//  The file the answer is about, read on the I/O queue with its
//  writability, and the answer carried out in the completion while the same
//  disk is in the bay.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ReadForAnswer (int slot, int drive, Entry & entry, ChangeAction chosen)
{
    shared_ptr<ReloadJob>  job = std::make_shared<ReloadJob>();
    DiskFileOps            ops = m_store.GetFileOps();



    job->slot    = slot;
    job->drive   = drive;
    job->imageId = entry.image->GetImageId();
    job->path    = entry.path;

    m_store.m_ioQueue->Post ([job, ops]
    {
        job->hrRead  = ops.Read (job->path, job->bytes);
        job->current = ops.ReadIdentity (job->path);
        DiskFileOps::ProbeWritability (job->path, job->isReadOnly, job->isDenied);
    },
    [this, job, chosen]
    {
        Entry         & bay    = m_store.GetEntry (job->slot, job->drive);
        ChangeAction    action = chosen;



        if (!bay.mounted || bay.image == nullptr || bay.image->GetImageId() != job->imageId)
        {
            return;
        }

        //  Answering "take it up" about a file that has since gone is not the
        //  same question any more.
        if (FAILED (job->hrRead) && action != ChangeAction::Ignore)
        {
            action = ChangeAction::Deleted;
        }

        //  THE USER DID THE INSERTING, so no program is given as the author.
        //  A question is put only when no intent was stated -- that is what
        //  `Ask` means -- and one that arrives while the question is on
        //  screen does not turn the answer into somebody else's reload.
        CarryOutChangeAction (job->slot, job->drive, action, job->bytes, ChangeAuthor::AnotherProgram, job->current);

        if (m_store.GetEntry (job->slot, job->drive).mounted)
        {
            m_store.SetFileWriteProtect (job->slot, job->drive, job->isReadOnly, job->isDenied);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ResolveConflictAnswer
//
//  "Save as..." after a copy could not be written where this store
//  chose. The disk stays in the drive and the bay moves onto the file
//  the user picked, which is the same outcome as keeping it -- only the
//  folder is theirs rather than ours. Finishing an eject, a disk whose
//  format cannot store its writes is saved as WOZ instead.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ResolveConflictAnswer (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath)
{
    bool      finishEject = entry.sharedState.IsEjectWhenAnswered();
    uint64_t  imageId     = entry.image->GetImageId();



    entry.sharedState.SetAskedAction (ChangeAction::Ignore);

    if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
    {
        m_store.m_save.SaveAnswerCopy (entry, savePath, finishEject, [this, slot, drive, imageId, finishEject] (HRESULT hr, const string & written)
        {
            Entry  & bay = m_store.GetEntry (slot, drive);



            if (bay.mounted && bay.image != nullptr && bay.image->GetImageId() == imageId)
            {
                FinishConflictAnswer (slot, drive, bay, finishEject, hr, written);
            }
        });

        return;
    }

    FinishConflictAnswer (slot, drive, entry, finishEject, S_OK, string());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::FinishConflictAnswer
//
//  After the answer's copy, or at once when there was nothing to write:
//  after a failed copy the question is put again, and otherwise the pending
//  change is dropped and an eject that raised the question finishes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::FinishConflictAnswer (int slot, int drive, Entry & entry, bool finishEject, HRESULT hr, const string & written)
{
    SaveFailureCause  cause   = entry.sharedState.GetAskedCause();
    bool              isAsked = false;
    ChangePrompt      prompt;



    if (FAILED (hr))
    {
        //  There is nowhere to put it. The disk is untouched, and the
        //  question is put again rather than dropped, in the terms of the
        //  first.
        if (m_store.m_askSink)
        {
            prompt = ChangePrompt::ComposeSaveFailure (entry.path, drive, written, hr, cause);

            if (cause == SaveFailureCause::Unserializable)
            {
                prompt.suggestedSavePath = DiskImageStore::MakeRecoveryPath (entry.path, 0);
            }

            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            isAsked = m_store.m_askSink (slot, drive, prompt);
        }

        entry.sharedState.SetAskOutstanding   (isAsked);
        entry.sharedState.SetEjectWhenAnswered (finishEject && isAsked);

        return;
    }

    if (written.empty() && finishEject)
    {
        //  THE USER CHOSE TO LET IT GO, so the dirty bit goes with it.
        //  Left standing, the eject below would find the same collision
        //  and put the same question up again, and the disk they just
        //  discarded would be undiscardable.
        entry.image->ClearDirty();
    }

    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
    }

    //  Dismissed rather than answered, so the name this store had
    //  picked goes back with the question that offered it.
    entry.sharedState.ReleaseUnwrittenReservation();

    //  The eject that raised this question, now that it has an answer.
    //  Either answer leaves nothing for the flush inside it to do: the
    //  copy landed and cleared the dirty bit, or the discard did.
    if (finishEject)
    {
        entry.sharedState.SetEjectWhenAnswered (false);

        m_store.Eject (slot, drive, nullptr);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::ResolveLostFileAnswer
//
//  An answer about a file that has gone is being given to a question
//  that no longer applies. Saving what is held and emptying the drive
//  is the only thing still on offer.
//
//  THE SAVE HAPPENS BEFORE THE EJECT, always. The eject is what
//  discards the in-memory disk, so a failure between the two must
//  leave the disk in the drive rather than gone from both places.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::ResolveLostFileAnswer (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath)
{
    ChangeAction  asked = entry.sharedState.GetAskedAction();



    entry.sharedState.SetAskedAction (ChangeAction::Ignore);

    if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
    {
        SaveLostFileCopy (slot, drive, entry, asked, savePath);
        return;
    }

    //  Nothing was saved, so there is nothing to point at.
    //
    //  THE SHELL CAN ONLY SEND BACK AN ANSWER THE PROMPT OFFERED, so a
    //  third value is a coding error rather than a user choice -- and
    //  one that would empty the drive of a disk existing nowhere else.
    //  The disk stays put rather than paying for the mistake.
    ASSERT (chosen == ChangeAction::Discard);

    if (chosen != ChangeAction::Discard)
    {
        return;
    }

    EjectLostImage (slot, drive);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::SaveLostFileCopy
//
//  The Save as answer about a lost file: the image is serialized here and
//  committed on the I/O queue. In the completion, a save the user asked for
//  and did not get is not followed by throwing the only copy away -- the
//  lost-file question is put again, so its answer is read as one.
//
//  THE DISK STAYS IN THE DRIVE, ON THE FILE THE USER PICKED. Serialize writes
//  a whole image, not a fragment, so the drive now has somewhere to live --
//  and emptying it would mean handing back a complete disk and then making
//  the user go and find it. Keeping a version during a conflict works this
//  way too, and one act should not have two outcomes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::SaveLostFileCopy (int slot, int drive, Entry & entry, ChangeAction asked, const string & savePath)
{
    struct CopyJob
    {
        vector<Byte>       bytes;
        vector<uint64_t>   generations;
        HRESULT            hr = S_OK;
        ImageIdentity      after;
    };

    shared_ptr<CopyJob>  job     = std::make_shared<CopyJob>();
    DiskFileOps          ops     = m_store.GetFileOps();
    uint64_t             imageId = entry.image->GetImageId();



    job->hr          = entry.image->Serialize (job->bytes);
    job->generations = entry.image->GetTrackGenerations();

    m_store.m_ioQueue->Post ([job, ops, savePath]
    {
        if (SUCCEEDED (job->hr))
        {
            job->hr = ops.Commit (savePath, job->bytes, CommitMode::Replace);
        }

        if (SUCCEEDED (job->hr))
        {
            job->after = ops.ReadIdentity (savePath);
        }
    },
    [this, job, slot, drive, imageId, asked, savePath]
    {
        Entry    & bay       = m_store.GetEntry (slot, drive);
        HRESULT    hrRepoint = S_OK;



        if (!bay.mounted || bay.image == nullptr || bay.image->GetImageId() != imageId)
        {
            return;
        }

        if (FAILED (job->hr))
        {
            if (m_store.m_askSink)
            {
                bay.sharedState.SetAskedAction (asked);
                bay.sharedState.SetAskOutstanding (
                    m_store.m_askSink (slot, drive,
                               ChangePrompt::ComposeSaveFailure (bay.path, drive, savePath, job->hr,
                                                                 SaveFailureCause::FileLost)));
            }

            return;
        }

        if (bay.image->GetTrackGenerations() == job->generations)
        {
            bay.image->MarkSaved (job->bytes);
        }

        hrRepoint = RepointBayToFile (slot, drive, savePath, job->after);
        IGNORE_RETURN_VALUE (hrRepoint, S_OK);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::CarryOutChangeAction
//
//  Doing what was decided.
//
//  ONE SHAPE WHETHER THE POLICY DECIDED IT OR THE USER DID. An answer chosen in
//  a dialog arrives here as the same value the policy would have produced, so
//  there is no second implementation of "take it up" that can drift from the
//  first.
//
//  A REPORT IS RAISED ONCE AND ABSORBS WHAT FOLLOWS. Three builds before the
//  developer turns back to the emulator are three pick-ups and one report: the
//  contents reloaded are always the most recent, and three reports about one
//  disk say nothing three times.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                           const vector<Byte> & bytes, ChangeAuthor author,
                                           const ImageIdentity & identity)
{
    HRESULT  hr           = S_OK;
    Entry &  entry        = m_store.GetEntry (slot, drive);
    bool     restarted    = false;
    bool     tookUp       = false;
    bool     preserved    = false;
    bool     preserveFail = false;
    string   preservedPath;
    //  KEEPING MOVES THE BAY ONTO THE COPY, so entry.path is no longer the file
    //  any of these messages are about by the time they are composed. Captured
    //  before anything can move it.
    string   original     = entry.path;



    switch (action)
    {
    case ChangeAction::ReloadInPlace:
    case ChangeAction::Restart:
        hr = MountExternallyModifiedDisk (slot, drive, bytes, identity);

        if (SUCCEEDED (hr))
        {
            tookUp    = true;
            restarted = (action == ChangeAction::Restart);
        }
        else
        {
            //  The bytes passed a trial load and then failed the real one.
            //  Nothing was swapped, so the mounted disk is untouched and the
            //  user is told what they are still holding.
            action = ChangeAction::Unusable;
        }

        break;

    case ChangeAction::KeepHeld:
        //  The user kept the disk that is in the drive, so it gets a file of
        //  its own and the bay moves onto it. The original keeps whatever the
        //  other program wrote.
        //
        //  WRITTEN NOW RATHER THAN AT SOME LATER FLUSH. Deferring it meant the
        //  kept version lived only in memory: a disk the guest had not written
        //  to was not dirty, so quitting or ejecting discarded the very thing
        //  the user had just chosen to keep, without a word. A file on disk is
        //  the only form of "kept" that survives.
        //  The flush may have written it already, under the very name the
        //  question showed. Writing again would make a second copy of the same
        //  disk and leave the first orphaned.
        preservedPath = entry.sharedState.GetPreservedPath();

        if (!entry.sharedState.IsPreservedWritten())
        {
            hr = m_store.m_save.SaveLoadedImage (entry, preservedPath);

            entry.sharedState.SetPreservedPath (preservedPath);

            if (FAILED (hr))
            {
                preserveFail = true;
                break;
            }

            entry.sharedState.SetPreservedWritten (true);
        }

        preserved = true;

        entry.image->ClearDirty();

        //  The disk in the drive is untouched by this; only the file behind it
        //  changes. Repointing also drops the pending change, which is right:
        //  the file it referred to is not this bay's any more.
        hr = RepointBayToFile (slot, drive, preservedPath);
        IGNORE_RETURN_VALUE (hr, S_OK);

        entry.sharedState.ClearPreserved();

        break;

    case ChangeAction::Ignore:
        //  THE NAME GOES BACK WHEN THE QUESTION DOES. It was reserved to put
        //  the question, and it carries the moment the question was put, so
        //  holding it after the user waves the question away labels the next
        //  copy with a timestamp from whenever this happened to be -- and
        //  hands it a name another file may have taken since.
        entry.sharedState.ReleaseUnwrittenReservation();

        break;

    default:
        break;
    }

    if (!preserveFail)
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
    }

    //  Whatever was decided, the conflict is over: either the copy was taken
    //  up as the bay's file or the external version went in beside it.
    //
    //  THE PATH AND THE FACT TRAVEL TOGETHER. A bay holds a reserved name from
    //  the moment a question is put, and holds the name it tried after a write
    //  that failed, so the reserved path indicates where a copy would go and
    //  only `IsPreservedWritten` indicates whether one is there.
    if (tookUp)
    {
        preservedPath = entry.sharedState.GetPreservedPath();
        preserved     = preserved || entry.sharedState.IsPreservedWritten();

        entry.sharedState.ClearPreserved();
    }

    if (restarted && m_store.m_restartCallback)
    {
        m_store.m_restartCallback();
    }

    //  A COPY THAT COULD NOT BE WRITTEN IS A QUESTION, not a notice: it offers
    //  somewhere else to put the file, and a notice has nowhere to put an
    //  answer. It leaves the drive exactly as it was either way.
    if (preserveFail)
    {
        if (m_store.m_askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskOutstanding (true);
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            m_store.m_askSink (slot, drive,
                       ChangePrompt::ComposeSaveFailure (original, drive, preservedPath, hr,
                                                         SaveFailureCause::ExternalChange));
        }

        return;
    }

    //  A FILE FOUND MISSING WHILE ACTING IS A QUESTION TOO. The bytes passed a
    //  trial load and failed the real one, or the re-read behind an answer
    //  found the file gone -- either way what is left is the same offer the
    //  watcher would have made: save the disk somewhere, or let it go. Sent to
    //  the notice bar instead, its buttons did nothing at all; the bar routes
    //  no answers.
    //
    //  THE PENDING RECORD IS ALREADY CLEARED ABOVE, so nothing re-asks behind
    //  this.
    if (ExternalChangePolicy::IsFileLost (action) && m_store.m_askSink)
    {
        entry.sharedState.SetAskedAction (action);

        entry.sharedState.SetAskOutstanding (
            m_store.m_askSink (slot, drive, ChangePrompt::Compose (original, drive, action)));

        return;
    }

    //  EVERY TIME, NOT ONLY THE FIRST. The sink re-words a notice already up
    //  for this bay rather than raising a second, so absorbing further changes
    //  costs nothing -- and reporting only the first left the bar saying
    //  something that had stopped being true.
    if (m_store.m_reportSink)
    {
        ChangePrompt  report;

        //  THE RELOAD IS REPORTED AHEAD OF PRESERVING, because a conflict that
        //  reloaded did both and the reload is the headline; the copy is a
        //  clause on the end of it.
        if (tookUp)
        {
            report = ChangePrompt::ComposeReloadReport (original, drive, restarted,
                                                        m_store.m_machineName, author,
                                                        preservedPath, preserved);
        }
        else if (preserved)
        {
            report = ChangePrompt::ComposeConflictReport (original, drive, preservedPath);
        }
        else
        {
            report = ChangePrompt::Compose (original, drive, action);
        }

        if (!report.title.empty())
        {
            m_store.m_reportSink (slot, drive, report);
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::MountExternallyModifiedDisk
//
//  Swaps a mounted image's contents for the ones on disk.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE POINT. The ordinary remount path
//  flushes what the bay held before loading, which here would write the old
//  disk straight over the change that prompted the swap.
//
//  THE NEW BYTES LOAD INTO A FRESH IMAGE FIRST, so a load that fails leaves the
//  mounted disk exactly as it was. The machine is running and what it holds is
//  known-good; there is no version of this worth half-doing.
//
//  With retention on, the outgoing disk is kept as an ejected one is, so
//  stepping back across the reload can put it back in the drive. It is kept
//  without the change this reload deals with and without the name of any copy
//  of the guest's writes made on the way. Put back with them, the change would
//  be acted on a second time, and the disk's next conflict saved over that
//  copy.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreReload::MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes)
{
    return MountExternallyModifiedDisk (slot, drive, bytes, m_store.ReadIdentity (m_store.GetEntry (slot, drive).path));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::MountExternallyModifiedDisk, with the file's identity
//
//  For a caller whose job read the file and stat'd it after the read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreReload::MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes, const ImageIdentity & identity)
{
    HRESULT                  hr         = S_OK;
    Entry                  & entry      = m_store.GetEntry (slot, drive);
    SalvageAssessment        assessment;
    HRESULT                  hrAssess   = S_OK;
    bool                     usable     = false;
    unique_ptr<DiskImage>    loaded     = make_unique<DiskImage> ();
    Entry                    outgoing;



    loaded->LoadFromBytes (entry.format, bytes, entry.path);

    usable = loaded->IsLoaded();
    CBR (usable);

    //  The drive's open write is a write to the outgoing contents. It is
    //  committed here, so it goes into the disk that is leaving, or into the
    //  copy for history, instead of into the one arriving.
    entry.image->CommitPendingWrite();

    if (m_store.m_media.IsRetaining())
    {
        outgoing.image          = make_unique<DiskImage>();
        *outgoing.image         = std::move (*entry.image);
        outgoing.path           = entry.path;
        outgoing.format         = entry.format;
        outgoing.mounted        = true;
        outgoing.salvageOffered = entry.salvageOffered;
        outgoing.sharedState    = entry.sharedState;

        //  The identity stays the one recorded when this disk was loaded, so
        //  a flush of the disk once it is back still finds the file changed.
        outgoing.sharedState.ClearPending();
        outgoing.sharedState.ClearPreserved();

        m_store.m_media.RetireBay (outgoing);
    }

    //  THE CONTENTS MOVE, THE OBJECT STAYS. The controller holds a raw pointer
    //  to this DiskImage -- SetExternalDisk hands one over at mount -- so
    //  replacing the unique_ptr would leave the drive reading freed memory the
    //  instant a disk was reloaded. Assigning through keeps the address the
    //  drive was given.
    *entry.image = std::move (*loaded);

    //  The identity is refreshed from the file the bytes came from, so the
    //  swap does not immediately look like another external change. The
    //  session a recovery copy was taken from is gone, so its record is too.
    entry.sharedState.SetIdentity (identity);
    entry.sharedState.ClearRecovery();

    hrAssess             = m_store.m_salvage.AssessSalvage (slot, drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    m_store.MarkStatusChanged();

    //  The disk in the drive was replaced under the running machine. Swapped,
    //  not Inserted: the door opens and closes rather than only closing,
    //  because from the user's seat a disk came out and another went in.
    m_store.EmitBayChange (slot, drive, BayChange::Swapped);

    m_store.NotifyMediaChanged();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::RepointBayToFile
//
//  Moves a bay onto a different file. The disk in the drive does not change.
//
//  A SAVE-AS RATHER THAN A MOUNT. Nothing is loaded and nothing is swapped, so
//  a guest reading the drive sees no interruption; only the file this bay will
//  read and write from here on is different.
//
//  THE WATCH MOVES WITH IT, and the old file stops being this bay's business.
//  Two disks out of one folder share a watch, so the common case of a
//  timestamped copy beside its original costs nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreReload::RepointBayToFile (int slot, int drive, const string & newPath)
{
    return RepointBayToFile (slot, drive, newPath, m_store.ReadIdentity (newPath));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::RepointBayToFile, with the new file's identity
//
//  For a caller whose job wrote the new file and stat'd it after, so the
//  repoint does no file I/O.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreReload::RepointBayToFile (int slot, int drive, const string & newPath, const ImageIdentity & identity)
{
    HRESULT  hr     = S_OK;
    bool     usable = DiskImageStore::IsValidBay (slot, drive) && !newPath.empty();



    CBR (usable);

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        CBR (entry.mounted && entry.image != nullptr);

        //  Before the path moves, while EndWatching can still tell which
        //  directory this bay was using.
        EndWatching (slot, drive);

        entry.path = newPath;

        //  A fresh identity for the new file, and nothing pending against the
        //  old one. Mount is what records both.
        entry.sharedState.Mount (identity);

        m_store.MarkStatusChanged();
        BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::BeginWatching
//
//  Starts a watch on the directory holding a bay's image.
//
//  A DIRECTORY THAT CANNOT BE WATCHED IS RECORDED, NOT REPORTED. A network
//  share or a synchronizing folder is exactly the case that produces one, and
//  the guarantee does not live here: the check made before every write is what
//  carries it, and notification only makes it prompt.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::BeginWatching (int slot, int drive)
{
    Entry &  entry     = m_store.GetEntry (slot, drive);
    string   directory = MountedImageState::GetDirectory (entry.path);
    bool     watching  = false;



    if (m_store.m_watcher != nullptr && !directory.empty())
    {
        watching = m_store.m_watcher->Watch (directory,
                                     [this] (const string & path)
                                     {
                                         NoteExternalChange (path, ExternalChangeIntent::Unstated);
                                     });
    }

    entry.sharedState.SetWatching (watching);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::EndWatching
//
//  Drops a bay's watch, unless another bay still needs the same directory.
//
//  TWO DISKS OUT OF ONE FOLDER IS THE ORDINARY CASE -- a boot disk and a work
//  disk built by the same script -- and ejecting the first must not blind the
//  second.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::EndWatching (int slot, int drive)
{
    Entry   & entry      = m_store.GetEntry (slot, drive);
    string    directory  = MountedImageState::GetDirectory (entry.path);
    bool      stillUsed  = false;
    int       otherSlot  = 0;
    int       otherDrive = 0;



    for (otherSlot = 0; otherSlot < DiskImageStore::kSlotCount; otherSlot++)
    {
        for (otherDrive = 0; otherDrive < DiskImageStore::kDriveCount; otherDrive++)
        {
            bool  isSelf = (otherSlot == slot && otherDrive == drive);

            if (!isSelf && m_store.m_entries[otherSlot][otherDrive].mounted)
            {
                string  other = MountedImageState::GetDirectory (m_store.m_entries[otherSlot][otherDrive].path);

                stillUsed = stillUsed || MountedImageState::IsSamePath (other, directory);
            }
        }
    }

    if (m_store.m_watcher != nullptr && !directory.empty() && !stillUsed)
    {
        m_store.m_watcher->Unwatch (directory);
    }

    entry.sharedState.SetWatching (false);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload::EjectLostImage
//
//  Empties a bay whose file has gone.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE DIFFERENCE FROM Eject. The ordinary
//  eject persists a dirty image first, which here would mean writing the disk
//  back to a path the user has just been told no longer exists -- recreating
//  the file they deleted, or failing and reporting a second problem about the
//  first one.
//
//  WHATEVER THE USER WANTED SAVED IS ALREADY SAVED by the time this runs. The
//  offer comes first and this is what follows it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreReload::EjectLostImage (int slot, int drive)
{
    if (!DiskImageStore::IsValidBay (slot, drive))
    {
        return;
    }

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        EndWatching (slot, drive);

        m_store.m_media.RetireBay (entry);
    }

    //  A file that vanished empties the drive the same as a user eject, and
    //  the door and its sound follow the same way.
    m_store.EmitBayChange (slot, drive, BayChange::Ejected);

    m_store.NotifyMediaChanged();

    return;
}




