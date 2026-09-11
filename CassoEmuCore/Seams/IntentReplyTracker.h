#pragma once

#include "Pch.h"

#include "Devices/Disk/ExternalChangePolicy.h"
#include "Seams/Win32IntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTracker
//
//  Which tool is waiting for an answer about which image, from the moment the
//  request arrives to the moment the emulator knows the outcome.
//
//  A MOUNT AND A RELOAD BOTH FINISH SOMEWHERE ELSE. The request arrives on the
//  UI thread; the mount completes later, posted back from the CPU thread, and a
//  reload is decided on the CPU thread at a moment with nothing in flight. So
//  the sender's window is held here until the outcome is known, and the words
//  of the answer are composed here, where a test can read them.
//
//  A HAND-OFF WITH NO WINDOW IS STILL TRACKED. A disk dropped from Explorer has
//  nobody to answer, but its folder is recorded when the mount succeeds, and
//  recording only a mount that worked needs the same bookkeeping.
//
//  Thread-safe: requests are noted on the UI thread and reload decisions are
//  taken on the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class IntentReplyTracker
{
public:
    void  NoteInsert (int drive, const std::string & path, HWND replyTo);
    void  NoteReload (const std::string & path, HWND replyTo);

    //  True when a hand-off was noted for this drive and path; the window
    //  comes back, null when nobody is waiting. The note is consumed either way.
    bool  TryTakeInsert (int drive, const std::string & path, HWND & outReplyTo);
    bool  TryTakeReload (const std::string & path, HWND & outReplyTo);

    //  What a finished mount answers.
    static Win32IntentChannel::Reply  MakeInsertReply (HRESULT mountResult, const std::string & reason);

    //  What a reload decision answers. `guestCopyPreserved` is whether the
    //  guest's unsaved writes were moved to `preservedPath` before the new
    //  contents were taken. False when the decision asks the user, which
    //  answers the request with the user's own later choice rather than now.
    static bool  TryMakeReloadReply (ChangeAction                  action,
                                     bool                          guestCopyPreserved,
                                     const std::string           & preservedPath,
                                     Win32IntentChannel::Reply   & outReply);

    //  Two paths name one image when they agree case-insensitively with the
    //  separators unified, as the store itself matches them.
    static bool  ArePathsEqual (const std::string & a, const std::string & b);

private:
    struct Pending
    {
        int          drive   = 0;
        std::string  path;
        HWND         replyTo = nullptr;
    };

    std::mutex            m_mutex;
    std::vector<Pending>  m_inserts;
    std::vector<Pending>  m_reloads;
};
