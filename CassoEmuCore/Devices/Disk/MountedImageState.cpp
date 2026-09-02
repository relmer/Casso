#include "Pch.h"

#include "MountedImageState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::Mount
//
////////////////////////////////////////////////////////////////////////////////

void MountedImageState::Mount (const ImageIdentity & identity)
{
    m_identity       = identity;
    m_pending        = PendingChange();
    m_mounted        = true;
    m_watching       = false;
    m_reportStanding = false;
    m_askOutstanding = false;
    m_askedAction    = ChangeAction::Ignore;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::Eject
//
//  Everything goes, including any standing report. A report about a disk that
//  is no longer in the drive has nothing to offer and no image to name.
//
////////////////////////////////////////////////////////////////////////////////

void MountedImageState::Eject()
{
    m_identity       = ImageIdentity();
    m_pending        = PendingChange();
    m_mounted        = false;
    m_watching       = false;
    m_reportStanding = false;
    m_askOutstanding = false;
    m_askedAction    = ChangeAction::Ignore;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::NoteChange
//
//  A change arrived, or another one did.
//
//  THE TIMER RESTARTS RATHER THAN RUNNING ON. Measuring the quiet period from
//  the FIRST change would fire in the middle of a three-command build, which is
//  the case coalescing exists for. Measuring from the last means the build
//  settles once, after it stops writing.
//
//  A CHANGE ARRIVING MID-APPLY IS NOT DROPPED. Nothing here knows or cares
//  whether something is currently acting on an earlier change: the record is
//  refreshed either way, and an apply that has already read the file will find
//  it still pending when it finishes.
//
////////////////////////////////////////////////////////////////////////////////

void MountedImageState::NoteChange (int64_t nowMs, PickUpIntent intent)
{
    m_pending.seen       = true;
    m_pending.lastSeenMs = nowMs;

    //  A stated intent replaces whatever stood before, including replacing a
    //  stated one with Unstated: the last writer is the one whose bytes are on
    //  the disk, and a stale intent describes contents that are gone.
    m_pending.intent = intent;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::IsSettled
//
////////////////////////////////////////////////////////////////////////////////

bool MountedImageState::IsSettled (int64_t nowMs) const
{
    return m_pending.seen
        && (nowMs - m_pending.lastSeenMs) >= kQuietPeriodMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::ClearPending
//
////////////////////////////////////////////////////////////////////////////////

void MountedImageState::ClearPending()
{
    m_pending = PendingChange();

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::SamePath
//
//  Whether two spellings name one file.
//
//  A WATCHER REPORTS A PATH THE WAY THE PLATFORM SPELLS IT and a mount recorded
//  the way a user typed it, so the two rarely match as strings. Separators and
//  case are the two that differ in practice on this platform; both are
//  normalized here rather than at each call site, so a bay cannot be matched
//  one way in one place and another way somewhere else.
//
////////////////////////////////////////////////////////////////////////////////

bool MountedImageState::SamePath (const std::string & left, const std::string & right)
{
    std::string  a = left;
    std::string  b = right;
    size_t       i = 0;



    for (i = 0; i < a.size(); i++)
    {
        a[i] = (a[i] == '/') ? '\\' : (char) tolower ((unsigned char) a[i]);
    }

    for (i = 0; i < b.size(); i++)
    {
        b[i] = (b[i] == '/') ? '\\' : (char) tolower ((unsigned char) b[i]);
    }

    return a == b;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedImageState::DirectoryOf
//
//  The directory holding a path.
//
//  A PATH WITH NO SEPARATOR YIELDS AN EMPTY STRING rather than a dot. The
//  caller watches what comes back, and watching "." would watch the process's
//  working directory, which is not where the image is.
//
////////////////////////////////////////////////////////////////////////////////

std::string MountedImageState::DirectoryOf (const std::string & path)
{
    size_t       cut       = path.find_last_of ("/\\");
    std::string  directory;



    if (cut != std::string::npos)
    {
        directory = path.substr (0, cut);
    }

    return directory;
}
