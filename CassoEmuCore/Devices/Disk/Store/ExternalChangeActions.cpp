#include "Pch.h"

#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/ExternalChangePolicy.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeActions::ResolvePendingChange
//
//  The answer to a question this store asked.
//
//  THE BYTES ARE READ AGAIN HERE RATHER THAN KEPT FROM THE QUESTION. The user
//  may have run two more builds while the prompt sat on the screen, and the
//  version they mean is the one they just built -- not the one that was on disk
//  when the question was composed.
//
////////////////////////////////////////////////////////////////////////////////

void ExternalChangeActions::ResolvePendingChange (
    int             slot,
    int             drive,
    ChangeAction    chosen,
    const string  & savePath)
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

                hr = m_repoint.RepointBayToFile (slot, drive, savePath);
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

                m_lifecycle.Eject (slot, drive);
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

                hr = m_repoint.RepointBayToFile (slot, drive, savePath);
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

            m_lifecycle.EjectLostImage (slot, drive);

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
//  ExternalChangeActions::CarryOutChangeAction
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

void ExternalChangeActions::CarryOutChangeAction (
    int                    slot,
    int                    drive,
    ChangeAction           action,
    const vector<Byte>  &  bytes,
    ChangeAuthor           author)
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
        hr = m_lifecycle.MountExternallyModifiedDisk (slot, drive, bytes);

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
        hr = m_repoint.RepointBayToFile (slot, drive, preservedPath);
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





