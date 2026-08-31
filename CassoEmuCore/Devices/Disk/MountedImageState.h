#pragma once

#include "Pch.h"
#include "ImageIdentity.h"
#include "ExternalChangePolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PendingChange
//
//  A change that has been noticed and not yet acted on.
//
//  FURTHER CHANGES UPDATE THIS RECORD RATHER THAN QUEUEING BEHIND IT. A disk
//  carrying more than one thing is built by more than one command -- assemble a
//  loader, assemble a program, place a data file -- and the developer means one
//  build. Acting per command restarts the machine repeatedly and lands later
//  writes while it is still booting from an earlier one.
//
////////////////////////////////////////////////////////////////////////////////

struct PendingChange
{
    bool          seen       = false;
    PickUpIntent  intent     = PickUpIntent::Unstated;

    //  When the most recent change arrived. The quiet period is measured from
    //  HERE and not from the first, which is what lets a burst settle once.
    int64_t       lastSeenMs = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState
//
//  What the emulator knows about one mounted image beyond its bytes.
//
//  ALL OF IT IS PER-MOUNT STATE AND THE RULES THAT GOVERN IT. Naming and
//  writing a preserved copy live in PreservedCopy, and deciding what to do
//  lives in ExternalChangePolicy, so this stays one job: remembering what was
//  read, what has been noticed since, and whether anybody has been told.
//
////////////////////////////////////////////////////////////////////////////////

class MountedImageState
{
public:

    //  How long a change must be quiet before it is acted on.
    //
    //  ONE SECOND, matching the spindown the drive already debounces by. A
    //  multi-command build settles once rather than once per command, and the
    //  constant is named so tuning it against a real build is one edit.
    static constexpr int64_t  kQuietPeriodMs = 1000;



    //  Set at mount, cleared at eject.
    void  Mount   (const ImageIdentity & identity);
    void  Eject   ();

    bool  IsMounted () const { return m_mounted; }

    //  What was recorded when this image was read, and replacing it after a
    //  write this emulator made -- which is what keeps its own commit from
    //  being reported back to it as somebody else's change.
    const ImageIdentity &  Identity   () const { return m_identity; }
    void                   SetIdentity (const ImageIdentity & identity) { m_identity = identity; }

    //  Whether the location could be watched at all. False is a state to
    //  degrade into rather than an error: the check before every write carries
    //  the guarantee, and a network share or a synchronizing folder is exactly
    //  the case that produces it.
    bool  IsWatching  () const       { return m_watching; }
    void  SetWatching (bool watching) { m_watching = watching; }

    //  A change arrived. Records it, or refreshes the one already standing.
    //
    //  A LATER STATED INTENT REPLACES AN EARLIER ONE, because the last writer
    //  is the one whose bytes are on disk.
    void  NoteChange (int64_t nowMs, PickUpIntent intent);

    //  Whether a noticed change has settled and may be acted on.
    bool  IsSettled  (int64_t nowMs) const;

    const PendingChange &  Pending () const { return m_pending; }

    //  The change has been dealt with.
    void  ClearPending ();

    //  Whether a report is standing that the user has not acted on.
    //
    //  IT OUTLIVES THE PENDING CHANGE. Acting on a change consumes the pending
    //  record, but the report stays up until dismissed, absorbs later changes
    //  while it stands, and keeps the restart reachable. Without a field for it
    //  the state would have to live in the shell, which no test can reach.
    bool  IsReportStanding () const        { return m_reportStanding; }
    void  SetReportStanding (bool standing) { m_reportStanding = standing; }

    //  Whether two spellings of a path name the same file.
    //
    //  HERE RATHER THAN IN THE SHELL because comparing them is a rule, not a
    //  presentation detail: case, separators and trailing slashes all differ
    //  between what a watcher reports and what a mount recorded.
    static bool  SamePath (const std::string & left, const std::string & right);

    //  The directory a path sits in, which is what gets watched.
    static std::string  DirectoryOf (const std::string & path);

private:
    ImageIdentity  m_identity;
    PendingChange  m_pending;
    bool           m_mounted        = false;
    bool           m_watching       = false;
    bool           m_reportStanding = false;
};
