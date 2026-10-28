#include "Pch.h"

#include "DiskStoreSave.h"
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
//  written as WOZ, with the bay on its file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::SaveAnswerCopy (Entry & entry, const string & savePath, bool isEjecting, string & outPath)
{
    HRESULT       hr        = S_OK;
    HRESULT       hrRepoint = S_OK;
    bool          isWoz     = false;
    vector<Byte>  held;



    outPath = savePath;

    hr    = entry.image->Serialize (held);
    isWoz = FAILED (hr) && isEjecting;

    if (isWoz)
    {
        hr = WriteWozCopy (entry, savePath, outPath);
        CHR (hr);
    }
    else
    {
        CHR (hr);

        hr = CommitFile (savePath, held, CommitMode::Replace);
        CHR (hr);

        entry.image->MarkSaved (held);
        entry.sharedState.SetPreservedPath (string());

        hrRepoint = m_store.m_reload.RepointBayToFile (entry.slot, entry.drive, savePath);
        IGNORE_RETURN_VALUE (hrRepoint, S_OK);
    }

Error:
    return hr;
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
//  outPath is the copy's path, or, when the write failed, the path of the failed write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::TryWriteRecoveryImage (Entry & entry, string & outPath)
{
    HRESULT              hr         = S_OK;
    MountedImageState  & state      = entry.sharedState;
    bool                 haveImage  = entry.image != nullptr;
    bool                 havePath   = !entry.path.empty();
    bool                 isIntact   = false;
    bool                 isCurrent  = false;
    vector<Byte>         wozBytes;



    outPath.clear();

    CBR (haveImage);
    CBR (havePath);

    isIntact  = !state.GetRecoveryPath().empty() && IsRecoveryCopyIntact (entry);
    isCurrent = isIntact && state.IsRecoveryCurrent (entry.image->GetImageId(), entry.image->GetTrackGenerations());

    if (isCurrent)
    {
        outPath = state.GetRecoveryPath();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    if (isIntact)
    {
        outPath = state.GetRecoveryPath();

        hr = CommitFile (outPath, wozBytes, CommitMode::Replace);
        CHR (hr);
    }
    else
    {
        hr = WriteNewRecoveryImage (entry, wozBytes, outPath);
        CHR (hr);
    }

    state.SetRecovery (outPath, entry.image->GetImageId(), entry.image->GetTrackGenerations(), m_store.ReadIdentity (outPath));

Error:
    return hr;
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
    HRESULT  hr          = S_OK;
    int      attempt     = 0;
    bool     isDone      = false;
    bool     isNameFixed = m_store.m_flushSink && m_store.m_fileIo == nullptr;
    bool     isFree      = false;
    bool     isTaken     = false;



    for (attempt = 0; !isDone && attempt < DiskImageStore::kMaxRecoveryNameAttempts; attempt++)
    {
        outPath = DiskImageStore::MakeRecoveryPath (entry.path, attempt);
        isFree  = isNameFixed || !DoesPathExist (outPath);

        if (isFree)
        {
            hr      = CommitFile (outPath, wozBytes, CommitMode::CreateNew);
            isTaken = hr == HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS) || hr == HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS);
            isDone  = isNameFixed || !isTaken;
        }
    }

    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry
//
//  Centralized flush helper. Dispatches through SetFlushSink when the test
//  hook is installed; otherwise writes to the host filesystem. Does nothing
//  if the image is clean or no source path is recorded. A genuine failure
//  to persist a dirty image is surfaced to the user via the shared EHM
//  notifier (ReportSaveFailure), because every caller drops the return value.
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
//  recovery copy written, if any, for a caller that acts on the result.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::FlushEntry (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome)
{
    HRESULT        hr          = S_OK;
    bool           isPreserved = false;
    vector<Byte>   bytes;



    // No-op cases -- nothing dirty to persist -- succeed silently. There is
    // deliberately no way to force a flush past these gates: the only caller
    // that wanted one was changing a write-protect flag, and rebuilding a
    // whole image to carry one bit is what SetImageWriteProtect exists to
    // avoid. An API that cannot be asked to do that cannot be misused into it.
    //  A replay recomputes a past the host already saw; nothing it does
    //  reaches a file.
    BAIL_OUT_IF (m_store.m_isReplaying, S_OK);
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);

    // A write still open on a flux track has not reached the image yet, so
    // the image is not dirty until it is committed.
    entry.image->CommitPendingWrite();

    //  A store with no files, such as a scratch machine's, saves nothing.
    if (!m_store.m_isFileBacked)
    {
        entry.image->ClearDirty();
        BAIL_OUT_IF (true, S_OK);
    }

    BAIL_OUT_IF (!entry.image->IsDirty(), S_OK);

    if (entry.image->IsWriteProtected())
    {
        entry.image->ClearDirty();
        BAIL_OUT_IF (true, S_OK);
    }

    // A genuine failure to persist a DIRTY image must not vanish: every
    // caller drops FlushEntry's HRESULT (Eject / PowerCycle are void; the
    // shell / SoftReset IGNORE_RETURN_VALUE it), so the loss is surfaced
    // here through the shared EHM notifier rather than a return nobody
    // checks. The image keeps its dirty bit on failure so a later flush
    // can retry.
    // Serialization now refuses rather than quietly emitting a buffer with
    // zeros where a track could not be read. That protects the file on disk,
    // but it strands the session unless the session goes somewhere -- so
    // preserve it losslessly beside the original before reporting the loss.
    //
    // There is deliberately no warning here about overwriting an image whose
    // stored checksum did not validate at load. There used to be, and it is
    // no longer reachable: a checksum mismatch now write-protects the image,
    // so the gate above returns before this point and the file is never
    // rewritten. The user is told at mount instead, which is earlier and is
    // where the decision actually gets made.
    hr = PreserveConflictingCopy (entry, moment, isPreserved, outOutcome);
    CHR (hr);

    //  Nothing is written over the original. It is not this bay's file any
    //  more.
    BAIL_OUT_IF (isPreserved, S_OK);

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        outOutcome.saveError        = hr;
        outOutcome.isUnserializable = true;
        outOutcome.recoveryError    = TryWriteRecoveryImage (entry, outOutcome.recoveryPath);
    }

    CHRF (hr, ReportSaveFailure (entry, moment, outOutcome));

    // Never write in place: a flush that fails midway would otherwise have
    // already truncated the user's image, trading a stale file for no file at
    // all. The dirty bit survives a failure, so a later flush retries.
    //
    // The failure message includes the recovery copy written above, because a
    // flush that cannot land is exactly when the session's only lossless copy
    // of the disk is the one sitting beside the original.
    if (m_store.m_flushSink || !entry.path.empty())
    {
        hr                   = CommitFile (entry.path, bytes, CommitMode::Replace);
        outOutcome.saveError = hr;
        CHRF (hr, ReportSaveFailure (entry, moment, outOutcome));
    }

    RecordCommittedFile (entry, bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreserveConflictingCopy
//
//  Immediately before committing, and whatever any watcher has or has not
//  reported. THIS IS THE GUARANTEE AND NOTIFICATION IS ONLY PROMPTNESS: a
//  missed notification, an unwatchable share, a change that arrived while
//  the machine was busy -- all of them end here, one stat before the write.
//
//  A BAY THAT NEVER RECORDED AN IDENTITY IS NOT CHECKED. MountFromBytes has
//  no file behind it, and a test that redirects reads into memory has none
//  either; comparing against a stat that never ran would refuse writes that
//  nothing has endangered.
//
//  isPreserved is true when the guest's version went to a file of its own,
//  and FlushEntry then writes nothing over the original.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::PreserveConflictingCopy (Entry & entry, FlushMoment moment, bool & isPreserved, FlushOutcome & outOutcome)
{
    HRESULT        hr           = S_OK;
    HRESULT        hrKeep       = S_OK;
    bool           unchanged    = true;
    bool           keptExternal = false;
    bool           asked        = false;
    string         original;
    string         preservedPath;
    ImageIdentity  current;



    isPreserved = false;

    BAIL_OUT_IF (!entry.sharedState.GetIdentity().recorded, S_OK);

    current   = m_store.ReadIdentity (entry.path);
    unchanged = entry.sharedState.GetIdentity().Matches (current);

    //  THE SAME CONFLICT THE WATCHER FINDS, discovered at the other end.
    //  Notification failed, or the change landed while this write was
    //  already under way, and the guest's version is about to go over a
    //  version this store never took up.
    //
    //  THE FILE STAYS WITH WHOEVER CHANGED IT. The guest's version moves to
    //  a file of its own and the bay follows it; the original keeps what
    //  the other program wrote and is not touched here at all. The watcher
    //  path applies exactly this rule, so one collision produces the same
    //  two files whichever end found it. It did not use to: which version
    //  kept the original name came down to which side was quicker.
    BAIL_OUT_IF (unchanged, S_OK);

    original = entry.path;
    asked    = entry.sharedState.IsAskOutstanding();

    //  Under the name the question already showed, when there is a
    //  question. Reserving happens once, wherever it happens first.
    preservedPath = entry.sharedState.GetPreservedPath();

    hrKeep = SaveLoadedImage (entry, preservedPath);

    //  A preserve that did not happen stops the write. The image KEEPS
    //  ITS DIRTY BIT, which is the difference between refusing and
    //  losing: the guest's writes are still in memory and still
    //  flushable once there is somewhere to put them.
    //  STG_E_NOTCURRENT says precisely this -- the object changed since
    //  it was last read.
    keptExternal = SUCCEEDED (hrKeep);

    //  The name it tried is held whether or not the write landed, so a
    //  retry goes to the file the user was told about.
    entry.sharedState.SetPreservedPath (preservedPath);

    CBRFEx (keptExternal, STG_E_NOTCURRENT,
            ReportPreserveFailure (entry, moment, preservedPath, hrKeep, original));

    //  On disk under its own name, so the bay carries nothing unsaved.
    entry.sharedState.SetPreservedWritten (true);

    entry.image->SetSourceCrcMismatch (false);
    entry.image->ClearDirty();
    m_store.MarkStatusChanged();

    isPreserved = true;

    //  A QUESTION ALREADY ON SCREEN OWNS WHAT HAPPENS NEXT. Writing the
    //  copy is not optional -- the guest's work would be gone otherwise
    //  -- but moving the bay and reporting are the ANSWER's job. Doing
    //  them here settled the matter underneath an open dialog, which
    //  left the user being asked about a file the bay no longer had.
    //  The copy is on disk under the name that dialog is showing, so
    //  either answer still does what it says.
    BAIL_OUT_IF (asked, S_OK);

    hrKeep = m_store.m_reload.RepointBayToFile (entry.slot, entry.drive, preservedPath);
    IGNORE_RETURN_VALUE (hrKeep, S_OK);

    //  Composed from the path the bay HAD, since that is the file the
    //  message is about and the repoint above has already moved it. Before a
    //  mount, the mount reports the copy, in a text that depends on its end.
    if (moment == FlushMoment::Replacing)
    {
        outOutcome.movedFrom = original;
        outOutcome.movedTo   = preservedPath;
    }
    else if (m_store.m_reportSink)
    {
        m_store.m_reportSink (entry.slot, entry.drive,
                      ChangePrompt::ComposeConflictReport (original, entry.drive, preservedPath));
    }

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
//  RecordCommittedFile
//
//  After a save lands. The file now carries a freshly computed CRC that
//  matches it, so the mismatch is no longer true of what is on disk -- and
//  the warning must not repeat on every later eject or power cycle.
//
//  The file this store just wrote is a change to it, and without the
//  identity refresh the next flush would find its own commit sitting where
//  the mount-time identity used to be and refuse itself. NOT SEPARABLE FROM
//  PreserveConflictingCopy's CHECK: the two are one mechanism, and the first
//  one alone breaks the second flush of every ordinary session.
//
//  Only for a bay that had an identity to begin with, so a mount from bytes
//  does not acquire one by being flushed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreSave::RecordCommittedFile (Entry & entry, const vector<Byte> & savedBytes)
{
    entry.image->SetSourceCrcMismatch (false);
    entry.image->MarkSaved (savedBytes);
    entry.sharedState.ClearRecovery();
    m_store.MarkStatusChanged();

    if (entry.sharedState.GetIdentity().recorded)
    {
        entry.sharedState.SetIdentity (m_store.ReadIdentity (entry.path));
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
    std::error_code  ec;



    if (m_store.m_fileIo != nullptr)
    {
        return m_store.m_fileIo->Exists (path);
    }

    return fs::exists (fs::path (path), ec);
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
    HRESULT  hr      = S_OK;
    string   stamp   = PreservedCopy::MakeStamp (m_store.m_timestamp ? m_store.m_timestamp() : time (nullptr));
    bool     isFree  = false;
    int      attempt = 0;



    for (attempt = 0; attempt < PreservedCopy::kMaxAttempts; attempt++)
    {
        string  candidate = PreservedCopy::MakePath (imagePath, stamp, attempt);

        if (!DoesPathExist (candidate))
        {
            outPath = candidate;
            isFree  = true;
            break;
        }
    }

    //  Ninety-nine names taken in one second is not a collision any more, it is
    //  something else going wrong, and inventing a hundredth would not help.
    CBR (isFree);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::CommitFile
//
//  Every write this class makes: an image, a salvaged copy, a preserved copy.
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
    HRESULT               hr       = S_OK;
    Win32DiskFileIo       localIo;
    IDiskFileIo         & fileIo   = (m_store.m_fileIo != nullptr) ? *m_store.m_fileIo : localIo;
    CommitPlan::Progress  progress = {};



    if (m_store.m_flushSink)
    {
        hr = m_store.m_flushSink (path, bytes);
        CHR (hr);
    }
    else
    {
        hr = DurableCommit::Commit (fileIo, path, bytes, DiskImageStore::GetInvocationTag(), mode, progress);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreSave::SaveLoadedImage
//
//  Writes what the bay is holding to a file of its own.
//
//  SERIALIZED FROM THE MOUNTED IMAGE, NOT COPIED FROM THE FILE. The entire
//  reason this version needs preserving is that the file no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreSave::SaveLoadedImage (Entry & entry, string & outPath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;



    CBR (entry.image != nullptr);

    hr = entry.image->Serialize (bytes);
    CHR (hr);

    //  A name already reserved is used as it is. Choosing a fresh one here is
    //  what let a question and a flush disagree about where the copy went.
    if (outPath.empty())
    {
        hr = FindFreePreservedPath (entry.path, outPath);
        CHR (hr);
    }

    hr = CommitFile (outPath, bytes, CommitMode::Replace);
    CHR (hr);

    entry.image->MarkSaved (bytes);

Error:
    //  outPath IS LEFT AS THE PATH THAT WAS TRIED, even on failure. The notice
    //  raised for a failed copy prints where it tried to write, and clearing it
    //  here left that notice with nothing to show but an error code.
    return hr;
}




