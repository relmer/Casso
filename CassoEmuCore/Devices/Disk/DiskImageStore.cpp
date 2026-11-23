#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "DiskImageStore.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Devices/Disk/Store/DiskStoreMessages.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "DiskCheck/ThumbnailStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  Test-friendly mount path that bypasses the host filesystem. The
//  virtualPath is used purely as the round-trip identifier; production
//  code uses Mount() which reads from disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    MountDiagnosis  ignored;



    return MountFromBytes (slot, drive, virtualPath, fmt, bytes, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  The same mount, saying why it refused. A load that fails is classified from
//  the bytes it was given, which is where the answer is: the store knows how
//  many bytes arrived and which container the name promised, and the WOZ
//  loader knows whether its header was ever there.
//
//  A mount that succeeds calls the bay-change sink with Inserted, as Mount
//  does, so a controller attached by that sink reads the new image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT   hr = S_OK;



    hr = LoadIntoBay (slot, drive, virtualPath, fmt, bytes, outDiagnosis);
    CHR (hr);

    EmitBayChange (slot, drive, BayChange::Inserted);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadIntoBay
//
//  Replaces a bay's image with one loaded from bytes, flushing the old one
//  first. When the loader cannot load the bytes, the bay ends empty.
//
//  THE OLD IMAGE IS FREED BEFORE THE LOAD, so a bay that had a disk and ends
//  empty calls the bay-change sink with Ejected. Without that call, a
//  controller attached to the old image reads freed memory after a failed
//  remount.
//
//  A WOZ image of any disk but a 5.25-inch one is refused before the bay is
//  touched, as a file in another bay is: the bay keeps its disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::LoadIntoBay (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT                 hr         = S_OK;
    bool                    wasMounted = false;
    bool                    is525      = true;
    WozLoader::Description  about;



    outDiagnosis              = MountDiagnosis();
    outDiagnosis.format       = fmt;
    outDiagnosis.fileByteSize = bytes.size();

    CBRAEx (slot >= 0 && slot < DiskBayTable::kSlotCount && drive >= 0 && drive < DiskBayTable::kDriveCount, E_INVALIDARG);

    if (fmt == DiskFormat::Woz)
    {
        WozLoader::Describe (bytes, about);

        //  A file that isn't a WOZ at all fails in the loader, with its reason.
        is525 = !about.isWoz || about.diskType == WozLoader::kDiskType525;
    }

    CBRFEx (is525, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT), outDiagnosis.failure = MountFailure::WozNot525);

    {
        DiskBay &   entry = m_bays.GetBay (slot, drive);

        wasMounted = entry.mounted;

        //  The watch is keyed by the old image's directory, so it is dropped
        //  before the path changes; Mount watches the new one on success.
        if (wasMounted)
        {
            hr = FlushEntry (entry, FlushMoment::Running);
            IGNORE_RETURN_VALUE (hr, S_OK);

            EndWatching (slot, drive);
        }

        entry.image   = make_unique<DiskImage> ();
        entry.path    = virtualPath;
        entry.format  = fmt;
        entry.mounted = true;

        entry.image->LoadFromBytes (fmt, bytes, virtualPath);

        // A format the loader rejects leaves the slot empty rather than
        // half-mounted.
        if (!entry.image->IsLoaded())
        {
            entry.image.reset();
            entry.path.clear();
            entry.mounted        = false;
            entry.salvageOffered = false;
            outDiagnosis         = DiskSourceFormat::ClassifyLoadFailure (fmt, bytes);
            hr = E_FAIL;

            if (wasMounted)
            {
                entry.sharedState.Eject();
                EmitBayChange (slot, drive, BayChange::Ejected);
            }
        }
        else
        {
            // Decide the salvage question once, here, where the cost is paid
            // by a mount the user already asked for. Undamaged images settle
            // it for free; a damaged one is decoded once instead of on every
            // menu draw.
            SalvageAssessment  assessment;
            HRESULT            hrAssess = m_salvager.AssessSalvage (slot, drive, assessment);

            entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  Production mount path. Reads the file from the host filesystem then
//  routes through the appropriate loader by extension.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path)
{
    MountDiagnosis  ignored;



    return Mount (slot, drive, path, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReportPreserveFailure
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

void DiskImageStore::ReportPreserveFailure (DiskBay & entry, FlushMoment moment,
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
//  DiskImageStore::RescueOnTheWayOut
//
//  Asks where to put a disk that is about to go, and puts it there.
//
//  SYNCHRONOUS, BECAUSE IT IS THE LAST THING THAT RUNS. No answer is coming
//  back later at this point -- the pump has gone and so has the thread that
//  would act on one -- so the asking and the writing both happen here, inside
//  the call that found the problem.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::RescueOnTheWayOut (DiskBay & entry, const string & original)
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
//  Mount
//
//  The same mount, saying why it refused. The two failures BEFORE any loader
//  runs are settled here and nowhere else: a name no loader claims, and bytes
//  that never arrived. Neither is visible further in -- the loaders are handed
//  a buffer and a format, and by then the file name and the read are history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path,
                               MountDiagnosis & outDiagnosis)
{
    HRESULT       hr        = S_OK;
    DiskFormat    fmt       = DiskFormat::Dsk;
    bool          elsewhere = false;
    vector<Byte>  bytes;



    outDiagnosis = MountDiagnosis();

    //  ONE FILE, ONE DRIVE, and refused before a byte is read -- the bytes are
    //  not what is wrong with it. Two bays on one file each hold their own
    //  DiskImage, and a flush writes the whole image, so from the guest's
    //  first write each drive overwrites whatever the other saved. One
    //  external change then raises the conflict twice, writes two rescue
    //  copies and puts two dialogs up in a row.
    //
    //  THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THIS. Re-mounting is how
    //  a machine switch and a reload put the disk back, so the bay being
    //  mounted into is the one bay this does not look at.
    elsewhere = m_bays.IsFileInAnotherBay (path, slot, drive, outDiagnosis.occupiedDrive);

    CBRFEx (!elsewhere, E_INVALIDARG,
            outDiagnosis.failure = MountFailure::AlreadyMounted);

    hr = DiskSourceFormat::GetSourceFormatByExtension (path, fmt);
    CHRF (hr, outDiagnosis.failure = MountFailure::UnknownExtension);

    outDiagnosis.format = fmt;

    hr = m_files.ReadImageFile (path, bytes);
    CHRF (hr, outDiagnosis.failure = MountFailure::FileUnreadable);

    hr = LoadIntoBay (slot, drive, path, fmt, bytes, outDiagnosis);
    CHR (hr);

    //  Recorded AFTER the read and only on success, matching the command
    //  line's read-then-stamp order. A stamp taken before the read could
    //  describe a file the loaded bytes did not come from, and a stamp
    //  recorded for a mount that failed would sit on an empty bay.
    m_bays.GetBay (slot, drive).sharedState.Mount (m_files.ReadIdentity (path));

    //  Mount registers the watch. It is orchestration rather than platform
    //  work, so it lives here and not in whatever built the watcher.
    BeginWatching (slot, drive);

    //  A disk went in. The shell wires the controller and lights the door from
    //  here rather than from the path that called Mount, so a command-line
    //  mount and a picker mount and a machine-switch remount all react alike.
    EmitBayChange (slot, drive, BayChange::Inserted);

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
//  notifier (CHRN/CBRN), because every caller drops the return value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEntry (DiskBay & entry, FlushMoment moment)
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

            hrKeep = RepointBayToFile (entry.slot, entry.drive, preservedPath);
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

HRESULT DiskImageStore::Flush (int slot, int drive)
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

HRESULT DiskImageStore::SetImageWriteProtect (int slot, int drive, bool writeProtected)
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
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsSalvageOffered (int slot, int drive) const
{
    if (!DiskBayTable::IsValidBay (slot, drive))
    {
        return false;
    }

    return m_bays.GetBay (slot, drive).salvageOffered;
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

HRESULT DiskImageStore::FlushAll()
{
    return FlushEveryBay (FlushMoment::Running);
}



HRESULT DiskImageStore::FlushAllForShutdown()
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

HRESULT DiskImageStore::FlushEveryBay (FlushMoment moment)
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
//  Eject
//
//  Auto-flush dirty before releasing. Honors FR-025.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive)
{
    HRESULT   hr = S_OK;



    // An out-of-range bay and an empty one are both nothing to eject.
    if (DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).mounted)
    {
        DiskBay &   entry = m_bays.GetBay (slot, drive);

        // Flush failures are reported to the user by FlushEntry itself; the
        // eject proceeds either way, because refusing to unmount would leave
        // the user with no way to get the disk out.
        hr = FlushEntry (entry, FlushMoment::Ejecting);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  EXCEPT WHEN THE FLUSH PUT A QUESTION UP, which it does only when
        //  the guest's version has nowhere to go and this eject is what would
        //  destroy it. The disk stays until the answer comes, and the answer
        //  finishes what this call started -- both of the answers it offers
        //  end with the drive empty, so nothing is stuck.
        if (entry.sharedState.IsEjectWhenAnswered())
        {
            return;
        }

        //  Before the path goes: the watch is keyed by the directory the
        //  image sits in, and there is no finding that from a cleared path.
        EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted = false;
        entry.sharedState.Eject();

        //  The disk left. Emitted after the bay is empty, so the handler that
        //  detaches the controller sees the post-eject state.
        EmitBayChange (slot, drive, BayChange::Ejected);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  FR-034 / Phase 4 contract: keep mounts mounted, flush every dirty
//  image so a soft reset never loses user writes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SoftReset()
{
    HRESULT   hr = FlushAll();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  FR-035 / data-model.md: unmount everything, flushing dirty as we go.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PowerCycle()
{
    int   slot  = 0;
    int   drive = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            Eject (slot, drive);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetImage / IsMounted / GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * DiskImageStore::GetImage (int slot, int drive)
{
    // Null for a bad bay is the same answer as for an empty one: no image.
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMediaId
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskImageStore::GetMediaId (int slot, int drive) const
{
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).mediaId : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteStateRestored
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteStateRestored (int slot, int drive)
{
    if (DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).image != nullptr)
    {
        m_bays.GetBay (slot, drive).mediaId = m_bays.TakeNextMediaId();
        EmitBayChange (slot, drive, BayChange::Restored);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMounted
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMounted (int slot, int drive) const
{
    return DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).mounted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

const string & DiskImageStore::GetSourcePath (int slot, int drive) const
{
    // Returns a reference, so a bad bay yields the member empty string rather
    // than a temporary.
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).path : m_emptyPath;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMountedSourcePaths
//
//  Every mounted entry's backing path with its bay. Entries mounted from
//  bytes with an empty virtual path are skipped -- there is no host file to
//  collide with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskImageStore::MountedSource> DiskImageStore::GetMountedSourcePaths() const
{
    std::vector<MountedSource>  result;
    int                         slot   = 0;
    int                         drive  = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            const DiskBay &  entry = m_bays.GetBay (slot, drive);

            if (entry.mounted && !entry.path.empty())
            {
                result.push_back (MountedSource{ entry.path, slot, drive });
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState
//
//  What a bay knows about its image beyond the bytes.
//
//  NULL FOR AN OUT-OF-RANGE BAY rather than a reference to a shared empty, so a
//  caller cannot write into a placeholder and believe it recorded something.
//
////////////////////////////////////////////////////////////////////////////////

MountedImageState * DiskImageStore::GetSharedState (int slot, int drive)
{
    MountedImageState *  state = nullptr;



    if (DiskBayTable::IsValidBay (slot, drive))
    {
        state = &m_bays.GetBay (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState  (const)
//
//  The same answer for a caller that only reads it.
//
////////////////////////////////////////////////////////////////////////////////

const MountedImageState * DiskImageStore::GetSharedState (int slot, int drive) const
{
    const MountedImageState *  state = nullptr;



    if (DiskBayTable::IsValidBay (slot, drive))
    {
        state = &m_bays.GetBay (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetNowMs
//
//  Milliseconds now.
//
//  INJECTED SO A TEST NEED NOT WAIT. The quiet period is a second, and a suite
//  that spent one proving every coalescing rule would be a suite nobody runs.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DiskImageStore::GetNowMs() const
{
    int64_t  now = 0;



    if (m_hooks.clock)
    {
        now = m_hooks.clock();
    }
    else
    {
        now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                  std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    return now;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EmitBayChange
//
//  Tells the shell a bay's disk changed.
//
//  EVERY MUTATION PATH ENDS HERE. Mount, user eject, pick-up, lost file -- the
//  door and its sounds are lit from this one call, so a new path that moves a
//  disk cannot forget to. A null sink is a headless or test session with no
//  drive on screen, and does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EmitBayChange (int slot, int drive, BayChange change)
{
    if (slot >= 0 && slot < DiskBayTable::kSlotCount && drive >= 0 && drive < DiskBayTable::kDriveCount)
    {
        m_bays.GetBay (slot, drive).mediaId = m_bays.TakeNextMediaId();
    }

    if (m_hooks.bayChangeSink)
    {
        m_hooks.bayChangeSink (slot, drive, change);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::NoteExternalChange
//
//  Somebody says this image changed.
//
//  IT RECORDS AND RETURNS. The watcher calls this from its own thread and the
//  message channel from the UI thread, while reading an image and swapping it
//  under a running guest belongs to the thread that owns disk writes. Doing the
//  work here would race the emulator mid-sector.
//
//  A PATH NO BAY HOLDS IS NOT AN ERROR. A directory watch reports every file
//  under it, and an intent may be stated for an image nothing has mounted; both
//  are ordinary and both end here, silently.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteExternalChange (const string & path, ExternalChangeIntent intent, bool fromCassoExplorer)
{
    std::lock_guard<std::mutex>  held (m_bays.GetPendingMutex());
    int64_t  now   = GetNowMs();
    int      slot  = 0;
    int      drive = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            DiskBay &  entry = m_bays.GetBay (slot, drive);

            if (entry.mounted && MountedImageState::IsSamePath (entry.path, path))
            {
                entry.sharedState.NoteChange (now, intent, fromCassoExplorer);
            }
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ApplyPendingReload
//
//  Act on whatever has settled, on the thread that owns disk writes.
//
//  CALLED OFTEN AND DOES NOTHING ALMOST EVERY TIME. It is pumped from the
//  controller's idle callback and from motor spindown, so the common path is a
//  loop over sixteen bays that finds nothing pending and returns.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReload()
{
    int      readySlot[DiskBayTable::kSlotCount * DiskBayTable::kDriveCount]  = {};
    int      readyDrive[DiskBayTable::kSlotCount * DiskBayTable::kDriveCount] = {};
    int      readyCount                           = 0;
    int      slot                                 = 0;
    int      drive                                = 0;
    int      i                                    = 0;
    int64_t  now                                  = GetNowMs();



    //  ONE CLOCK READING AND ONE LOCK FOR THE WHOLE WALK, not one of each per
    //  bay. This runs sixty times a second forever, and asking the clock
    //  sixteen times and taking the lock sixteen times cost 695 ns a call --
    //  measured, and about a hundred times what the work itself is worth. The
    //  snapshot below brings it back to the handful of comparisons it should be.
    {
        std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

        for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
        {
            for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
            {
                const DiskBay &  entry = m_bays.GetBay (slot, drive);

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
    }

    //  ACTED ON OUTSIDE THE LOCK. Reloading an image reads a file and swaps a
    //  disk; holding the pending mutex across that would block the watcher
    //  thread for the length of a read.
    for (i = 0; i < readyCount; i++)
    {
        ApplyPendingReloadToBay (readySlot[i], readyDrive[i]);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ApplyPendingReloadToBay
//
//  One bay's settled change, from noticing it to acting on it.
//
//  THE READ HAPPENS BEFORE THE DECISION, and that ordering is what lets the
//  policy rank "cannot be used" above everything else: whether the new bytes
//  can be this disk is not knowable without reading them.
//
//  A CHANGE THAT TURNS OUT NOT TO BE ONE IS DROPPED. A directory watch reports
//  a write to any file in the folder and a stated intent is only a hint, so the
//  recorded identity is what settles whether anything actually happened. This is
//  also what stops the store's own commit from coming back as somebody else's
//  change.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReloadToBay (int slot, int drive)
{
    HRESULT                            hr        = S_OK;
    DiskBay                          & entry     = m_bays.GetBay (slot, drive);
    bool                               settled   = false;
    bool                               held      = false;
    bool                               usable    = false;
    bool                               unchanged = false;
    bool                               preserved = false;
    ExternalChangeIntent               intent    = ExternalChangeIntent::Unstated;
    bool                               explorer  = false;
    ChangeAuthor                       author    = ChangeAuthor::AnotherProgram;
    ChangeAction                       action    = ChangeAction::Ignore;
    ImageIdentity                      current;
    vector<Byte>                       bytes;
    DiskFormat                         fmt       = DiskFormat::Dsk;
    ExternalChangePolicy::Situation    situation;
    string                             savePath;



    {
        std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

        settled = entry.mounted
               && entry.image != nullptr
               && entry.sharedState.IsSettled (GetNowMs());
        intent   = entry.sharedState.GetPending().intent;
        explorer = entry.sharedState.GetPending().fromCassoExplorer;
    }

    if (!settled)
    {
        return;
    }

    //  WHILE A QUESTION STANDS THE USER OWNS THE BAY, and every later change
    //  waits behind their answer -- a stated intent included. Applying one
    //  underneath an open dialog swapped the disk out from under it, so
    //  "keep your current version" came to mean the other program's version.
    //
    //  NOTHING IS LOST BY WAITING. `NoteChange` keeps refreshing the pending
    //  record, and the answer re-reads the file when it arrives, so what
    //  lands is the newest bytes rather than whatever stood when the question
    //  went up. One consequence: a restart requested while a question is on
    //  screen reloads without restarting, because the answer consumes the
    //  record that carried the request.
    //
    //  IT ALSO STOPS THE READING. The rest of this function reads the whole
    //  image and trial-loads it before it reaches the point where an
    //  outstanding question is noticed, and the change stays pending by
    //  design for as long as the user reads -- sixty full reads and
    //  nibblizations a second until they answer.
    if (entry.sharedState.IsAskOutstanding())
    {
        return;
    }

    //  Deferred rather than refused, indefinitely and silently: the pick-up
    //  simply happens once the hold is released. Both writers in this system
    //  commit atomically, but a text editor or a copy tool need not, and the
    //  quiet period alone does not cover one that takes its time.
    if (m_hooks.fileIo != nullptr && m_hooks.fileIo->IsHeldByAnotherProcess (entry.path))
    {
        return;
    }

    current   = m_files.ReadIdentity (entry.path);
    unchanged = entry.sharedState.GetIdentity().Matches (current);

    if (unchanged)
    {
        std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

        entry.sharedState.ClearPending();

        return;
    }

    //  Read, and decide whether these bytes can be this disk. A trial load into
    //  a throwaway image is the only honest answer -- the extension and the
    //  length agree with plenty of files that will not load.
    hr = m_files.ReadImageFile (entry.path, bytes);

    if (SUCCEEDED (hr))
    {
        hr = DiskSourceFormat::GetSourceFormatByExtension (entry.path, fmt);
    }

    if (SUCCEEDED (hr))
    {
        DiskImage  trial;

        trial.LoadFromBytes (fmt, bytes, entry.path);
        usable = trial.IsLoaded();
    }

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
        savePath = entry.sharedState.GetPreservedPath();

        hr = m_files.SaveLoadedImage (entry, savePath);

        entry.sharedState.SetPreservedPath (savePath);

        if (FAILED (hr))
        {
            //  Nothing is mounted and nothing is overwritten. The pending
            //  change is DROPPED RATHER THAN RETRIED: retrying costs a full
            //  image read and a failed write on every idle tick, sixty times a
            //  second for as long as the folder stays full. The next change to
            //  the file, or the next flush, tries again.
            {
                std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

                entry.sharedState.ClearPending();
            }

            if (m_hooks.decisionSink)
            {
                m_hooks.decisionSink (entry.path, ChangeAction::Conflict, false, savePath);
            }

            //  ASKED RATHER THAN REPORTED, because "Save as..." is an answer
            //  and a notice has nowhere to put one. The bay is left with the
            //  question outstanding so a second failure does not stack a
            //  second dialog on the first.
            if (m_hooks.askSink && !entry.sharedState.IsAskOutstanding())
            {
                entry.sharedState.SetAskedAction (ChangeAction::Conflict);

                entry.sharedState.SetAskOutstanding (
                    m_hooks.askSink (slot, drive,
                               ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                                 savePath, hr,
                                                                 SaveFailureCause::ExternalChange)));
            }

            return;
        }

        //  It is on disk under its own name now, so the bay carries nothing
        //  unsaved and the change that follows is no longer a conflict.
        entry.sharedState.SetPreservedWritten (true);

        entry.image->ClearDirty();

        situation.guestDirty = false;
        action               = ExternalChangePolicy::Decide (situation);
        preserved            = true;
    }

    //  A file that is simply gone gets its own sentence rather than sharing
    //  one with a file that is present and unreadable.
    if (action == ChangeAction::Unusable && !m_files.DoesPathExist (entry.path))
    {
        action = ChangeAction::Deleted;
    }

    //  A question nothing can answer stays pending rather than resolving itself
    //  by default. A headless host has no way to ask, and picking one of the
    //  answers on the user's behalf is what the fallback exists for.
    //
    //  ONE QUESTION AT A TIME PER BAY. Asking cannot block -- the answer comes
    //  from another thread -- so the change is still pending on the next idle
    //  tick, and without this the user would be asked again sixty times a
    //  second while reading the first one.
    if (ExternalChangePolicy::NeedsAnAnswer (action))
    {
        if (m_hooks.askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskedAction (action);

            {
                HRESULT  hrName = S_OK;

                //  RESERVED, NOT JUST CALCULATED. The name shown here is the
                //  one the copy will take whoever writes it -- this bay's own
                //  flush may get there first, while this question is still on
                //  screen. Working it out again at that point produced a
                //  different name and left the dialog offering a file nobody
                //  ever created.
                string  reserved = entry.sharedState.GetPreservedPath();

                if (reserved.empty())
                {
                    hrName = m_files.FindFreePreservedPath (entry.path, reserved);
                    IGNORE_RETURN_VALUE (hrName, S_OK);

                    entry.sharedState.SetPreservedPath (reserved);
                }

                entry.sharedState.SetAskOutstanding (
                    m_hooks.askSink (slot, drive,
                               ChangePrompt::Compose (entry.path, drive, action,
                                                      reserved,
                                                      entry.sharedState.IsPreservedWritten())));
            }
        }

        return;
    }

    if (m_hooks.decisionSink)
    {
        m_hooks.decisionSink (entry.path, action, preserved, savePath);
    }

    //  THE INTENT THAT DECIDED IS THE INTENT THAT ATTRIBUTES. `intent` was read
    //  at the top of this function, and the record it came from is refreshed by
    //  every later write -- a read taken now would credit this reload to
    //  whoever wrote most recently rather than to whoever asked for it.
    author = (intent == ExternalChangeIntent::Unstated) ? ChangeAuthor::AnotherProgram
           : explorer                                   ? ChangeAuthor::CassoExplorer
                                                        : ChangeAuthor::CassoCli;

    CarryOutChangeAction (slot, drive, action, bytes, author);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ResolvePendingChange
//
//  The answer to a question this store asked.
//
//  THE BYTES ARE READ AGAIN HERE RATHER THAN KEPT FROM THE QUESTION. The user
//  may have run two more builds while the prompt sat on the screen, and the
//  version they mean is the one they just built -- not the one that was on disk
//  when the question was composed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ResolvePendingChange (int slot, int drive, ChangeAction chosen,
                                          const string & savePath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;



    if (!DiskBayTable::IsValidBay (slot, drive))
    {
        return;
    }

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

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

        //  "Save as..." after a copy could not be written where this store
        //  chose. The disk stays in the drive and the bay moves onto the file
        //  the user picked, which is the same outcome as keeping it -- only the
        //  folder is theirs rather than ours.
        if (entry.sharedState.GetAskedAction() == ChangeAction::Conflict)
        {
            bool  finishEject = entry.sharedState.IsEjectWhenAnswered();

            entry.sharedState.SetAskedAction (ChangeAction::Ignore);

            if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
            {
                vector<Byte>  held;

                hr = entry.image->Serialize (held);

                if (SUCCEEDED (hr))
                {
                    hr = m_files.WritePreserved (savePath, held);
                }

                if (FAILED (hr))
                {
                    //  Still nowhere to put it. The disk is untouched and the
                    //  question stands rather than being quietly dropped.
                    if (m_hooks.askSink)
                    {
                        entry.sharedState.SetAskOutstanding (
                            m_hooks.askSink (slot, drive,
                                       ChangePrompt::ComposeSaveFailure (
                                           entry.path, drive, savePath, hr,
                                           SaveFailureCause::ExternalChange)));
                    }

                    return;
                }

                entry.image->ClearDirty();
                entry.sharedState.SetPreservedPath (string());

                hr = RepointBayToFile (slot, drive, savePath);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
            else if (finishEject)
            {
                //  THE USER CHOSE TO LET IT GO, so the dirty bit goes with it.
                //  Left standing, the eject below would find the same collision
                //  and put the same question up again, and the disk they just
                //  discarded would be undiscardable.
                entry.image->ClearDirty();
            }

            {
                std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

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

                Eject (slot, drive);
            }

            return;
        }

        //  An answer about a file that has gone is being given to a question
        //  that no longer applies. Saving what is held and emptying the drive
        //  is the only thing still on offer.
        //
        //  THE SAVE HAPPENS BEFORE THE EJECT, always. The eject is what
        //  discards the in-memory disk, so a failure between the two must
        //  leave the disk in the drive rather than gone from both places.
        if (ExternalChangePolicy::IsFileLost (entry.sharedState.GetAskedAction()))
        {
            entry.sharedState.SetAskedAction (ChangeAction::Ignore);

            if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
            {
                vector<Byte>  held;

                hr = entry.image->Serialize (held);

                if (SUCCEEDED (hr))
                {
                    hr = m_files.WritePreserved (savePath, held);
                }

                //  A save the user asked for and did not get must not be
                //  followed by throwing the only copy away.
                if (FAILED (hr))
                {
                    if (m_hooks.askSink)
                    {
                        entry.sharedState.SetAskOutstanding (
                            m_hooks.askSink (slot, drive,
                                       ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                                         savePath, hr,
                                                                         SaveFailureCause::FileLost)));
                    }

                    return;
                }

                //  THE DISK STAYS IN THE DRIVE, ON THE FILE THE USER PICKED.
                //  Serialize writes a whole image, not a fragment, so the drive
                //  now has somewhere to live -- and emptying it would mean
                //  handing back a complete disk and then making them go and
                //  find it. Keeping a version during a conflict already works
                //  this way, and one act should not have two outcomes.
                entry.image->ClearDirty();

                hr = RepointBayToFile (slot, drive, savePath);
                IGNORE_RETURN_VALUE (hr, S_OK);

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

        hr = m_files.ReadImageFile (entry.path, bytes);

        //  Answering "take it up" about a file that has since gone is not the
        //  same question any more.
        if (FAILED (hr) && chosen != ChangeAction::Ignore)
        {
            chosen = ChangeAction::Deleted;
        }
    }

    //  THE USER DID THE INSERTING, so no program is named. A question is only
    //  ever put when nothing stated an intent -- that is what `Ask` means --
    //  and one arriving while the question stood on screen does not turn the
    //  answer into somebody else's reload.
    CarryOutChangeAction (slot, drive, chosen, bytes, ChangeAuthor::AnotherProgram);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::CarryOutChangeAction
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

void DiskImageStore::CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                           const vector<Byte> & bytes, ChangeAuthor author)
{
    HRESULT    hr            = S_OK;
    DiskBay  & entry         = m_bays.GetBay (slot, drive);
    bool       restarted     = false;
    bool       tookUp        = false;
    bool       preserved     = false;
    bool       preserveFail  = false;
    string     preservedPath;
    //  KEEPING MOVES THE BAY ONTO THE COPY, so entry.path is no longer the file
    //  any of these messages are about by the time they are composed. Captured
    //  before anything can move it.
    string   original     = entry.path;



    switch (action)
    {
    case ChangeAction::ReloadInPlace:
    case ChangeAction::Restart:
        hr = MountExternallyModifiedDisk (slot, drive, bytes);

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
            hr = m_files.SaveLoadedImage (entry, preservedPath);

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
        std::lock_guard<std::mutex>  guard (m_bays.GetPendingMutex());

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

    if (restarted && m_hooks.restartCallback)
    {
        m_hooks.restartCallback();
    }

    //  A COPY THAT COULD NOT BE WRITTEN IS A QUESTION, not a notice: it offers
    //  somewhere else to put the file, and a notice has nowhere to put an
    //  answer. It leaves the drive exactly as it was either way.
    if (preserveFail)
    {
        if (m_hooks.askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskOutstanding (true);
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            m_hooks.askSink (slot, drive,
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
    if (ExternalChangePolicy::IsFileLost (action) && m_hooks.askSink)
    {
        entry.sharedState.SetAskedAction (action);

        entry.sharedState.SetAskOutstanding (
            m_hooks.askSink (slot, drive, ChangePrompt::Compose (original, drive, action)));

        return;
    }

    //  EVERY TIME, NOT ONLY THE FIRST. The sink re-words a notice already up
    //  for this bay rather than raising a second, so absorbing further changes
    //  costs nothing -- and reporting only the first left the bar saying
    //  something that had stopped being true.
    if (m_hooks.reportSink)
    {
        ChangePrompt  report;

        //  THE RELOAD IS REPORTED AHEAD OF PRESERVING, because a conflict that
        //  reloaded did both and the reload is the headline; the copy is a
        //  clause on the end of it.
        if (tookUp)
        {
            report = ChangePrompt::ComposeReloadReport (original, drive, restarted,
                                                        m_hooks.machineName, author,
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
            m_hooks.reportSink (slot, drive, report);
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountExternallyModifiedDisk
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
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes)
{
    HRESULT                  hr         = S_OK;
    DiskBay                & entry      = m_bays.GetBay (slot, drive);
    SalvageAssessment        assessment;
    HRESULT                  hrAssess   = S_OK;
    bool                     usable     = false;
    unique_ptr<DiskImage>    loaded     = make_unique<DiskImage> ();



    loaded->LoadFromBytes (entry.format, bytes, entry.path);

    usable = loaded->IsLoaded();
    CBR (usable);

    //  THE CONTENTS MOVE, THE OBJECT STAYS. The controller holds a raw pointer
    //  to this DiskImage -- SetExternalDisk hands one over at mount -- so
    //  replacing the unique_ptr would leave the drive reading freed memory the
    //  instant a disk was reloaded. Assigning through keeps the address the
    //  drive was given.
    *entry.image = std::move (*loaded);

    //  The identity is refreshed from the file the bytes came from, so the
    //  swap does not immediately look like another external change.
    entry.sharedState.SetIdentity (m_files.ReadIdentity (entry.path));
    entry.mediaId = m_bays.TakeNextMediaId();

    hrAssess             = m_salvager.AssessSalvage (slot, drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    //  The disk in the drive was replaced under the running machine. Swapped,
    //  not Inserted: the door opens and closes rather than only closing,
    //  because from the user's seat a disk came out and another went in.
    EmitBayChange (slot, drive, BayChange::Swapped);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::RepointBayToFile
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

HRESULT DiskImageStore::RepointBayToFile (int slot, int drive, const string & newPath)
{
    HRESULT  hr     = S_OK;
    bool     usable = DiskBayTable::IsValidBay (slot, drive) && !newPath.empty();



    CBR (usable);

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        CBR (entry.mounted && entry.image != nullptr);

        //  Before the path moves, while EndWatching can still tell which
        //  directory this bay was using.
        EndWatching (slot, drive);

        entry.path = newPath;

        //  A fresh identity for the new file, and nothing pending against the
        //  old one. Mount is what records both.
        entry.sharedState.Mount (m_files.ReadIdentity (newPath));

        BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::BeginWatching
//
//  Starts a watch on the directory holding a bay's image.
//
//  A DIRECTORY THAT CANNOT BE WATCHED IS RECORDED, NOT REPORTED. A network
//  share or a synchronizing folder is exactly the case that produces one, and
//  the guarantee does not live here: the check made before every write is what
//  carries it, and notification only makes it prompt.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::BeginWatching (int slot, int drive)
{
    DiskBay  & entry     = m_bays.GetBay (slot, drive);
    string     directory = MountedImageState::GetDirectory (entry.path);
    bool       watching  = false;



    if (m_hooks.watcher != nullptr && !directory.empty())
    {
        watching = m_hooks.watcher->Watch (directory,
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
//  DiskImageStore::EndWatching
//
//  Drops a bay's watch, unless another bay still needs the same directory.
//
//  TWO DISKS OUT OF ONE FOLDER IS THE ORDINARY CASE -- a boot disk and a work
//  disk built by the same script -- and ejecting the first must not blind the
//  second.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EndWatching (int slot, int drive)
{
    DiskBay  & entry     = m_bays.GetBay (slot, drive);
    string     directory = MountedImageState::GetDirectory (entry.path);
    bool       stillUsed = m_bays.IsDirectoryInAnotherBay (directory, slot, drive);



    if (m_hooks.watcher != nullptr && !directory.empty() && !stillUsed)
    {
        m_hooks.watcher->Unwatch (directory);
    }

    entry.sharedState.SetWatching (false);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EjectLostImage
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

void DiskImageStore::EjectLostImage (int slot, int drive)
{
    if (!DiskBayTable::IsValidBay (slot, drive))
    {
        return;
    }

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted        = false;
        entry.salvageOffered = false;
        entry.sharedState.Eject();
    }

    //  A file that vanished empties the drive the same as a user eject, and
    //  the door and its sound follow the same way.
    EmitBayChange (slot, drive, BayChange::Ejected);

    return;
}





