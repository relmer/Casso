#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"
#include "ChangePrompt.h"
#include "IImageWatcher.h"

class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreReload
//
//  The disk store's response to a file changed outside Casso: the inbox
//  of noticed changes, the reload or the question, and the directory
//  watches. One thread uses it at a time, the thread with the store's
//  token, except NoteExternalChange, which any thread may call.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreReload
{
public:
    using Entry = DiskBayEntry;

    explicit DiskStoreReload (DiskImageStore & store) : m_store (store) {}

    //  DiskImageStore's NoteExternalChange, ApplyPendingReload and
    //  ResolvePendingChange.
    void           NoteExternalChange   (const string & path, ExternalChangeIntent intent);
    void           ApplyPendingReload   ();
    void           ResolvePendingChange (int slot, int drive, ChangeAction chosen, const string & savePath);

    //  Moves a mounted bay onto a different file without disturbing the disk
    //  in it. The image, and everything the guest can observe, is untouched;
    //  what changes is which file the bay reads and writes from here on.
    //
    //  THIS IS A SAVE-AS, NOT A MOUNT. Nothing is loaded, so a guest mid-write
    //  sees nothing at all.
    HRESULT       RepointBayToFile  (int slot, int drive, const string & newPath);

    //  Everything one bay's settled change leads to.
    void           ApplyPendingReloadToBay (int slot, int drive);

    //  ApplyPendingReloadToBay's steps: whether the noticed change is due,
    //  reading and trial-loading the file, moving the guest's version to a
    //  separate file in a conflict, and putting the question.
    //  ResolvePendingChange's answers: to a conflict whose copy could not be
    //  written, and to a question about a file that is gone.
    void           ResolveConflictAnswer        (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath);
    void           ResolveLostFileAnswer        (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath);

    bool           IsReloadDue                  (Entry & entry, ExternalChangeIntent & outIntent);
    bool           ReadChangedImage             (const Entry & entry, vector<Byte> & outBytes) const;
    bool           PreserveGuestCopyForConflict (int slot, int drive, Entry & entry, string & outSavePath);
    void           AskAboutChange               (int slot, int drive, Entry & entry, ChangeAction action);

    //  Moves the inbox NoteExternalChange fills into the bays, on the owning
    //  thread.
    void           DrainChangeInbox ();

    //  Carries out what was decided. Split from deciding so the decision has
    //  one shape whether it came from the policy or from the user.
    //
    //  `author` COMES FROM THE CALLER BECAUSE ONLY THE CALLER STILL KNOWS. A
    //  bay's pending record holds the newest change and nothing older, so by
    //  the time this runs it can describe a write that landed after the one
    //  being acted on. Reading it here named whoever wrote last.
    void           CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                         const vector<Byte> & bytes, ChangeAuthor author);

    //  Empties a bay whose file is gone, WITHOUT trying to flush it. The
    //  ordinary eject flushes first, which here would either fail or write the
    //  disk back to a path the user has just been told no longer exists.
    void           EjectLostImage (int slot, int drive);

    //  Replaces a mounted image's contents WITHOUT flushing what it held.
    //
    //  A FLUSH HERE WOULD WRITE THE OLD DISK OVER THE NEW FILE, which is the
    //  precise loss this feature exists to prevent. The new bytes are loaded
    //  into a fresh image first, so a load that fails leaves the mounted disk
    //  exactly as it was.
    HRESULT        MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes);

    //  Registers and drops the watch on a bay's directory.
    //
    //  A DIRECTORY IS DROPPED ONLY WHEN NO OTHER BAY STILL NEEDS IT. Two disks
    //  out of one folder are ordinary, and ejecting the first must not blind
    //  the second.
    void           BeginWatching (int slot, int drive);
    void           EndWatching   (int slot, int drive);

private:
    DiskImageStore &  m_store;

    //  Changes noted from any thread, keyed by path, with only the latest per
    //  path, so the inbox is bounded by the distinct paths noted. The mutex
    //  guards the inbox alone: NoteExternalChange reads no Entry, and
    //  ApplyPendingReload moves the inbox into the bays on the thread that has
    //  the store's token.
    struct NotedChange
    {
        int64_t               atMs   = 0;
        ExternalChangeIntent  intent = ExternalChangeIntent::Unstated;
    };

    mutable std::mutex                   m_pendingMutex;
    std::map<std::string, NotedChange>   m_changeInbox;

    //  The owning thread's side of the swap: the inbox is swapped into it,
    //  and it is cleared after the drain, so an idle drain allocates nothing.
    std::map<std::string, NotedChange>   m_drainedChanges;
};
