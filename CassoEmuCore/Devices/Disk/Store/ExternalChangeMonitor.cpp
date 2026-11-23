#include "Pch.h"

#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeMonitor::GetNowMs
//
//  Milliseconds now.
//
//  INJECTED SO A TEST NEED NOT WAIT. The quiet period is a second, and a suite
//  that spent one proving every coalescing rule would be a suite nobody runs.
//
////////////////////////////////////////////////////////////////////////////////

int64_t ExternalChangeMonitor::GetNowMs() const
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
//  ExternalChangeMonitor::NoteExternalChange
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

void ExternalChangeMonitor::NoteExternalChange (const string & path, ExternalChangeIntent intent, bool fromCassoExplorer)
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
//  ExternalChangeMonitor::ApplyPendingReload
//
//  Act on whatever has settled, on the thread that owns disk writes.
//
//  CALLED OFTEN AND DOES NOTHING ALMOST EVERY TIME. It is pumped from the
//  controller's idle callback and from motor spindown, so the common path is a
//  loop over sixteen bays that finds nothing pending and returns.
//
////////////////////////////////////////////////////////////////////////////////

void ExternalChangeMonitor::ApplyPendingReload()
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
//  ExternalChangeMonitor::ApplyPendingReloadToBay
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

void ExternalChangeMonitor::ApplyPendingReloadToBay (int slot, int drive)
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

    m_actions.CarryOutChangeAction (slot, drive, action, bytes, author);

    return;
}





