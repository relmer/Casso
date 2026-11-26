#include "Pch.h"

#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/Store/DiskStoreMessages.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFlusher::ReportPreserveFailure
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
//  ONLY A REFUSED OR IMPOSSIBLE RESCUE IS ANNOUNCED. Saying "your writes are
//  still in memory" is true while the machine runs and a lie as the drive
//  empties, so the sentence the user gets says which of the two happened.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFlusher::ReportPreserveFailure (DiskBay & entry, FlushMoment moment,
                                            const string & attemptedPath, HRESULT hrKeep,
                                            const string & original)
{
    SaveFailureCause  cause = (moment == FlushMoment::Ejecting)
                                  ? SaveFailureCause::Ejecting
                                  : SaveFailureCause::ExternalChange;



    if (moment == FlushMoment::ShuttingDown)
    {
        //  The last chance there is. A copy written here is written by this
        //  call rather than by an answer that will never arrive.
        if (RescueOnTheWayOut (entry, original))
        {
            return;
        }

        EhmNotifyUser (DiskStoreMessages::FormatDiscardedWritesMessage (original, attemptedPath, hrKeep).c_str());

        return;
    }

    if (!m_hooks.askSink || entry.sharedState.IsAskOutstanding())
    {
        //  Nothing can ask. An eject still empties the bay, so it gets the
        //  sentence that says the writes are going rather than the one that
        //  says they are waiting.
        if (moment == FlushMoment::Ejecting)
        {
            EhmNotifyUser (DiskStoreMessages::FormatDiscardedWritesMessage (original, attemptedPath,
                                                         hrKeep).c_str());
        }
        else
        {
            EhmNotifyUser (DiskStoreMessages::FormatExternalChangeMessage (original).c_str());
        }

        return;
    }

    entry.sharedState.SetAskedAction (ChangeAction::Conflict);

    entry.sharedState.SetAskOutstanding (
        m_hooks.askSink (entry.slot, entry.drive,
                   ChangePrompt::ComposeSaveFailure (original, entry.drive, attemptedPath,
                                                     hrKeep, cause)));

    //  THE EJECT WAITS ON THE ANSWER, but only when the question actually went
    //  up. One that could not be delivered must not leave a disk nobody can get
    //  out of the drive.
    if (moment == FlushMoment::Ejecting && entry.sharedState.IsAskOutstanding())
    {
        entry.sharedState.SetEjectWhenAnswered (true);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFlusher::RescueOnTheWayOut
//
//  Asks where to put a disk that is about to go, and puts it there.
//
//  SYNCHRONOUS, BECAUSE IT IS THE LAST THING THAT RUNS. No answer is coming
//  back later at this point -- the pump has gone and so has the thread that
//  would act on one -- so the asking and the writing both happen here, inside
//  the call that found the problem.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFlusher::RescueOnTheWayOut (DiskBay & entry, const string & original)
{
    HRESULT       hr = S_OK;
    string        chosen;
    vector<Byte>  held;



    if (!m_hooks.rescueSink)
    {
        return false;
    }

    if (!m_hooks.rescueSink (original, chosen) || chosen.empty())
    {
        return false;
    }

    hr = entry.image->Serialize (held);

    if (SUCCEEDED (hr))
    {
        hr = m_files.WritePreserved (chosen, held);
    }

    if (FAILED (hr))
    {
        return false;
    }

    entry.image->ClearDirty();
    entry.image->NoteSaved();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry
//
//  Centralized flush helper. Dispatches through SetFlushSink when the test
//  hook is installed; otherwise writes to the host filesystem. Does nothing
//  if the image is clean or no source path is recorded. A genuine failure
//  to persist a dirty image is surfaced to the user via the shared EHM
//  notifier (CHRN/CBRN), because every caller drops the return value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::FlushEntry (DiskBay & entry, FlushMoment moment)
{
    HRESULT        hr           = S_OK;
    HRESULT        hrRecovery   = S_OK;
    bool           unchanged    = true;
    bool           keptExternal = false;
    string         recoveryPath;
    string         preservedPath;
    vector<Byte>   bytes;
    vector<Byte>   external;
    ImageIdentity  current;



    // No-op cases -- nothing dirty to persist -- succeed silently. There is
    // deliberately no way to force a flush past these gates: the only caller
    // that wanted one was changing a write-protect flag, and rebuilding a
    // whole image to carry one bit is what SetImageWriteProtect exists to
    // avoid. An API that cannot be asked to do that cannot be misused into it.
    //  A replay recomputes a past the host already saw; nothing it does
    //  reaches a file.
    BAIL_OUT_IF (m_bays.IsReplaying(), S_OK);
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);

    // A write still open on a flux track has not reached the image yet, so
    // the image is not dirty until it is committed.
    entry.image->CommitPendingWrite();

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
    //  Immediately before committing, and whatever any watcher has or has not
    //  reported. THIS IS THE GUARANTEE AND NOTIFICATION IS ONLY PROMPTNESS: a
    //  missed notification, an unwatchable share, a change that arrived while
    //  the machine was busy -- all of them end here, one stat before the write.
    //
    //  A BAY THAT NEVER RECORDED AN IDENTITY IS NOT CHECKED. MountFromBytes has
    //  no file behind it, and a test that redirects reads into memory has none
    //  either; comparing against a stat that never ran would refuse writes that
    //  nothing has endangered.
    if (entry.sharedState.GetIdentity().recorded)
    {
        current   = m_files.ReadIdentity (entry.path);
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
        if (!unchanged)
        {
            string   original = entry.path;
            bool     asked    = entry.sharedState.IsAskOutstanding();
            HRESULT  hrKeep   = S_OK;

            //  Under the name the question already showed, when there is a
            //  question. Reserving happens once, wherever it happens first.
            preservedPath = entry.sharedState.GetPreservedPath();

            hrKeep = m_files.SaveLoadedImage (entry, preservedPath);

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

            //  A QUESTION ALREADY ON SCREEN OWNS WHAT HAPPENS NEXT. Writing the
            //  copy is not optional -- the guest's work would be gone otherwise
            //  -- but moving the bay and reporting are the ANSWER's job. Doing
            //  them here settled the matter underneath an open dialog, which
            //  left the user being asked about a file the bay no longer had.
            //  The copy is on disk under the name that dialog is showing, so
            //  either answer still does what it says.
            if (asked)
            {
                BAIL_OUT_IF (true, S_OK);
            }

            hrKeep = m_repoint.RepointBayToFile (entry.slot, entry.drive, preservedPath);
            IGNORE_RETURN_VALUE (hrKeep, S_OK);

            //  Composed from the path the bay HAD, since that is the file the
            //  message is about and the repoint above has already moved it.
            if (m_hooks.reportSink)
            {
                m_hooks.reportSink (entry.slot, entry.drive,
                              ChangePrompt::ComposeConflictReport (original, entry.drive,
                                                                  preservedPath));
            }

            //  Nothing is written over the original. It is not this bay's file
            //  any more.
            BAIL_OUT_IF (true, S_OK);
        }
    }

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        hrRecovery = m_files.TryWriteRecoveryImage (entry, recoveryPath);
        IGNORE_RETURN_VALUE (hrRecovery, S_OK);
    }

    CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());

    if (m_hooks.flushSink)
    {
        hr = m_hooks.flushSink (entry.path, bytes);
        CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());
    }
    else if (!entry.path.empty())
    {
        // Never write in place: a flush that fails midway would otherwise
        // have already truncated the user's image, trading a stale file for
        // no file at all. The dirty bit survives a failure, so a later flush
        // retries.
        //
        // The failure message names the recovery copy written above, because
        // a flush that cannot land is exactly when the session's only
        // lossless copy of the disk is the one sitting beside the original.
        hr = DiskFileCommit::WriteFileAtomically (entry.path, bytes);
        CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());
    }

    // The file now carries a freshly computed CRC that matches it, so the
    // mismatch is no longer true of what is on disk -- and the warning above
    // must not repeat on every later eject or power cycle.
    entry.image->SetSourceCrcMismatch (false);
    entry.image->ClearDirty();
    entry.image->NoteSaved();

    //  The file this store just wrote is a change to it, and without this the
    //  next flush would find its own commit sitting where the mount-time
    //  identity used to be and refuse itself. NOT SEPARABLE FROM THE CHECK
    //  ABOVE: the two are one mechanism, and the first one alone breaks the
    //  second flush of every ordinary session.
    //
    //  Only for a bay that had an identity to begin with, so a mount from bytes
    //  does not acquire one by being flushed.
    if (entry.sharedState.GetIdentity().recorded)
    {
        entry.sharedState.SetIdentity (m_files.ReadIdentity (entry.path));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush / FlushAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::Flush (int slot, int drive)
{
    HRESULT   hr = S_OK;



    CBRAEx (slot >= 0 && slot < DiskBayTable::kSlotCount && drive >= 0 && drive < DiskBayTable::kDriveCount, E_INVALIDARG);

    hr = FlushEntry (m_bays.GetBay (slot, drive), FlushMoment::Running);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetImageWriteProtect
//
//  Change a mounted WOZ's write-protect flag in its backing file, by patching
//  the one byte that holds it. The flag lives inside the file, so the change
//  has to be written; the question is how much of the file gets rewritten to
//  carry it. The answer here is one byte plus the header CRC.
//
//  The path this replaces sent the flag through DiskImage::Serialize, the full
//  rebuild-from-model writer. Everything that writer could not reproduce was
//  lost on a menu click -- no guest write, no emulation, just a click -- and
//  it fired in both directions, so un-protecting a preservation dump before
//  writing to it rewrote it too. Now the guarantee does not depend on the
//  writer reproducing every field: the bytes are never parsed, so they cannot
//  be damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::SetImageWriteProtect (int slot, int drive, bool writeProtected)
{
    HRESULT       hr        = S_OK;
    bool          bayOk     = false;
    bool          isWoz     = false;
    bool          hasPath   = false;
    bool          hasImage  = false;
    bool          isDamaged = false;
    vector<Byte>  bytes;



    bayOk = DiskBayTable::IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        isWoz = (entry.format == DiskFormat::Woz);
        CBRAEx (isWoz, E_INVALIDARG);

        hasPath = !entry.path.empty();
        CBR (hasPath);

        // A damaged image is refused outright. Patching the flag byte
        // recomputes the header checksum, and that checksum failing to match
        // IS the damage report -- so the one write that is otherwise harmless
        // is the one write that would destroy the evidence.
        isDamaged = entry.image->IsDamaged();
        CBRN (!isDamaged, DiskStoreMessages::FormatDamagedImageMessage (entry.path, entry.image->HasDamagedTracks()).c_str());

        // Guest writes go out FIRST, while the image still accepts a flush.
        // Patching the flag byte afterwards edits a file that already holds
        // them; doing it the other way round would strand them behind the
        // gate this call is about to close.
        hr = FlushEntry (entry, FlushMoment::Running);
        CHR (hr);

        hr = m_files.ReadImageFile (entry.path, bytes);
        CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr).c_str());

        hr = WozLoader::SetWriteProtectFlag (bytes, writeProtected);
        CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr).c_str());

        if (m_hooks.flushSink)
        {
            hr = m_hooks.flushSink (entry.path, bytes);
        }
        else
        {
            hr = DiskFileCommit::WriteFileAtomically (entry.path, bytes);
        }

        CHRN (hr, DiskStoreMessages::FormatFlushLossMessage (entry.path, hr).c_str());

        // The live image follows the file, and only once the file has
        // actually changed -- so a failed write leaves the two agreeing.
        entry.image->SetImageWriteProtected (writeProtected);

        //  THIS STORE JUST WROTE THE FILE, so the identity it watches has to
        //  move with it, exactly as FlushEntry does after its own commit.
        //  Without this the next poll finds bytes that do not match the
        //  mount-time identity and reports the disk as changed outside Casso,
        //  over a change the user made from the Disk menu.
        //
        //  Only for a bay that recorded one, so a mount from bytes does not
        //  acquire an identity by being write-protected.
        if (entry.sharedState.GetIdentity().recorded)
        {
            entry.sharedState.SetIdentity (m_files.ReadIdentity (entry.path));
        }

        m_bays.NotifyMediaChanged();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushAll / FlushAllForShutdown
//
//  MOST FLUSHES HAPPEN WITH THE MACHINE RUNNING. A drive spinning down, a
//  machine switch and a soft reset all keep every bay and every pump, so a
//  failure among them is a question like any other. Only the last flush of the
//  process is different, which is why it goes through a call of its own.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::FlushAll()
{
    return FlushEveryBay (FlushMoment::Running);
}



HRESULT DiskFlusher::FlushAllForShutdown()
{
    return FlushEveryBay (FlushMoment::ShuttingDown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEveryBay
//
//  What both of the above are, once the moment is decided.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::FlushEveryBay (FlushMoment moment)
{
    HRESULT   hr      = S_OK;
    HRESULT   hrFirst = S_OK;
    int       slot    = 0;
    int       drive   = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            hr = FlushEntry (m_bays.GetBay (slot, drive), moment);

            if (FAILED (hr) && SUCCEEDED (hrFirst))
            {
                hrFirst = hr;
            }
        }
    }

    return hrFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushAllUnlessHeld
//
//  The motor spinning down, a reset and a power cycle flush through here.
//  While the machine is behind live in its recorded history, or a replay is
//  running, the guest's writes are machine state that a step can still
//  change, so these moments leave the file alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFlusher::FlushAllUnlessHeld()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_bays.IsFlushHeld() || m_bays.IsReplaying(), S_OK);

    hr = FlushAll();
    CHR (hr);

Error:
    return hr;
}
