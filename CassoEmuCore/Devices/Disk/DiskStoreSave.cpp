#include "Pch.h"

#include "DiskStoreSave.h"
#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "ExternalChangePolicy.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::AreWritesSaved
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::AreWritesSaved (Entry & entry)
{
    return !entry.mounted || entry.image == nullptr || !entry.image->IsDirty() || IsRecoveryCurrent (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::IsRecoveryCurrent
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::IsRecoveryCurrent (Entry & entry)
{
    return entry.image != nullptr
        && entry.sharedState.IsRecoveryCurrent (entry.image->GetImageId(), entry.image->GetTrackGenerations())
        && IsRecoveryCopyIntact (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::ReportPreserveFailure
//
//  The guest's version could not be written beside the file that displaced it.
//
//  IT IS THE SAME FAILURE THE WATCHER SIDE ALREADY ASKS ABOUT, so it is asked
//  the same way. Which of the two ends notices a collision first is a matter of
//  timing the user cannot see, and it used to decide whether they were offered
//  somewhere else to put the disk or merely told the write had not happened.
//
//  THE ROUTE DIFFERS BY WHAT IS STILL STANDING. While the machine runs the
//  question is posted and answered later, and nothing is lost while it stands
//  -- the image keeps its dirty bit and a later flush retries. An eject is
//  asked the same way but does not go ahead until the answer comes, because
//  emptying the bay is what would destroy a disk that has nowhere else to be.
//  A shutdown has no pump to deliver a posted question and no CPU thread to act
//  on the answer, but it runs on the UI thread with its apartment still up, so
//  it asks through a blocking dialog instead and writes the copy itself.
//
//  WITH NO QUESTION POSSIBLE, THE DISK STAYS IN THE DRIVE. The guest's version
//  is in no file, so an eject is declined, and the notice states that the disk
//  is in the drive and that another eject raises the question again. At a quit
//  there is no drive for the disk, and the notice states that the changes are
//  lost.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::ReportPreserveFailure (Entry & entry, FlushMoment moment,
                                            const string & attemptedPath, HRESULT hrKeep,
                                            const string & original)
{
    SaveFailureCause  cause     = (moment == FlushMoment::Ejecting)
                                      ? SaveFailureCause::Ejecting
                                      : SaveFailureCause::ExternalChange;
    bool              isAskable = m_store.m_askSink && !entry.sharedState.IsAskOutstanding();
    bool              isAsked   = false;



    if (moment == FlushMoment::ShuttingDown)
    {
        //  The last chance there is. A copy written here is written by this
        //  call rather than by an answer that will never arrive.
        if (!RescueOnTheWayOut (entry, original, false))
        {
            EhmNotifyUser (DiskStoreText::FormatPreserveFailureMessage (original, attemptedPath, hrKeep, entry.drive, moment).c_str());
        }
    }
    else
    {
        if (isAskable)
        {
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);
            entry.sharedState.SetAskedCause  (cause);

            isAsked = m_store.m_askSink (entry.slot, entry.drive,
                                 ChangePrompt::ComposeSaveFailure (original, entry.drive, attemptedPath,
                                                                   hrKeep, cause));

            entry.sharedState.SetAskOutstanding (isAsked);
        }

        //  THE EJECT WAITS ON THE ANSWER, but only when the question actually
        //  went up. Without one the eject is declined, because the disk's
        //  writes are in no file, and a notice takes the question's place.
        if (moment == FlushMoment::Ejecting && isAsked)
        {
            entry.sharedState.SetEjectWhenAnswered (true);
        }
        else if (moment == FlushMoment::Ejecting)
        {
            EhmNotifyUser (DiskStoreText::FormatPreserveFailureMessage (original, attemptedPath, hrKeep, entry.drive, moment).c_str());
        }
        else if (!isAsked)
        {
            EhmNotifyUser (DiskStoreText::FormatExternalChangeMessage (original).c_str());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::RescueOnTheWayOut
//
//  Asks where to put a disk that is about to go, and puts it there.
//
//  SYNCHRONOUS, BECAUSE IT IS THE LAST THING THAT RUNS. No answer is coming
//  back later at this point -- the pump has gone and so has the thread that
//  would act on one -- so the asking and the writing both happen here, inside
//  the call that found the problem.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::RescueOnTheWayOut (Entry & entry, const string & original, bool isUnserializable)
{
    HRESULT       hr        = S_OK;
    string        offered   = isUnserializable ? DiskImageStore::MakeRecoveryPath (original, 0) : original;
    string        chosen;
    string        written;
    bool          isChosen  = false;
    bool          isNative  = false;
    vector<Byte>  held;



    isChosen = m_store.m_rescueSink && m_store.m_rescueSink (offered, chosen) && !chosen.empty();
    CBR (isChosen);

    hr       = entry.image->Serialize (held);
    isNative = SUCCEEDED (hr);

    //  A disk whose format cannot store its writes goes out as WOZ, the one
    //  format that stores them whole.
    if (isNative)
    {
        hr = CommitFile (chosen, held, CommitMode::Replace);
        CHR (hr);

        entry.image->MarkSaved (held);
    }
    else
    {
        hr = WriteWozCopy (entry, chosen, written);
        CHR (hr);
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::AskBeforeEject
//
//  The question an eject raises when its save to the disk's file failed and
//  no recovery copy has the writes. The eject waits for the answer, which
//  finishes it either way.
//
//  A DISK WHOSE FORMAT CANNOT STORE ITS WRITES IS OFFERED A WOZ COPY, at the
//  name the failed recovery copy had, so the picker opens on a .woz name.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::AskBeforeEject (Entry & entry, const FlushOutcome & outcome)
{
    MountedImageState  & state     = entry.sharedState;
    bool                 isAskable = m_store.m_askSink && !state.IsAskOutstanding();
    bool                 isAsked   = false;
    SaveFailureCause     cause     = outcome.isUnserializable ? SaveFailureCause::Unserializable : SaveFailureCause::Unwritable;
    ChangePrompt         prompt;



    if (isAskable)
    {
        if (outcome.isUnserializable)
        {
            prompt = ChangePrompt::ComposeSaveFailure (entry.path, entry.drive, outcome.recoveryPath,
                                                       outcome.recoveryError, cause);

            prompt.suggestedSavePath = DiskImageStore::MakeRecoveryPath (entry.path, 0);
        }
        else
        {
            prompt = ChangePrompt::ComposeSaveFailure (entry.path, entry.drive, entry.path,
                                                       outcome.saveError, cause);
        }

        state.SetAskedAction (ChangeAction::Conflict);
        state.SetAskedCause  (cause);

        isAsked = m_store.m_askSink (entry.slot, entry.drive, prompt);

        state.SetAskOutstanding   (isAsked);
        state.SetEjectWhenAnswered (isAsked);
    }

    return isAsked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::WriteWozCopy
//
//  The bay's image, as WOZ, at the path the user chose with its extension
//  changed to .woz. A sector format cannot store what the guest wrote, so a
//  file with that format's extension would misstate its contents.
//
//  IT BECOMES THE RECOVERY COPY, marked as reported, because the user chose
//  where it went. The bay stays on its file, and at a later save that cannot
//  be stored either, this copy is current and nothing is written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::WriteWozCopy (Entry & entry, const string & path, string & outPath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  wozBytes;



    outPath = fs::path (path).replace_extension (".woz").string();

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    hr = CommitFile (outPath, wozBytes, CommitMode::Replace);
    CHR (hr);

    entry.sharedState.SetRecovery (outPath, entry.image->GetImageId(), entry.image->GetTrackGenerations(), m_store.ReadIdentity (outPath));
    entry.sharedState.MarkSaveFailureReported (outPath);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::SaveAnswerCopy
//
//  Writes the copy for a Save as answer. A native copy moves the bay
//  onto the new file, the same outcome as keeping the disk in any other
//  question. Finishing an eject, an image that does not serialize natively is
//  written as WOZ, with the bay on its file, and becomes the recovery copy,
//  marked as reported. The commit runs on the I/O queue, and done receives
//  the file written or attempted.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::SaveAnswerCopy (Entry & entry, const string & savePath, bool isEjecting, PathDone done)
{
    struct AnswerJob
    {
        int                slot        = 0;
        int                drive       = 0;
        uint64_t           imageId     = 0;
        bool               isWoz       = false;
        string             outPath;
        vector<Byte>       bytes;
        vector<uint64_t>   generations;
        HRESULT            hr          = S_OK;
        ImageIdentity      after;
    };

    shared_ptr<AnswerJob>  job = std::make_shared<AnswerJob>();
    DiskFileOps            ops = m_store.GetFileOps();



    job->slot        = entry.slot;
    job->drive       = entry.drive;
    job->imageId     = entry.image->GetImageId();
    job->generations = entry.image->GetTrackGenerations();
    job->outPath     = savePath;
    job->hr          = entry.image->Serialize (job->bytes);
    job->isWoz       = FAILED (job->hr) && isEjecting;

    if (job->isWoz)
    {
        job->outPath = fs::path (savePath).replace_extension (".woz").string();
        job->hr      = WozLoader::Serialize (*entry.image, job->bytes);
    }

    if (FAILED (job->hr))
    {
        done (job->hr, job->outPath);
    }
    else
    {
        m_store.m_ioQueue->Post ([job, ops]
        {
            job->hr = ops.Commit (job->outPath, job->bytes, CommitMode::Replace);

            if (SUCCEEDED (job->hr))
            {
                job->after = ops.ReadIdentity (job->outPath);
            }
        },
        [this, job, done]
        {
            Entry    * bay       = FindBay (job->slot, job->drive, job->imageId);
            HRESULT    hrRepoint = S_OK;



            if (bay != nullptr && SUCCEEDED (job->hr) && job->isWoz)
            {
                bay->sharedState.SetRecovery (job->outPath, job->imageId, job->generations, job->after);
                bay->sharedState.MarkSaveFailureReported (job->outPath);
            }
            else if (bay != nullptr && SUCCEEDED (job->hr))
            {
                if (bay->image->GetTrackGenerations() == job->generations)
                {
                    bay->image->MarkSaved (job->bytes);
                }

                bay->sharedState.SetPreservedPath (string());

                hrRepoint = m_store.m_reload.RepointBayToFile (bay->slot, bay->drive, job->outPath, job->after);
                IGNORE_RETURN_VALUE (hrRepoint, S_OK);
            }

            done (job->hr, job->outPath);
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::TryWriteRecoveryImage
//
//  Serializes the live image to WOZ and lands it beside the original. WOZ is
//  chosen deliberately over the denibblized sector buffer: that buffer has
//  zeros exactly where the unreadable track should be, so writing it would
//  discard the one thing the refusal was protecting. WozLoader::Serialize takes
//  the per-track bit streams verbatim and is format-agnostic, so a .dsk-sourced
//  mount round-trips whole.
//
//  ONE COPY PER MOUNT. A copy this mount wrote, and that nothing has changed
//  or mounted since, is the session's: when the guest has written nothing
//  since, nothing is written, and otherwise the copy is replaced, the same way
//  an ordinary save replaces the original. Any other file at a recovery name
//  is never overwritten: a new copy takes the first free name.
//
//  The bay's state is read here and the files are read and written in the
//  job. done receives the copy's path, or, when the write failed, the path of
//  the failed write.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::TryWriteRecoveryImage (Entry & entry, PathDone done)
{
    shared_ptr<RecoveryJob>    job        = std::make_shared<RecoveryJob>();
    const MountedImageState  & state      = entry.sharedState;
    DiskFileOps                ops        = m_store.GetFileOps();
    bool                       canCopy    = entry.image != nullptr && !entry.path.empty();
    int                        otherDrive = -1;



    job->done = std::move (done);

    if (!canCopy)
    {
        job->hr = E_FAIL;
        CompleteRecovery (job);
    }
    else
    {
        job->slot         = entry.slot;
        job->drive        = entry.drive;
        job->imageId      = entry.image->GetImageId();
        job->generations  = entry.image->GetTrackGenerations();
        job->imagePath    = entry.path;
        job->copyPath     = state.GetRecoveryPath();
        job->copyIdentity = state.GetRecoveryIdentity();
        job->isElsewhere  = !job->copyPath.empty() && m_store.IsFileInAnotherBay (job->copyPath, entry.slot, entry.drive, otherDrive);
        job->isCurrent    = state.IsRecoveryCurrent (job->imageId, job->generations);
        job->isNameFixed  = m_store.m_flushSink && m_store.m_fileIo == nullptr;
        job->hrWoz        = WozLoader::Serialize (*entry.image, job->wozBytes);

        m_store.m_ioQueue->Post ([job, ops] { RunRecoveryWork (*job, ops); },
                                 [this, job] { CompleteRecovery (job); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::RunRecoveryWork
//
//  I/O thread: whether the copy this mount wrote is as it was written, and
//  the write when one is due.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::RunRecoveryWork (RecoveryJob & job, const DiskFileOps & ops)
{
    bool  isIntact = !job.copyPath.empty() && !job.isElsewhere &&
                     (!job.copyIdentity.recorded || ops.ReadIdentity (job.copyPath).Matches (job.copyIdentity));



    if (isIntact && job.isCurrent)
    {
        job.outPath = job.copyPath;
    }
    else if (FAILED (job.hrWoz))
    {
        job.hr = job.hrWoz;
    }
    else if (isIntact)
    {
        job.outPath   = job.copyPath;
        job.hr        = ops.Commit (job.copyPath, job.wozBytes, CommitMode::Replace);
        job.isWritten = true;
    }
    else
    {
        job.hr        = ops.WriteNewRecoveryImage (job.imagePath, job.wozBytes, job.isNameFixed, job.outPath);
        job.isWritten = true;
    }

    if (job.isWritten && SUCCEEDED (job.hr))
    {
        job.after = ops.ReadIdentity (job.outPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::CompleteRecovery
//
//  A copy written becomes the bay's recovery copy, while the disk is in it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::CompleteRecovery (const shared_ptr<RecoveryJob> & job)
{
    Entry  * entry = FindBay (job->slot, job->drive, job->imageId);



    if (entry != nullptr && job->isWritten && SUCCEEDED (job->hr))
    {
        entry->sharedState.SetRecovery (job->outPath, job->imageId, job->generations, job->after);
    }

    if (job->done)
    {
        job->done (job->hr, job->outPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::IsRecoveryCopyIntact
//
//  Whether the bay's recovery copy is as this mount wrote it: its identity
//  matches the identity read after the write, or there is none (a flush sink
//  has no file to stat), and no other bay has it mounted.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::IsRecoveryCopyIntact (const Entry & entry)
{
    const MountedImageState  & state       = entry.sharedState;
    const ImageIdentity      & recorded    = state.GetRecoveryIdentity();
    bool                       isSame      = !recorded.recorded || m_store.ReadIdentity (state.GetRecoveryPath()).Matches (recorded);
    int                        otherDrive  = -1;
    bool                       isElsewhere = m_store.IsFileInAnotherBay (state.GetRecoveryPath(), entry.slot, entry.drive, otherDrive);



    return isSame && !isElsewhere;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::WriteNewRecoveryImage
//
//  Writes a recovery copy at the first free recovery name, never over a file
//  that is there: a name that is taken, or taken between the check and the
//  write, steps to the next one.
//
//  With a flush sink and no file seam there is no file to collide with, so
//  the first name is used.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::WriteNewRecoveryImage (const Entry & entry, const vector<Byte> & wozBytes, string & outPath)
{
    return m_store.GetFileOps().WriteNewRecoveryImage (entry.path, wozBytes, m_store.m_flushSink && m_store.m_fileIo == nullptr, outPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FlushEntry
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::FlushEntry (Entry & entry, FlushMoment moment)
{
    HRESULT       hr = S_OK;
    FlushOutcome  outcome;



    hr = FlushEntry (entry, moment, outcome);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry, with what a failed save produced
//
//  The save and its report. outOutcome indicates the save's error and the
//  recovery copy written, if any, for a caller that acts on the result. With
//  the inline queue the save is over when Save returns; with a threaded one
//  this waits for its completion.
//
//  ONLY WHAT THIS SAVE PRODUCED IS WRITTEN TO outOutcome. A remount of the same
//  file saves twice into one outcome, and the second save, with nothing left
//  to write, must not erase the copy the first one moved the guest's version
//  to.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::FlushEntry (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome)
{
    HRESULT             hr   = S_OK;
    shared_ptr<IoWait>  wait = std::make_shared<IoWait>();



    Save (entry, moment, [wait] (HRESULT hrSave, const FlushOutcome & outcome)
    {
        wait->hr      = hrSave;
        wait->outcome = outcome;
        wait->isDone  = true;
    });

    hr = WaitForIo (*wait);
    MergeOutcome (wait->outcome, outOutcome);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::MergeOutcome
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::MergeOutcome (const FlushOutcome & from, FlushOutcome & into)
{
    if (FAILED (from.saveError))
    {
        into.saveError = from.saveError;
    }

    if (from.isUnserializable)
    {
        into.isUnserializable = true;
    }

    if (!from.recoveryPath.empty() || FAILED (from.recoveryError))
    {
        into.recoveryPath  = from.recoveryPath;
        into.recoveryError = from.recoveryError;
    }

    if (!from.movedTo.empty())
    {
        into.movedFrom = from.movedFrom;
        into.movedTo   = from.movedTo;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::Save
//
//  A failure to save a DIRTY image is never silent. Callers that do not act
//  on the result drop it (Eject and PowerCycle are void; the shell and
//  SoftReset ignore it), so the loss is reported from the save's completion,
//  through the shared EHM notifier. The image is dirty after a failure, so a
//  later save retries.
//
//  ONE SAVE OF A DISK AT A TIME. Two saves of one disk in flight could land
//  in either order, and the older bytes could replace the newer ones. A save
//  requested meanwhile is queued, and starts when the one in flight is over.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::Save (Entry & entry, FlushMoment moment, SaveDone done)
{
    bool      isNeeded  = IsSaveNeeded (entry);
    bool      isWaiting = false;
    uint64_t  imageId   = 0;



    if (isNeeded)
    {
        imageId   = entry.image->GetImageId();
        isWaiting = m_waiting.contains (imageId);
    }

    if (!isNeeded)
    {
        if (done)
        {
            done (S_OK, FlushOutcome());
        }
    }
    else if (isWaiting)
    {
        m_waiting[imageId].push_back (SaveRequest { moment, std::move (done) });
    }
    else
    {
        m_waiting[imageId];
        StartSave (entry, moment, std::move (done));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::IsSaveNeeded
//
//  No-op cases -- nothing dirty to persist -- succeed silently. There is
//  deliberately no way to force a save past these gates. The one former use
//  of one changed a write-protect flag, and SetImageWriteProtect patches that
//  flag's byte instead of rebuilding the whole image.
//
//  A replay recomputes a past the host has already run, and none of it is
//  written to a file. A store with no files, such as a scratch machine's,
//  saves nothing.
//
//  There is deliberately no warning here about overwriting an image whose
//  stored checksum did not validate at load. A checksum mismatch
//  write-protects the image, so the gate returns before a save and the file
//  is never rewritten. The mismatch is reported at mount instead, which is
//  earlier, and is when the user decides.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::IsSaveNeeded (Entry & entry)
{
    bool  isNeeded = !m_store.m_isReplaying && entry.mounted && entry.image != nullptr;



    // A write open on a flux track is not in the image until it is
    // committed, and the image is not dirty before that.
    if (isNeeded)
    {
        entry.image->CommitPendingWrite();
    }

    if (isNeeded && !m_store.m_isFileBacked)
    {
        entry.image->ClearDirty();
        isNeeded = false;
    }

    isNeeded = isNeeded && entry.image->IsDirty();

    if (isNeeded && entry.image->IsWriteProtected())
    {
        entry.image->ClearDirty();
        isNeeded = false;
    }

    return isNeeded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::StartSave
//
//  Serializes the image on this thread, so no other thread reads it, and
//  posts the file work with the bytes. The track generations are taken with
//  the bytes: the completion marks the image saved only when the guest has
//  not written since.
//
//  Serialization fails rather than emitting a buffer with zeros where a
//  track could not be read. The work then only checks the file, and the
//  completion writes a recovery copy.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::StartSave (Entry & entry, FlushMoment moment, SaveDone done)
{
    shared_ptr<SaveJob>  job = std::make_shared<SaveJob>();
    DiskFileOps          ops = m_store.GetFileOps();



    job->slot        = entry.slot;
    job->drive       = entry.drive;
    job->imageId     = entry.image->GetImageId();
    job->moment      = moment;
    job->done        = std::move (done);
    job->path        = entry.path;
    job->recorded    = entry.sharedState.GetIdentity();
    job->hasTarget   = m_store.m_flushSink || !entry.path.empty();
    job->hrSerialize = entry.image->Serialize (job->bytes);
    job->generations = entry.image->GetTrackGenerations();

    m_store.m_ioQueue->Post ([job, ops] { RunSaveWork (*job, ops); },
                             [this, job] { CompleteSave (job); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::RunSaveWork
//
//  I/O thread. Immediately before committing, and whatever any watcher has
//  or has not reported, the file is compared with the bay's identity from
//  its mount or last save. THIS IS THE GUARANTEE AND NOTIFICATION IS ONLY
//  PROMPTNESS: a
//  missed notification, an unwatchable share, a change that arrived while
//  the machine was busy -- all of them end here, one stat before the write.
//
//  A BAY THAT NEVER RECORDED AN IDENTITY IS NOT CHECKED. MountFromBytes has
//  no file behind it, and a test that redirects reads into memory has none
//  either; comparing against a stat that never ran would refuse writes that
//  nothing has endangered.
//
//  Never write in place: a save that fails midway would otherwise have
//  already truncated the user's image, trading a stale file for no file at
//  all. The commit goes through a temporary.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::RunSaveWork (SaveJob & job, const DiskFileOps & ops)
{
    bool  isCommitting = false;



    if (job.recorded.recorded)
    {
        job.isChanged = !job.recorded.Matches (ops.ReadIdentity (job.path));
    }

    isCommitting = !job.isChanged && SUCCEEDED (job.hrSerialize) && job.hasTarget;

    if (isCommitting)
    {
        job.hrCommit = ops.Commit (job.path, job.bytes, CommitMode::Replace);
    }

    if (!job.isChanged && SUCCEEDED (job.hrSerialize) && SUCCEEDED (job.hrCommit) && job.recorded.recorded)
    {
        job.after = ops.ReadIdentity (job.path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::CompleteSave
//
//  What the work found, applied to the bay while the disk is in it: a
//  preserved copy when another program changed the file, a recovery copy
//  when the image did not serialize, the report of a failed commit, or the
//  record of a save that landed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::CompleteSave (const shared_ptr<SaveJob> & job)
{
    Entry       * entry = FindBay (job->slot, job->drive, job->imageId);
    FlushOutcome  outcome;



    if (entry == nullptr)
    {
        FinishSave (job->imageId, S_OK, outcome, job->done);
    }
    else if (job->isChanged)
    {
        PreserveAfterChange (*entry, job);
    }
    else if (FAILED (job->hrSerialize))
    {
        SaveUnserializable (*entry, job);
    }
    else if (FAILED (job->hrCommit))
    {
        outcome.saveError = job->hrCommit;
        ReportSaveFailure (*entry, job->moment, outcome);
        FinishSave (job->imageId, job->hrCommit, outcome, job->done);
    }
    else
    {
        RecordSave (*entry, *job);
        FinishSave (job->imageId, S_OK, outcome, job->done);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::RecordSave
//
//  After a save lands. The file now has a freshly computed CRC that matches
//  it, so the mismatch is no longer true of what is on disk -- and the
//  warning must not repeat on every later eject or power cycle.
//
//  The bytes become the image's saved state only when the guest has not
//  written since they were taken. Otherwise the image stays dirty, and the
//  next save writes the newer tracks.
//
//  The file this store just wrote is a change to it. Without the identity
//  refresh, the next save would compare the file against the mount-time
//  identity, find this commit, and decline as for another program's change.
//  NOT SEPARABLE FROM RunSaveWork's CHECK: the two are one mechanism, and the
//  first one alone breaks the second save of every ordinary session.
//
//  Only for a bay that had an identity to begin with, so a mount from bytes
//  does not acquire one by being saved.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::RecordSave (Entry & entry, const SaveJob & job)
{
    entry.image->SetSourceCrcMismatch (false);

    if (entry.image->GetTrackGenerations() == job.generations)
    {
        entry.image->MarkSaved (job.bytes);
    }

    entry.sharedState.ClearRecovery();
    m_store.MarkStatusChanged();

    if (job.recorded.recorded)
    {
        entry.sharedState.SetIdentity (job.after);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::PreserveAfterChange
//
//  THE SAME CONFLICT THE WATCHER FINDS, discovered at the other end.
//  Notification failed, or the change landed while this write was already
//  under way, and the guest's version is about to go over a version this
//  store never took up.
//
//  THE FILE STAYS WITH WHOEVER CHANGED IT. The guest's version moves to a
//  separate file and the bay follows it; the original has what the other
//  program wrote and is not touched here at all. The watcher path applies
//  exactly this rule, so one collision produces the same two files whichever
//  end found it.
//
//  The copy is written under the name shown in a question on screen, when
//  there is one. The name is reserved once, wherever that happens first.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::PreserveAfterChange (Entry & entry, const shared_ptr<SaveJob> & job)
{
    string  original      = entry.path;
    bool    isAsked       = entry.sharedState.IsAskOutstanding();
    string  preservedPath = entry.sharedState.GetPreservedPath();



    SaveLoadedImage (entry, preservedPath, [this, job, original, isAsked] (HRESULT hrKeep, const string & triedPath, const ImageIdentity & identity)
    {
        FinishPreserve (job, original, isAsked, hrKeep, triedPath, identity);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FinishPreserve
//
//  A preserve that did not happen stops the save. The image is DIRTY AFTER
//  IT, which is the difference between declining and losing: the guest's
//  writes are in memory and can be saved once there is somewhere to put
//  them. STG_E_NOTCURRENT indicates precisely this -- the object changed
//  since it was last read.
//
//  The name of the copy is set on the bay whether or not the write landed,
//  so a retry goes to the file in the notice.
//
//  WITH A QUESTION ALREADY ON SCREEN, THE ANSWER SETTLES WHAT HAPPENS NEXT.
//  Writing the copy is not optional -- the guest's work would be gone
//  otherwise -- but moving the bay and reporting happen when the user
//  answers. Doing them here settled the matter underneath an open dialog,
//  which left the user being asked about a file the bay no longer had. The
//  copy is on disk under the name that dialog is showing, so either answer
//  does what it states.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::FinishPreserve (const shared_ptr<SaveJob> & job, const string & original, bool isAsked,
                                    HRESULT hrKeep, const string & triedPath, const ImageIdentity & identity)
{
    Entry       * entry  = FindBay (job->slot, job->drive, job->imageId);
    FlushOutcome  outcome;
    HRESULT       hr     = S_OK;
    HRESULT       hrMove = S_OK;



    if (entry != nullptr)
    {
        entry->sharedState.SetPreservedPath (triedPath);
    }

    if (entry != nullptr && FAILED (hrKeep))
    {
        ReportPreserveFailure (*entry, job->moment, triedPath, hrKeep, original);
        hr = STG_E_NOTCURRENT;
    }
    else if (entry != nullptr)
    {
        //  On disk under a separate name, so the bay has nothing unsaved.
        entry->sharedState.SetPreservedWritten (true);
        entry->image->SetSourceCrcMismatch (false);
        m_store.MarkStatusChanged();

        //  The question on screen about a file that is gone reads as though the
        //  disk exists in memory only; a second dialog states the copy.
        if (isAsked && ExternalChangePolicy::IsFileLost (entry->sharedState.GetAskedAction()))
        {
            EhmNotifyUser (DiskStoreText::FormatCopySavedMessage (entry->drive, triedPath).c_str());
        }
    }

    if (entry != nullptr && SUCCEEDED (hrKeep) && !isAsked)
    {
        hrMove = m_store.m_reload.RepointBayToFile (entry->slot, entry->drive, triedPath, identity);
        IGNORE_RETURN_VALUE (hrMove, S_OK);

        //  Composed from the path the bay HAD, since that is the file the
        //  message is about and the repoint above has already moved it. Before
        //  a mount, the mount reports the copy, in a text that depends on its
        //  end.
        if (job->moment == FlushMoment::Replacing)
        {
            outcome.movedFrom = original;
            outcome.movedTo   = triedPath;
        }
        else if (m_store.m_reportSink)
        {
            m_store.m_reportSink (entry->slot, entry->drive,
                                  ChangePrompt::ComposeConflictReport (original, entry->drive, triedPath));
        }
    }

    FinishSave (job->imageId, hr, outcome, job->done);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::SaveUnserializable
//
//  An image that would not serialize, in a file nobody changed: the session
//  is preserved losslessly beside the original before the loss is reported,
//  and the report includes the copy, because a save that cannot land is
//  exactly when the session's only lossless copy of the disk is the one
//  beside the original.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::SaveUnserializable (Entry & entry, const shared_ptr<SaveJob> & job)
{
    TryWriteRecoveryImage (entry, [this, job] (HRESULT hrCopy, const string & copyPath)
    {
        Entry       * bay = FindBay (job->slot, job->drive, job->imageId);
        FlushOutcome  outcome;



        outcome.saveError        = job->hrSerialize;
        outcome.isUnserializable = true;
        outcome.recoveryError    = hrCopy;
        outcome.recoveryPath     = copyPath;

        if (bay != nullptr)
        {
            ReportSaveFailure (*bay, job->moment, outcome);
        }

        FinishSave (job->imageId, job->hrSerialize, outcome, job->done);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FinishSave
//
//  The save is over: its done function receives the result, and the saves
//  requested while it was in flight start, in the order of the requests. A
//  disk that has left its bay has nothing left to save.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::FinishSave (uint64_t imageId, HRESULT hr, const FlushOutcome & outcome, SaveDone & done)
{
    std::vector<SaveRequest>  waiting;
    Entry                   * entry = nullptr;
    auto                      it    = m_waiting.find (imageId);



    if (it != m_waiting.end())
    {
        waiting = std::move (it->second);
        m_waiting.erase (it);
    }

    if (done)
    {
        done (hr, outcome);
    }

    for (SaveRequest & request : waiting)
    {
        entry = FindImage (imageId);

        if (entry != nullptr)
        {
            Save (*entry, request.moment, std::move (request.done));
        }
        else if (request.done)
        {
            request.done (S_OK, FlushOutcome());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FindBay
//
////////////////////////////////////////////////////////////////////////////////

DiskStoreSave::Entry * DiskStoreSave::FindBay (int slot, int drive, uint64_t imageId)
{
    Entry  & entry = m_store.GetEntry (slot, drive);



    if (entry.mounted && entry.image != nullptr && entry.image->GetImageId() == imageId)
    {
        return &entry;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FindImage
//
////////////////////////////////////////////////////////////////////////////////

DiskStoreSave::Entry * DiskStoreSave::FindImage (uint64_t imageId)
{
    Entry  * found = nullptr;
    int      slot  = 0;
    int      drive = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount && found == nullptr; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount && found == nullptr; drive++)
        {
            found = FindBay (slot, drive, imageId);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::WaitForIo
//
//  For a caller that uses a result before it returns. With the inline queue
//  the work is done; with a threaded one the store's thread drains until it
//  is, for at most s_kSyncIoTimeoutMs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::WaitForIo (const IoWait & wait)
{
    HRESULT  hr = S_OK;



    if (!wait.isDone)
    {
        hr = m_store.m_ioQueue->WaitUntilIdle (s_kSyncIoTimeoutMs);
        CHR (hr);
    }

    CBREx (wait.isDone, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    hr = wait.hr;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportSaveFailure
//
//  Tells the user a dirty image could not be saved, through the shared EHM
//  notifier. The recovery copy's path is in the text when one was written.
//
//  A FAILED SAVE NOBODY ASKED FOR IS REPORTED ONCE PER MOUNT. Every spindown
//  saves again, and a failure reported at every one buries the user in the
//  same notice. A new recovery copy is news, and so is the first failure with
//  no copy. Every failure of a save the user asked for is reported.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::ReportSaveFailure (Entry & entry, FlushMoment moment, const FlushOutcome & outcome)
{
    MountedImageState  & state    = entry.sharedState;
    string               copyPath = SUCCEEDED (outcome.recoveryError) ? outcome.recoveryPath : string();
    bool                 hasCopy  = !copyPath.empty();
    bool                 isNews   = state.ShouldReportSaveFailure (copyPath);
    bool                 isShown  = true;



    //  Before a mount, only a first recovery copy is news: a failure with
    //  writes in no file declines the mount, and the decline is the report.
    //  With a copy, the eject goes ahead and the quit discards nothing, so the
    //  copy is reported as at a background save. Without one, the eject raises
    //  a question about where the changes go and the quit offers the rescue,
    //  and the notice is for when neither happens.
    if (moment == FlushMoment::Replacing)
    {
        isShown = hasCopy && isNews;
    }
    else if (moment == FlushMoment::Background || (hasCopy && moment != FlushMoment::Running))
    {
        isShown = isNews;
    }
    else if (moment == FlushMoment::Ejecting)
    {
        isShown = !AskBeforeEject (entry, outcome);
    }
    else if (moment == FlushMoment::ShuttingDown)
    {
        isShown = !RescueOnTheWayOut (entry, entry.path, outcome.isUnserializable);
    }

    if (isShown)
    {
        EhmNotifyUser (DiskStoreText::FormatFlushLossMessage (entry.path, outcome, moment).c_str());
        state.MarkSaveFailureReported (copyPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::DoesPathExist
//
//  Whether something is already sitting at a path.
//
//  THROUGH THE FILE SEAM WHEN ONE IS INSTALLED, so a test that keeps its files
//  in memory is asked about the world it actually has. Reaching past it to the
//  filesystem would let the collision loop below hand out a name a test already
//  used.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreSave::DoesPathExist (const string & path) const
{
    return m_store.GetFileOps().Exists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::FindFreePreservedPath
//
//  A preserved-copy name nothing is sitting at yet.
//
//  THE LOOP IS THE POINT. A one-second timestamp cannot keep the promise that
//  repeated conflicts accumulate rather than overwrite each other, and two
//  conflicts on one image inside a second is exactly what a build loop makes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::FindFreePreservedPath (const string & imagePath, string & outPath) const
{
    return m_store.GetFileOps().FindFreePreservedPath (imagePath, PreservedCopy::MakeStamp (m_store.m_timestamp ? m_store.m_timestamp() : time (nullptr)), outPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::CommitFile
//
//  A write whose result its caller uses before it returns: a salvaged copy,
//  a copy chosen in an answer. It runs on the I/O queue, and this waits for
//  it.
//
//  THROUGH THE FLUSH SINK WHEN ONE IS INSTALLED, so a test captures every
//  write in one place. Otherwise through DurableCommit, over the file seam when
//  the shell installed one and over a local Win32DiskFileIo when not, so a
//  copy or an image is never found half-written. A half-written preserved copy
//  is worthless at the moment the user opens it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::CommitFile (const string & path, const vector<Byte> & bytes, CommitMode mode)
{
    HRESULT             hr   = S_OK;
    shared_ptr<IoWait>  wait = std::make_shared<IoWait>();
    DiskFileOps         ops  = m_store.GetFileOps();



    m_store.m_ioQueue->Post ([wait, ops, path, bytes, mode] { wait->hr = ops.Commit (path, bytes, mode); },
                             [wait] { wait->isDone = true; });

    hr = WaitForIo (*wait);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::SaveLoadedImage
//
//  Writes the bay's disk to a separate file, for a caller that uses the
//  result before it returns. outPath is the name to use, or empty for a free
//  preserved-copy name; afterward it is the path written to or attempted,
//  because the notice for a failed copy prints that path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::SaveLoadedImage (Entry & entry, string & outPath)
{
    HRESULT             hr   = S_OK;
    shared_ptr<IoWait>  wait = std::make_shared<IoWait>();



    wait->path = outPath;

    SaveLoadedImage (entry, outPath, [wait] (HRESULT hrSave, const string & path, const ImageIdentity & identity)
    {
        UNREFERENCED_PARAMETER (identity);

        wait->hr     = hrSave;
        wait->path   = path;
        wait->isDone = true;
    });

    hr      = WaitForIo (*wait);
    outPath = wait->path;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::SaveLoadedImage, through the queue
//
//  SERIALIZED FROM THE MOUNTED IMAGE, NOT COPIED FROM THE FILE. The copy
//  exists because the file no longer has this version.
//
//  A name already reserved is used as it is. Choosing a fresh one here is
//  what let a question and a save disagree about where the copy went.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::SaveLoadedImage (Entry & entry, const string & path, CopyDone done)
{
    shared_ptr<vector<Byte>>  bytes       = std::make_shared<vector<Byte>>();
    shared_ptr<string>        target      = std::make_shared<string> (path);
    shared_ptr<HRESULT>       hrWrite     = std::make_shared<HRESULT> (S_OK);
    shared_ptr<ImageIdentity> identity    = std::make_shared<ImageIdentity>();
    DiskFileOps               ops         = m_store.GetFileOps();
    HRESULT                   hr          = (entry.image != nullptr) ? entry.image->Serialize (*bytes) : E_FAIL;
    string                    imagePath   = entry.path;
    string                    stamp       = PreservedCopy::MakeStamp (m_store.m_timestamp ? m_store.m_timestamp() : time (nullptr));
    int                       slot        = entry.slot;
    int                       drive       = entry.drive;
    uint64_t                  imageId     = (entry.image != nullptr) ? entry.image->GetImageId() : 0;
    vector<uint64_t>          generations = (entry.image != nullptr) ? entry.image->GetTrackGenerations() : vector<uint64_t>();



    if (FAILED (hr))
    {
        if (done)
        {
            done (hr, path, ImageIdentity());
        }
    }
    else
    {
        m_store.m_ioQueue->Post ([ops, bytes, target, hrWrite, identity, imagePath, stamp]
        {
            if (target->empty())
            {
                *hrWrite = ops.FindFreePreservedPath (imagePath, stamp, *target);
            }

            if (SUCCEEDED (*hrWrite))
            {
                *hrWrite = ops.Commit (*target, *bytes, CommitMode::Replace);
            }

            if (SUCCEEDED (*hrWrite))
            {
                *identity = ops.ReadIdentity (*target);
            }
        },
        [this, bytes, target, hrWrite, identity, slot, drive, imageId, generations, done]
        {
            Entry  * bay = FindBay (slot, drive, imageId);



            if (bay != nullptr && SUCCEEDED (*hrWrite) && bay->image->GetTrackGenerations() == generations)
            {
                bay->image->MarkSaved (*bytes);
            }

            if (done)
            {
                done (*hrWrite, *target, *identity);
            }
        });
    }
}
