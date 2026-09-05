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
    bool                  seen   = false;
    ExternalChangeIntent  intent = ExternalChangeIntent::Unstated;

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
    const ImageIdentity &  GetIdentity () const { return m_identity; }
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
    void  NoteChange (int64_t nowMs, ExternalChangeIntent intent);

    //  Whether a noticed change has settled and may be acted on.
    bool  IsSettled  (int64_t nowMs) const;

    const PendingChange &  GetPending () const { return m_pending; }

    //  The change has been dealt with.
    void  ClearPending ();

    //  Whether a question has been put to the user and not yet answered.
    //
    //  WITHOUT IT THE QUESTION WOULD BE ASKED AGAIN EVERY IDLE TICK. Asking
    //  runs on the thread that owns disk writes and answering happens on the
    //  one that owns the screen, so the change necessarily stays pending while
    //  the user reads it -- and a pending change is exactly what the pick-up
    //  path looks for.
    bool  IsAskOutstanding  () const          { return m_askOutstanding; }
    void  SetAskOutstanding (bool outstanding) { m_askOutstanding = outstanding; }

    //  Which question was put to the user, so the answer can be read in the
    //  terms it was asked in.
    //
    //  "SAVE A COPY" MEANS DIFFERENT THINGS IN DIFFERENT QUESTIONS. Answering
    //  it about a file that has gone saves the disk and empties the drive;
    //  there is no other question where it would do that, and an answer read
    //  without knowing what was asked is how the wrong one gets carried out.
    ChangeAction  GetAskedAction () const           { return m_askedAction; }
    void          SetAskedAction (ChangeAction action) { m_askedAction = action; }

    //  The name the guest's version goes under, and whether it is there yet.
    //
    //  RESERVED BEFORE IT IS WRITTEN, and that separation is the point. A
    //  question tells the user the name it WOULD take, and the flush path may
    //  then write the copy while that question is still on screen. If the two
    //  worked it out independently they produced different names, and the
    //  dialog ended up offering a file that was never created -- measured,
    //  five seconds apart. Reserving it once means whoever writes it writes
    //  the name the user was already shown.
    //
    //  HERE RATHER THAN ON THE BAY so that mounting and ejecting clear it
    //  along with everything else per-mount. Held on the bay it was the one
    //  field the two of them did not reach, and every reservation bug this
    //  subsystem has had was that exception expressing itself.
    const std::string &  GetPreservedPath () const                   { return m_preservedPath; }
    void                 SetPreservedPath (const std::string & path) { m_preservedPath = path; }

    bool  IsPreservedWritten  () const         { return m_preservedWritten; }
    void  SetPreservedWritten (bool written)    { m_preservedWritten = written; }

    //  A copy under this name is no longer this bay's concern.
    void  ClearPreserved ();

    //  Gives back a preserved name that was reserved and never used.
    //
    //  A NAME IS RESERVED WHEN A QUESTION IS PUT, so that whoever writes the
    //  copy writes the name the user was shown. A question that ends without a
    //  copy leaves it held, and `SaveLoadedImage` uses a name it is given as it
    //  stands -- so the next copy, whenever it came, went out under the old
    //  question's timestamp.
    //
    //  A COPY THAT EXISTS KEEPS ITS NAME. `m_preservedWritten` says a file is
    //  there, and forgetting where would write the same disk out a second time
    //  beside it.
    void  ReleaseUnwrittenReservation ();

    //  Whether two spellings of a path name the same file.
    //
    //  HERE RATHER THAN IN THE SHELL because comparing them is a rule, not a
    //  presentation detail: case, separators and trailing slashes all differ
    //  between what a watcher reports and what a mount recorded.
    static bool  IsSamePath (const std::string & left, const std::string & right);

    //  The directory a path sits in, which is what gets watched.
    static std::string  GetDirectory (const std::string & path);

private:
    ImageIdentity  m_identity;
    PendingChange  m_pending;
    bool           m_mounted          = false;
    bool           m_watching         = false;
    bool           m_askOutstanding   = false;
    ChangeAction   m_askedAction      = ChangeAction::Ignore;
    std::string    m_preservedPath;
    bool           m_preservedWritten = false;
};
