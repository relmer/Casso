#include "Pch.h"

#include "Seams/IntentReplyTracker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::ArePathsEqual
//
////////////////////////////////////////////////////////////////////////////////

bool IntentReplyTracker::ArePathsEqual (const std::string & a, const std::string & b)
{
    size_t  i = 0;



    if (a.size() != b.size())
    {
        return false;
    }

    for (i = 0; i < a.size(); i++)
    {
        char  left  = (a[i] == '/') ? '\\' : (char) tolower ((unsigned char) a[i]);
        char  right = (b[i] == '/') ? '\\' : (char) tolower ((unsigned char) b[i]);

        if (left != right)
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::NoteInsert
//
//  A newer request for the same drive replaces the older one: the drive holds
//  one disk, and only the last request can be the one that lands.
//
////////////////////////////////////////////////////////////////////////////////

void IntentReplyTracker::NoteInsert (int drive, const std::string & path, HWND replyTo)
{
    std::lock_guard<std::mutex>  guard (m_mutex);



    std::erase_if (m_inserts, [drive] (const Pending & pending) { return pending.drive == drive; });

    m_inserts.push_back (Pending { drive, path, replyTo });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::NoteReload
//
////////////////////////////////////////////////////////////////////////////////

void IntentReplyTracker::NoteReload (const std::string & path, HWND replyTo)
{
    std::lock_guard<std::mutex>  guard (m_mutex);



    std::erase_if (m_reloads, [&path] (const Pending & pending) { return ArePathsEqual (pending.path, path); });

    m_reloads.push_back (Pending { 0, path, replyTo });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::TryTakeInsert
//
////////////////////////////////////////////////////////////////////////////////

bool IntentReplyTracker::TryTakeInsert (int drive, const std::string & path, HWND & outReplyTo)
{
    std::lock_guard<std::mutex>  guard (m_mutex);



    outReplyTo = nullptr;

    for (auto it = m_inserts.begin(); it != m_inserts.end(); ++it)
    {
        if (it->drive == drive && ArePathsEqual (it->path, path))
        {
            outReplyTo = it->replyTo;
            m_inserts.erase (it);

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::TryTakeReload
//
////////////////////////////////////////////////////////////////////////////////

bool IntentReplyTracker::TryTakeReload (const std::string & path, HWND & outReplyTo)
{
    std::lock_guard<std::mutex>  guard (m_mutex);



    outReplyTo = nullptr;

    for (auto it = m_reloads.begin(); it != m_reloads.end(); ++it)
    {
        if (ArePathsEqual (it->path, path))
        {
            outReplyTo = it->replyTo;
            m_reloads.erase (it);

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::MakeInsertReply
//
////////////////////////////////////////////////////////////////////////////////

Win32IntentChannel::Reply IntentReplyTracker::MakeInsertReply (HRESULT mountResult, const std::string & reason)
{
    Win32IntentChannel::Reply  reply;



    if (SUCCEEDED (mountResult))
    {
        reply.kind = Win32IntentChannel::ReplyKind::InsertDone;
    }
    else
    {
        reply.kind = Win32IntentChannel::ReplyKind::InsertRefused;
        reply.text = reason;
    }

    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker::TryMakeReloadReply
//
//  Took the new contents: done, or a conflict when the guest's version had to
//  move aside first. Could not take them: refused, with the reason. A question
//  put to the user is no answer yet.
//
////////////////////////////////////////////////////////////////////////////////

bool IntentReplyTracker::TryMakeReloadReply (
    ChangeAction                  action,
    bool                          guestCopyPreserved,
    const std::string           & preservedPath,
    Win32IntentChannel::Reply   & outReply)
{
    outReply = Win32IntentChannel::Reply();

    switch (action)
    {
        case ChangeAction::Ignore:
        case ChangeAction::ReloadInPlace:
        case ChangeAction::Restart:
            if (guestCopyPreserved)
            {
                outReply.kind = Win32IntentChannel::ReplyKind::ReloadConflict;
                outReply.text = "The emulator had unsaved changes to this disk. They were kept in "
                              + preservedPath + ", and the disk now holds the new version.";
            }
            else
            {
                outReply.kind = Win32IntentChannel::ReplyKind::ReloadDone;
            }

            return true;

        case ChangeAction::Unusable:
            outReply.kind = Win32IntentChannel::ReplyKind::ReloadRefused;
            outReply.text = "The emulator could not use the new contents of this disk, and kept the version it had.";

            return true;

        case ChangeAction::Deleted:
            outReply.kind = Win32IntentChannel::ReplyKind::ReloadRefused;
            outReply.text = "The disk file is gone, so the emulator kept the version it had.";

            return true;

        case ChangeAction::Conflict:
            outReply.kind = Win32IntentChannel::ReplyKind::ReloadRefused;
            outReply.text = "The emulator had unsaved changes to this disk and could not save them "
                            "anywhere, so it kept the version it had.";

            return true;

        case ChangeAction::Ask:
        default:
            return false;
    }
}
